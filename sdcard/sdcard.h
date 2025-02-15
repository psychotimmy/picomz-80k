#define SDCARD_SPI_BUS    spi1

#define SDCARD_PIO        pio1
#define SDCARD_PIO_SM        0

#ifdef RC2014VGA

  /* SPI pin assignment for RC2014 RP2040 & Pi Pico VGA cards */

  #define SDCARD_PIN_SPI0_CS    5
  #define SDCARD_PIN_SPI0_SCK  26
  #define SDCARD_PIN_SPI0_MOSI 27
  #define SDCARD_PIN_SPI0_MISO 28

#else

  /* SPI pin assignment for Pimoroni Pico VGA base */

  #define SDCARD_PIN_SPI0_CS   22
  #define SDCARD_PIN_SPI0_SCK   5
  #define SDCARD_PIN_SPI0_MOSI 18
  #define SDCARD_PIN_SPI0_MISO 19

#endif
