# Drops SimpleFOCDrivers STM32HWEncoder.cpp from the build: it does not compile with stm32duino main
# (attachInterrupt / enableTimerClock signatures), and the bench does not use it. Only for the core_* envs.
Import("env")

env.AddBuildMiddleware(lambda node: None, "*STM32HWEncoder.cpp")
