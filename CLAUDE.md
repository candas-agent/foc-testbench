# FOC test bench - instructions for Claude

Test bench for the PWM dead-time compensation added to SimpleFOC (fork candas-agent/Arduino-FOC,
branch feat/deadtime-compensation) and for the MXLEMMING observer fixes (fork candas-agent/Arduino-FOC-drivers,
branch fix/mxlemming-checks). Hardware: B-G431B-ESC1 + hoverboard motor (very low phase resistance, ~0.17 Ohm).

## Safety rules
- Never flash the board or start/enable the motor without the user's explicit confirmation in the current session.
- Never raise ALIGN_VOLTAGE, OPENLOOP_VOLTAGE, FOC_VOLTAGE_LIMIT or CURRENT_LIMIT without asking: I ~ U/R, 1 V is ~6 A.
- Calibration (`DC<amps>` command) keeps a DC current for several seconds: max 2 A unless the user says otherwise.
- If the serial output shows an error or the behaviour is unexpected, disable the motor (`ME0`) before investigating.

## Workflow
- Build: `pio run -e esc1_hall` (or `esc1_hall_smoothing`), flash: `pio run -e <env> -t upload`.
- Serial: 115200 baud through the ST-Link virtual COM port. Commands are documented in README.md.
- Record results in docs/test-log.md.
