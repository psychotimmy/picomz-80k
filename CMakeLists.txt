  cmake_minimum_required(VERSION 3.13)

  include(pico_sdk_import.cmake)
  include(pico_extras_import.cmake)

  project(mz-80k C CXX ASM)

  set(CMAKE_C_STANDARD 11)
  set(CMAKE_CXX_STANDARD 17)

  if (PICO_SDK_VERSION_STRING VERSION_LESS "2.0.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.0.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
  endif()

  pico_sdk_init()

  if (PICO_PLATFORM MATCHES "rp2040")

  add_executable(picomz-80k
	picomz.c
        sharpcorp.c
	keyboard.c
        vgadisplay.c
        8255.c
        8253.c
        cassette.c
  )

  add_executable(picomz-80k-diag
	picomz.c
        sharpcorp.c
	keyboard.c
        vgadisplay.c
        8255.c
        8253.c
        cassette.c
  )

  target_include_directories(picomz-80k
  PRIVATE 
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_compile_definitions(picomz-80k-diag
  PRIVATE
      PLL_SYS_REFDIV=2
      PLL_SYS_VCO_FREQ_HZ=1050000000
      PLL_SYS_POSTDIV1=6
      PLL_SYS_POSTDIV2=1
      PICO1=1
      USBDIAGOUTPUT=1
  )

  add_subdirectory(zazu80)
  add_subdirectory(fatfs)
  add_subdirectory(sdcard)

  link_directories(zazu80)
  link_directories(fatfs)
  link_directories(sdcard)

  target_link_libraries(picomz-80k zazu80 fatfs sdcard pico_stdlib pico_util pico_multicore pico_scanvideo_dpi pico_time hardware_pwm hardware_pio hardware_gpio tinyusb_host tinyusb_board)

  target_link_libraries(picomz-80k-diag zazu80 fatfs sdcard pico_stdlib pico_util pico_multicore pico_scanvideo_dpi pico_time hardware_pwm hardware_pio hardware_gpio)

  pico_enable_stdio_usb(picomz-80k-diag 1)
  pico_enable_stdio_uart(picomz-80k-diag 0)

  pico_add_extra_outputs(picomz-80k)
  pico_add_extra_outputs(picomz-80k-diag)

else()

  add_executable(pico2mz-80k
	picomz.c
        sharpcorp.c
	keyboard.c
        vgadisplay.c
        8255.c
        8253.c
        cassette.c
  )

  add_executable(pico2mz-80k-diag
	picomz.c
        sharpcorp.c
	keyboard.c
        vgadisplay.c
        8255.c
        8253.c
        cassette.c
  )

  target_include_directories(pico2mz-80k
  PRIVATE 
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_compile_definitions(pico2mz-80k
  PRIVATE
      PLL_SYS_REFDIV=1
      PLL_SYS_VCO_FREQ_HZ=1440000000
      PLL_SYS_POSTDIV1=4
      PLL_SYS_POSTDIV2=4
      PICO2=1
  )

  target_compile_definitions(pico2mz-80k-diag
  PRIVATE
      PLL_SYS_REFDIV=1
      PLL_SYS_VCO_FREQ_HZ=1500000000
      PLL_SYS_POSTDIV1=6
      PLL_SYS_POSTDIV2=2
      PICO2=1
      USBDIAGOUTPUT=1
  )

  add_subdirectory(zazu80)
  add_subdirectory(fatfs)
  add_subdirectory(sdcard)

  link_directories(zazu80)
  link_directories(fatfs)
  link_directories(sdcard)

  target_link_libraries(pico2mz-80k zazu80 fatfs sdcard pico_stdlib pico_util pico_multicore pico_scanvideo_dpi pico_time hardware_pwm hardware_pio hardware_gpio tinyusb_host tinyusb_board)

  target_link_libraries(pico2mz-80k-diag zazu80 fatfs sdcard pico_stdlib pico_util pico_multicore pico_scanvideo_dpi pico_time hardware_pwm hardware_pio hardware_gpio)

  pico_enable_stdio_usb(pico2mz-80k-diag 1)
  pico_enable_stdio_uart(pico2mz-80k-diag 0)

  pico_add_extra_outputs(pico2mz-80k)
  pico_add_extra_outputs(pico2mz-80k-diag)

endif()
