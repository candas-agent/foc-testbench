"""
Read the test bench variables from the board memory with pyOCD, through the ST-Link, without stopping the program
(only one tool can use the ST-Link at a time).

The symbol addresses come from the firmware ELF (.pio/build/esc1/firmware.elf). The struct layouts below mirror
lib/testbench/src/bench.h: update both together (DBG_MAGIC is checked).
"""
import struct

from elftools.elf.elffile import ELFFile  # installed with pyocd

DBG_MAGIC = 0xF0CDB002
DBG_FIELDS = ["magic", "ia", "ib", "ic", "comp_a", "comp_b", "comp_c", "iq", "id", "uq", "ud",
              "electrical_angle", "velocity", "loop_us", "obs_angle", "angle_err", "fault", "enabled"]
DBG_FORMAT = "<I15fII"
SAMPLE_FIELDS = ["t_us", "ia", "comp_a", "angle_err"]
SAMPLE_FORMAT = "<I3f"


def elf_symbols(elf_path, names):
    """Addresses and sizes of the given symbols (name -> (address, size))."""
    found = {}
    with open(elf_path, "rb") as f:
        elf = ELFFile(f)
        symtab = elf.get_section_by_name(".symtab")
        if symtab is None:
            raise RuntimeError("no symbol table in the ELF")
        for sym in symtab.iter_symbols():
            if sym.name in names and sym["st_info"]["type"] == "STT_OBJECT":
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
        self.sym = elf_symbols(elf_path, ["dbg", "rec_buf", "rec_n", "rec_running"])
        expected = struct.calcsize(DBG_FORMAT)
        if self.sym["dbg"][1] != expected:
            raise RuntimeError(f"dbg size in the ELF is {self.sym['dbg'][1]} bytes, expected {expected}: "
                               "update DBG_FORMAT/DBG_FIELDS in board_mem.py")
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

    def recording_done(self):
        return self.read_u32("rec_running") == 0 and self.read_u32("rec_n") > 0

    def read_recording(self):
        """Samples of the last recording as a list of dicts."""
        n = self.read_u32("rec_n")
        addr, _ = self.sym["rec_buf"]
        size = struct.calcsize(SAMPLE_FORMAT)
        raw = self._read(addr, n * size)
        return [dict(zip(SAMPLE_FIELDS, struct.unpack_from(SAMPLE_FORMAT, raw, k * size))) for k in range(n)]
