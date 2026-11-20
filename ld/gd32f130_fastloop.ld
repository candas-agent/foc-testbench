

/* memory map */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08000000 + (DEFINED(LD_FLASH_OFFSET) ? LD_FLASH_OFFSET : 0), LENGTH = 64K - (DEFINED(LD_FLASH_OFFSET) ? LD_FLASH_OFFSET : 0)
  RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 8K
}

ENTRY(Reset_Handler)

SECTIONS
{
  __stack_size = DEFINED(__stack_size) ? __stack_size : 2048;
  
/* ISR vectors */
  .vectors :
  {
    . = ALIGN(4);
    KEEP(*(.vectors))
    . = ALIGN(4);
/*    __Vectors_End = .;
    __Vectors_Size = __Vectors_End - __gVectors;*/
  } >FLASH
  

  /* GD32F130: only the first 32 KB of flash run without wait states (measured on the bench: the same loop
     takes 5.5x more cycles above 0x08008000). The code of the control loop is placed first, the rest
     (init, commands, printing, string conversions) after. __fast_end must stay below 0x08008000. */
  .text :
  {
    . = ALIGN(4);
    /* soft float and the few libm/libc functions used in the loop */
    *libgcc.a:*(.text*)
    *libm.a:*(.text*)
    *(.text.qfp*)
    *libc_nano.a:lib_a-memcpy*.o(.text*)
    *libc_nano.a:lib_a-memset*.o(.text*)
    /* SimpleFOC control loop */
    *FOCMotor.cpp.o(.text*)
    *BLDCMotor.cpp.o(.text*)
    *CurrentSense.cpp.o(.text*)
    *gd32_mcu.cpp.o(.text*)
    *gd32f130_mcu.cpp.o(.text*)
    *BLDCDriver6PWM.cpp.o(.text*)
    *foc_utils.cpp.o(.text*)
    *pid.cpp.o(.text*)
    *lowpass_filter.cpp.o(.text*)
    *Sensor.cpp.o(.text*)
    /* test bench loop, time, hall interrupts, RTT polling */
    *bench.cpp.o(.text.*bench_step* .text.*bench_loop* .text.*bench_disable* .text.*updateDebug*)
    *bench.cpp.o(.text.*safety* .text.*rec_update* .text.*computeCompensation* .text.*VoltageLimit* .text.*isOpenLoop*)
    *fastmath.cpp.o(.text* .rodata*)
    *gd32_micros.c.o(.text*)
    *(.text._Z8prof_add* .text._Z13prof_end_loop*)
    *(.text._ZN9Commander3run*)
    *time_utils.cpp.o(.text*)
    *generic_mcu.cpp.o(.text*)
    /* framework and peripheral library functions called by the loop and the interrupts */
    *(.text.adc_inserted_data_read .text.timer_channel_output_pulse_value_config)
    *(.text.timer_channel_output_state_config .text.timer_channel_complementary_output_state_config)
    *(.text.exti_interrupt_flag_get .text.exti_interrupt_flag_clear .text.digitalRead .text.__errno)
    *main.cpp.o(.text.loop)
    *systick.c.o(.text*)
    *wiring_time.c.o(.text*)
    *gpio_interrupt.c.o(.text*)
    *SEGGER_RTT*.o(.text*)
    *RTTStream.cpp.o(.text*)
    /* C++ virtual tables: read at every virtual call (sensor, current sense, driver, RTT stream) */
    *(.rodata._ZTV*)
    /* other constant data read in the loop and the hall interrupts (switch tables, pin maps, hall sectors) */
    *FOCMotor.cpp.o(.rodata) *gd32f130_mcu.cpp.o(.rodata)
    *(.rodata._ZL16ELECTRIC_SECTORS .rodata.bench_io .rodata.gpio_pin .rodata.gpio_port .rodata.digital_pins)
    __fast_end = .;
    *(.text)
    *(.text*)
    *(.glue_7) 
    *(.glue_7t)
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    /* the symbol _etext will be defined at the end of code section */
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

   .ARM.extab :
  { 
     *(.ARM.extab* .gnu.linkonce.armextab.*) 
  } >FLASH
  
    .ARM : {
    __exidx_start = .;
      *(.ARM.exidx*)
      __exidx_end = .;
    } >FLASH

  .ARM.attributes : { *(.ARM.attributes) } > FLASH

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(.fini_array*))
    KEEP (*(SORT(.fini_array.*)))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH



  /* provide some necessary symbols for startup file to initialize data */
  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    /* the symbol _sdata will be defined at the data section end start */
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    /* the symbol _edata will be defined at the data section end */
    _edata = .;
  } >RAM AT> FLASH

  . = ALIGN(4);
  .bss :
  {
    /* the symbol _sbss will be defined at the bss section start */
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    /* the symbol _ebss will be defined at the bss section end */
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

 . = ALIGN(8);
  PROVIDE ( end = _ebss );
  PROVIDE ( _end = _ebss );
  .stack ORIGIN(RAM) + LENGTH(RAM) - __stack_size :
  {
    PROVIDE( _heap_end = . );
    . = __stack_size;
    PROVIDE( _sp = . );
  } >RAM AT>RAM
}

 ASSERT(__fast_end <= 0x08008000, "control loop code exceeds the 32 KB zero wait state flash area")

 /* input sections */
GROUP(libgcc.a libc.a libm.a libnosys.a)
