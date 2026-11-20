**Title:** [BUG] PlatformIO: library.json is missing from the registry package, so `lib_archive = false` is still needed and costs flash/RAM

**Describe the bug**

`library.json` (#357) sets `"libArchive": false` so that `lib_archive = false` should no longer be needed in
`platformio.ini`. But it only contains `build`, so `pio pkg pack` rejects it (no `name`/`version`) and the registry
package `askuric/Simple FOC` 2.4.0 has no `library.json`. Registry users get an archived SimpleFOC, where the weak
generic hardware functions of `generic_mcu.cpp` can win: empty PWM/ADC functions, no error (#443, #445).

The documented workaround, `lib_archive = false` in `platformio.ini`, overrides the setting of **every** library,
so unused code and global objects of all of them are linked too.

**Describe the hardware setup**

Any board using the PlatformIO registry package (e.g. B-G431B-ESC1: no PWM, #443). Tested by building the
examples of the existing workflows on 24 boards. Motor, driver and sensor not relevant.

**IDE you are using**

PlatformIO (Core 6.2.0). The Arduino IDE / `arduino-cli` do not archive libraries and are not affected
(this is also why the existing workflows don't see it).

**Tried the Getting started guide? - if applicable**

Yes: with `lib_archive = false` from the docs it works, but the firmware is bigger.

**Proposed fix**

Current `library.json` (master and dev), rejected by `pio pkg pack`:

```json
{
    "build": {
      "libArchive": false
    }
}
```

A complete `library.json` (based on `dev`), then drop `lib_archive = false` from the docs
(keep `version` in sync with `library.properties` at each release):

```json
{
  "name": "Simple FOC",
  "version": "2.4.1",
  "description": "A library demistifying FOC for BLDC motors",
  "keywords": "FOC, BLDC, stepper, motor control",
  "repository": { "type": "git", "url": "https://github.com/simplefoc/Arduino-FOC.git" },
  "homepage": "https://docs.simplefoc.com",
  "license": "MIT",
  "frameworks": "arduino",
  "platforms": "*",
  "headers": "SimpleFOC.h",
  "build": { "libArchive": false }
}
```

**Test**

A PlatformIO workflow builds the boards and examples of the existing workflows 3 ways and reads the linker map to
see which file provides each hardware function (branch https://github.com/candas-agent/Arduino-FOC/tree/ci/platformio-libarchive,
run https://github.com/candas-agent/Arduino-FOC/actions/runs/44497136332):
`lib_archive = false` (docs, "before"), no `library.json` (registry), complete `library.json` and no `lib_archive` ("after").
`pio pkg pack` fails with the current `library.json` and works with the complete one
(job "pack": https://github.com/candas-agent/Arduino-FOC/actions/runs/44497136332/job/135467106450,
step "Pack with a library.json containing only build"; the step does not fail the job since the error is expected):

```
$ pio pkg pack .
ManifestValidationError: Invalid manifest fields: {'name': ['Missing data for required field.'], 'version': ['Missing data for required field.']}.
```

With the complete `library.json`: `Wrote a tarball to ".../SimpleFOC-2.4.1.tar.gz"`, and the tarball contains `library.json`.

| Boards | Examples built | Hardware functions after = before | Registry package: empty hardware functions | Flash reduction | RAM reduction |
|---|---|---|---|---|---|
| AVR (Uno, Mega, Leonardo) | 51 | 51/51 | 0 | 0 to 9.4 % (up to 0.3 KB) | 0 to 0.8 % |
| SAM (Due) | 4 | 4/4 | 0 | 0 | 0 |
| mbed (Nano RP2040, Nano 33 BLE, Portenta H7), RP2040 (Pico) | 4 | 4/4 | 0 | 0 | 0 |
| ESP32 (S2, S3, C3, ESP32) | 15 | 15/15 | 0 | 0 | 0 |
| SAMD21 / SAMD51 (Nano 33 IoT, Metro M4) | 6 | 6/6 | 6 | 0 to 2.4 % (up to 1.2 KB) | 0 to 1.2 % |
| STM32 (F103, F405, F411, F746, G431, B-G431B-ESC1, L475, H723) | 52 | 52/52 | 45 | 0 to 6.8 % (up to 3.3 KB) | 0 to 5.2 % |
| Teensy (4.1, 3.6) | 1 | 1/1 | 1 | n/a | n/a |

Not built in any variant (unrelated): 2 Uno examples, and 4 Teensy entries wrongly specified in the test.
Details per board and example in the run summary.
