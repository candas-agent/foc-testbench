# PlatformIO pre script: compiles the control loop files with -O2 instead of -Os (speed instead of size).
# The whole firmware at -O2 does not fit in the 64 KB of the GD32F130.
Import("env")
import os

HOT = {"FOCMotor", "BLDCMotor", "CurrentSense", "LowsideCurrentSense", "gd32_mcu", "gd32f130_mcu", "BLDCDriver6PWM",
       "foc_utils", "pid", "lowpass_filter", "Sensor", "HallSensor", "SmoothingSensor", "MXLEMMINGObserverSensor",
       "bench", "fastmath"}


def o2_hot(env, node):
    name = os.path.splitext(os.path.basename(node.get_path()))[0]
    if name not in HOT:
        return node
    # the last -O option wins: -Os comes from CCFLAGS, -O2 is appended after it
    return env.Object(node, CCFLAGS=env["CCFLAGS"] + ["-O2"])


env.AddBuildMiddleware(o2_hot)
