# Tools index

One line per tool. Check here before doing a multi-step procedure by hand; add a line when you add a tool.
⚡ = the motor moves or is energized: only with the owner present (CLAUDE.md safety rules).

## Board access (no motor)
- `bench_cmd.py`: send bench commands, print the replies (serial on ESC1, RTT on GD32).
- `board_mem.py`: read the bench variables from memory with pyOCD, without stopping the firmware.
- `live.py`: live reading of the dbg structure to CSV.
- `pc_sample.py`: statistical profiler (DWT_PCSR samples, share per function), CPU not stopped.
- `catch_halt.py`: recovery when a firmware disabled SWD (attach and halt as soon as it answers).
- `gd32_drv_regs.py`: GD32F130 TIMER0/GPIO registers set by the driver, to compare two firmwares.
- `example_swd.py`: GD32 split example (env gd32_example_split) over SWD: init results.

## Equivalence ("same behaviour" checks)
- `equiv_read.py`: results of the on-target equivalence firmwares (envs `equiv_*`).
- `equiv_audit_read.py`: on-target audit of dev functions (env equiv_audit_dev_f103).
- `equiv_foc_compare.py`: loopFOC scenario traces dev vs PR, bit for bit.
- On a PC: `normalize_equiv.py`, `pid_equiv.py`, `pid_isset_equiv.py` (all 2^32 floats), `setpwm_equiv.py`.

## Measurements ⚡
- `perf_run.py` ⚡: loop cost of a change while running (XP profile, DWT cycles).
- `loop_time.py` ⚡: loop time in open loop on the GD32 board.
- `ol_run.py` ⚡: short open loop run with live current monitoring and software cut.
- `campaign.py` ⚡: open loop campaign over modes / speeds / compensation values.
- `lowspeed_campaign.py` ⚡: low speed comparison (velocity estimates, position ramp, dead-time A/B).
- `trace_run.py` ⚡: time trace of one velocity run (for `plot_trace.py`).
- `reenable_test.py` ⚡: current filters after a pause / re-enable.
- `ff_test.py` ⚡: voltage feed-forward modes (XK), velocity steps.
- `zero_torque.py` ⚡: zero torque test (wheel turned by hand).
- `hall_calib.py` ⚡: hall edge calibration (rotor dragged slowly).
- `offset_trim.py` ⚡ (0 V) / `offset_ripple.py` ⚡: current sense offsets.
- `sampling_sweep.py` ⚡: GD32 driver sampling point sweep.
- `trap_test.py` ⚡: trapezoidal 120 modulation check (GD32).
- `bug_repro.py` ⚡: reproduces the dev bugs of docs/equiv-tests.md (F103).
- GD32 PhaseState: `phase_state.py` ⚡ (0 V), `phase_hilo.py` (phase states written in firmware), `phase_brake.py` ⚡ (under power).

## Analysis (PC only)
- `plot_trace.py`, `plot_ab.py`: figures from recordings.
- `compare_currents.py`, `fold_compare.py`: phase current waveform comparisons.
- `stall_map.py`, `stall_sectors.py`: low speed stalls from the `lowspeed_campaign.py --edges` log.

## Pin scan (unknown boards, simplefoc-work #39)
- `pin_scan.py` (supply disconnected, 3.3 V only), `scope_pin.py` (Rigol frequency → MCU pin).
