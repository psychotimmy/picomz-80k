  cmake_minimum_required(VERSION 3.13)

  include(pico_sdk_import.cmake)
  include(pico_extras_import.cmake)

  project(mz-80ka700 C CXX ASM)

  set(CMAKE_C_STANDARD 11)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_BUILD_TYPE Release)

  string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage")

  if (NOT PICO_SDK_VERSION_STRING VERSION_EQUAL "2.2.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.2.0 is required to build PicoMZ 3.1.0. Your version is ${PICO_SDK_VERSION_STRING}")
  endif()

  pico_sdk_init()

  if (PICO_PLATFORM MATCHES "rp2040")

  set (PICO_BOARD "../boards/vgaboardrp2040")

  add_executable(picomz-rc2014
        picomz.c
        sharpcorp.c
        keyboard.c
        vgadisplay.c
        8255.c
        8253.c
        cassette.c
        miscfuncs.c
        pca9536.c
  )

  add_executable(picomz-pimoroni
        picomz.c
        sharpcorp.c
        keyboard.c
        vgadisplay.c
        8255.c
        8253.c
        cassette.c
        miscfuncs.c
  )

  target_include_directories(picomz-rc2014
  PRIVATE 
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_include_directories(picomz-pimoroni
  PRIVATE 
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_compile_definitions(picomz-rc2014
  PRIVATE
      PICO1=1
      RC2014VGA=1
  )

  target_compile_definitions(picomz-pimoroni
  PRIVATE
      PICO1=1
  )

  add_subdirectory(zazu80)
  add_subdirectory(fatfs)
  add_subdirectory(sdcard)

  link_directories(zazu80)
  link_directories(fatfs)
  link_directories(sdcard)

  target_link_libraries(picomz-rc2014 zazu80 fatfs sdcard pico_stdlib pico_util pico_multicore pico_scanvideo_dpi pico_time hardware_i2c hardware_pwm hardware_pio hardware_gpio tinyusb_host tinyusb_board)

  target_link_libraries(picomz-pimoroni zazu80 fatfs sdcard pico_stdlib pico_util pico_multicore pico_scanvideo_dpi pico_time hardware_pwm hardware_pio hardware_gpio tinyusb_host tinyusb_board)
  
  pico_add_extra_outputs(picomz-rc2014)
  pico_add_extra_outputs(picomz-pimoroni)

else()

  set (PICO_BOARD "../boards/vgaboardrp2350")

  add_executable(pico2mz-pimoroni
        picomz.c
        sharpcorp.c
        keyboard.c
        vgadisplay.c
        8255.c
        8253.c
        cassette.c
        miscfuncs.c
  )

  target_include_directories(pico2mz-pimoroni
  PRIVATE 
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  target_compile_definitions(pico2mz-pimoroni
  PRIVATE
      PICO2=1
  )

  add_subdirectory(zazu80)
  add_subdirectory(fatfs)
  add_subdirectory(sdcard)

  link_directories(zazu80)
  link_directories(fatfs)
  link_directories(sdcard)

  target_link_libraries(pico2mz-pimoroni zazu80 fatfs sdcard pico_stdlib pico_util pico_multicore pico_scanvideo_dpi pico_time hardware_pwm hardware_pio hardware_gpio tinyusb_host tinyusb_board)

  pico_add_extra_outputs(pico2mz-pimoroni)

endif()
