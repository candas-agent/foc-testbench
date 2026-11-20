"""
Read the test bench variables from the board memory with pyOCD, through the ST-Link, without stopping the program
(only one tool can use the ST-Link at a time).

The symbol addresses come from the firmware ELF (.pio/build/esc1/firmware.elf). The struct layouts below mirror
lib/testbench/src/bench.h: update both together (DBG_MAGIC is checked).
"""
import struct
import time

from elftools.elf.elffile import ELFFile  # installed with pyocd

DBG_MAGIC = 0xF0CDB005
DBG_FIELDS = ["magic", "ia", "ib", "ic", "comp_a", "comp_b", "comp_c", "iq", "id", "uq", "ud",
              "electrical_angle", "velocity", "loop_us", "obs_angle", "angle_err", "fault", "enabled", "vbus", "hall", "sensor_angle", "hall_count"]
DBG_FORMAT = "<I15fIIfIfi"
SAMPLE_FIELDS = ["t_us", "ia", "comp_a", "angle_err"]
SAMPLE_FORMAT = "<I3f"


def elf_symbols(elf_path, names, optional=()):
    """Addresses and sizes of the given symbols (name -> (address, size)). Optional symbols may be missing."""
    found = {}
    with open(elf_path, "rb") as f:
        elf = ELFFile(f)
        symtab = elf.get_section_by_name(".symtab")
        if symtab is None:
            raise RuntimeError("no symbol table in the ELF")
        for sym in symtab.iter_symbols():
            if (sym.name in names or sym.name in optional) and sym["st_info"]["type"] == "STT_OBJECT":
                found[sym.name] = (sym["st_value"], sym["st_size"])
    missing = set(names) - set(found)
    if missing:
        raise RuntimeError(f"symbols not found in the ELF: {sorted(missing)}")
    return found


class BoardMemory:
    """Usage:
        with BoardMemory(".pio/build/esc1/firmware.elf") as mem:
            print(mem.read_dbg())
    """

    def __init__(self, elf_path, probe_id=None, frequency=4_000_000):
        # _SEGGER_RTT: RTT control block, only in the firmware built with BENCH_USE_RTT (GD32 board)
        self.sym = elf_symbols(elf_path, ["dbg", "rec_buf", "rec_n", "rec_running"], optional=["_SEGGER_RTT", "hall_edges", "hall_edge_n"])
        expected = struct.calcsize(DBG_FORMAT)
        if self.sym["dbg"][1] != expected:
            raise RuntimeError(f"dbg size in the ELF is {self.sym['dbg'][1]} bytes, expected {expected}: "
                               "update DBG_FORMAT/DBG_FIELDS in board_mem.py")
        # recorder size from the ELF (RECORD_SAMPLES depends on the board)
        self.rec_capacity = self.sym["rec_buf"][1] // struct.calcsize(SAMPLE_FORMAT)
        # hall edge log (firmware with hall sensors): ring buffer size from the ELF
        self.hall_edge_capacity = self.sym["hall_edges"][1] // 8 if "hall_edges" in self.sym else 0
        self.hall_edge_next = None   # index of the next edge to read (None: start from the current one)
        self.probe_id = probe_id
        self.frequency = frequency
        self.session = None
        self.target = None

    def __enter__(self):
        from pyocd.core.helpers import ConnectHelper
        # attach: connect without halting or resetting the running program
        # cortex_m: generic target, enough to read/write memory (no CMSIS pack needed)
        self.session = ConnectHelper.session_with_chosen_probe(
            unique_id=self.probe_id, connect_mode="attach", target_override="cortex_m",
            frequency=self.frequency, options={"resume_on_disconnect": True})
        if self.session is None:
            raise RuntimeError("no debug probe found (ST-Link busy? close other debug tools)")
        self.session.open()
        self.target = self.session.board.target
        magic = self.read_dbg()["magic"]
        if magic != DBG_MAGIC:
            raise RuntimeError(f"dbg.magic is {magic:#x}, expected {DBG_MAGIC:#x}: firmware and tools out of sync, "
                               "or the program is not running")
        return self

    def __exit__(self, *exc):
        if self.session is not None:
            self.session.close()

    def _read(self, addr, size):
        return bytes(self.target.read_memory_block8(addr, size))

    def read_dbg(self):
        addr, size = self.sym["dbg"]
        values = struct.unpack(DBG_FORMAT, self._read(addr, size))
        return dict(zip(DBG_FIELDS, values))

    def read_u32(self, name):
        addr, _ = self.sym[name]
        return struct.unpack("<i", self._read(addr, 4))[0]

    def read_hall_edges(self, from_start=False):
        """New hall edges since the last call, as a list of (t_us, count): t_us = board micros() in the interrupt
        (wraps after 71 min), count = hall sectors since boot (+1 / -1 per edge gives the direction).
        The first call only sets the starting point (returns []) unless from_start. Raises if the ring buffer
        overflowed between two calls (edges lost: read more often)."""
        if not self.hall_edge_capacity:
            raise RuntimeError("no hall_edges in the ELF (firmware without hall sensors or too old)")
        n = struct.unpack("<I", self._read(self.sym["hall_edge_n"][0], 4))[0]
        cap = self.hall_edge_capacity
        if self.hall_edge_next is None:
            self.hall_edge_next = max(0, n - cap) if from_start else n
        start = self.hall_edge_next
        if n - start > cap:
            raise RuntimeError(f"hall edge buffer overflow: {n - start - cap} edges lost")
        if n == start:
            return []
        addr = self.sym["hall_edges"][0]
        a, b = start % cap, n % cap
        if a < b:
            raw = self._read(addr + 8 * a, 8 * (b - a))
        else:  # wraps around the end of the buffer
            raw = self._read(addr + 8 * a, 8 * (cap - a)) + (self._read(addr, 8 * b) if b else b"")
        self.hall_edge_next = n
        # the newest entry may be written while we read: the firmware publishes hall_edge_n after the entry,
        # so entries below the n we read are complete
        return [struct.unpack_from("<Ii", raw, 8 * k) for k in range(n - start)]

    def recording_done(self):
        return self.read_u32("rec_running") == 0 and self.read_u32("rec_n") > 0

    def read_recording(self):
        """Samples of the last recording as a list of dicts."""
        n = self.read_u32("rec_n")
        addr, _ = self.sym["rec_buf"]
        size = struct.calcsize(SAMPLE_FORMAT)
        raw = self._read(addr, n * size)
        return [dict(zip(SAMPLE_FIELDS, struct.unpack_from(SAMPLE_FORMAT, raw, k * size))) for k in range(n)]


class RttPort:
    """SEGGER RTT channel 0 through the debug probe, with the subset of the pyserial API used by the tools
    (write, readline, read, reset_input_buffer, close). Shares the pyOCD session of a BoardMemory.

        with BoardMemory(elf) as mem:
            port = RttPort(mem)
            port.write(b"X\\n")
            print(port.readline())
    """

    def __init__(self, mem, channel=0, timeout=0.1):
        from pyocd.debug.rtt import RTTControlBlock
        if "_SEGGER_RTT" not in mem.sym:
            raise RuntimeError("no _SEGGER_RTT in the ELF: this firmware does not use RTT (use the serial port)")
        # control block exactly at the symbol address (no RAM search, the generic cortex_m target has no memory map)
        self.cb = RTTControlBlock.from_target(mem.target, address=mem.sym["_SEGGER_RTT"][0])
        self.cb.start()
        self.up = self.cb.up_channels[channel]
        self.down = self.cb.down_channels[channel]
        self.timeout = timeout
        self._buf = b""

    def _poll(self):
        data = self.up.read()
        if data:
            self._buf += data

    @property
    def in_waiting(self):
        self._poll()
        return len(self._buf)

    def write(self, data):
        # the down buffer is small: write in pieces until everything is accepted
        sent, t0 = 0, time.time()
        while sent < len(data):
            n = self.down.write(data[sent:])
            sent += n
            if n == 0:
                if time.time() - t0 > 1.0:
                    raise TimeoutError("RTT down buffer full (firmware not reading commands?)")
                time.sleep(0.002)
        return sent

    def readline(self):
        """One line including the newline, or b"" if none is complete within the timeout."""
        t0 = time.time()
        while True:
            self._poll()
            k = self._buf.find(b"\n")
            if k >= 0:
                line, self._buf = self._buf[:k + 1], self._buf[k + 1:]
                return line
            if time.time() - t0 > self.timeout:
                return b""
            time.sleep(0.002)

    def read(self, size=1):
        self._poll()
        data, self._buf = self._buf[:size], self._buf[size:]
        return data

    def reset_input_buffer(self):
        self._poll()
        self._buf = b""

    def close(self):
        pass
