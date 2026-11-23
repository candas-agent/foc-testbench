# Pitfalls

Traps that cost time on this bench, with the way around them. One line each; details in the test log.

## Flashing and debugging
- **GD32F130: pyOCD `-t cortex_m` does not program the flash and reports no error.** Flash with the OpenOCD of
  `tool-openocd-gd32`: `-f interface/stlink.cfg -c "set CPUTAPID 0" -f target/stm32f1x.cfg -c "program {elf} verify reset exit"`.
- **SWD lost** (GD32F130, no NRST on the header): a firmware wrote a wrong alternate function on PA13 / PA14. Recovery:
  `tools/catch_halt.py` while only the board 3.3 V is replugged (the ST-Link stays on USB), then OpenOCD `init`,
  `halt`, `stm32f1x mass_erase 0` without reset. BOOT0 (pin 44) as last resort.
- **GD32E230K8 board (#39): SWD does not answer** (original firmware probably disables it), and `catch_halt.py`
  cannot work on the ST-Link 3.3 V: plugging the board 3.3 V resets the ST-Link (inrush, USB disconnect sound),
  so the probe is never ready when the MCU starts. Use a separate 3.3 V supply, NRST (QFN32 pin 4) or BOOT0 (pin 31).
- Only one tool on the ST-Link at a time (pyOCD, OpenOCD or an upload).
- Disconnect the ST-Link 3.3 V when the board is supplied.
- GD32F130 bus voltage is read at start-up only: when the 26 V arrives with the board already on, reset by software
  (`pyocd reset -t cortex_m -m sw`) to read it.
- With R_ds_on current sense, the phase currents read while the motor is disabled are meaningless (tens of A).

## Building
- **PowerShell 5.1 loses the quotes** of `gh workflow run ... -f envs='["a","b"]'`: start cloud builds from Bash.
- Any change of `platformio.ini` wipes every local `.pio/build`: build in the cloud.
- The GD32F130 linker script (`ld/gd32f130_fastloop.ld`) is not tracked by the build: delete
  `.pio/build/<env>/firmware.elf` after changing it. Link fails if `__fast_end` > 0x08008000 (32 KB without wait states).
- **No `lib_archive`** globally: it overrides `"libArchive": false` of SimpleFOC and testbench, and the weak hardware
  functions get replaced by the empty generic ones.
- **LTO (`-flto`) breaks SimpleFOC**: weak hardware functions dropped (STM32 current sense).
- GD32: the PlatformIO library finder pulls FreeRTOS: `lib_ignore` it.
- Cloud builds give the same machine code as local builds (only 3 newlib path strings differ).

## SimpleFOC and cores
- GD32 core `GD_PIN_AF_GET` (mask 0xFF overlaps the channel field): pinMode / analogRead through the pinmap on a pin
  with a channel != 0 writes a wrong AF on the next pins. Never use it.
- GD32 core `micros()` can go back by up to 1 ms: `gd32_micros.c` on the bench.
- GD32F130 `analogRead` / `_readRegularADCVoltage` ≈ 0.8 ms per read (slow flash): never every loop.
- STM32F103 on hoverboards runs at 64 MHz (HSI, no crystal), not 72.
- STM32F1 + SimpleFOC dev: no `SIMPLEFOC_STM32_ADC_INTERRUPT` (currents freeze, simplefoc#594).
- `_sin` / `_cos` of a negative angle return 0 on STM32 (dev): open loop with a negative angle applies no voltage.
- In trapezoidal modes the R_ds_on reading of the floating phase means nothing: the bench overcurrent cut trips.

## GitHub CI (Arduino-FOC forks)
- An example folder must have the sketch's name, otherwise arduino-lint breaks every workflow.
- The GD32 Arduino core package has no checksum: GD32 CI uses PlatformIO.
- Teensy and ESP32 jobs fail on upstream dev too (Python 3.9 missing on the runners): not our change.

## Scripts on this PC
- `python` is not on the PATH: use `.venv/Scripts/python.exe` (bench) or `~/.platformio/penv/Scripts/python.exe`.
- `python -I` ignores `PYTHONIOENCODING`: call `sys.stdout.reconfigure(encoding="utf-8")` before printing accents.
