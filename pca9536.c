/* ==========================================================================
        PCA9536 - 4bits GPIO Expander for I2C bus
   ========================================================================== 

   Code modified by Tim Holyoake, January 2025, from picoterm version 1.6.x, 
   https://github.com/RC2014Z80/picoterm supplied with the following licence:

   BSD 3-Clause License

   Copyright (c) 2023, RC2014

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

   3. Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE. */

#include "picomz.h"

i2c_inst_t *i2c_bus = i2c1;  // Use i2c bus 1

bool has_pca9536( i2c_inst_t *i2c ){
  // try to read configuration of the PC9536 on the I2C bus. Check for immutable
  // configuration bits
  uint8_t data[6];
  // 20ms timeout
  int nb_bytes=reg_read_timeout(i2c,PCA9536_ADDR,REG_INPUT,data,4,20000);
  if(nb_bytes <= 0){
    return false;
  }
  // try to write the register, 
  // will return the PICO_ERROR_GENERIC error if not possible
  data[0] = 0x00;
  nb_bytes = reg_write( i2c, PCA9536_ADDR, REG_POLARITY, data, 1 );
  if( nb_bytes == PICO_ERROR_GENERIC )
    return false;
  // so we have a PCA9536 on the I2C bus.
  return true;
}

bool pca9536_setup_io( i2c_inst_t *i2c, uint8_t io, uint8_t io_mode ){
  if( io>IO_3 )
    return false;

  uint8_t data[2];
  reg_read( i2c, PCA9536_ADDR, REG_CONFIG, data, 1 );

  uint8_t iodir = data[0];
  if( io_mode == IO_MODE_IN )
    iodir |= (1 << io); // Set the bit
  else
    if( io_mode == IO_MODE_OUT )
      iodir &= ~(1 << io); // Reset the bit
    else
      return false;

  // Write the config.
  data[0] = iodir;
  reg_write(i2c,PCA9536_ADDR,REG_CONFIG,data,1);
  return true;
}

bool pca9536_output_io( i2c_inst_t *i2c, uint8_t io, bool value ){
  if( io>IO_3 )
    return false;

  uint8_t data[2];
  reg_read( i2c, PCA9536_ADDR, REG_OUTPUT, data, 1 );

  uint8_t gpio_state = data[0];
  if( value )
    gpio_state |= (1 << io); // Set the bit
  else
    gpio_state &= ~(1 << io); // Clear the bit

  data[0] = gpio_state;
  reg_write( i2c, PCA9536_ADDR, REG_OUTPUT, data, 1 );
  return true;
}

bool pca9536_output_reset( i2c_inst_t *i2c, uint8_t mask ){
  /* 4 lower bits of mask indicates which of the output pins should be reset */
  uint8_t data[2];
  reg_read( i2c, PCA9536_ADDR, REG_OUTPUT, data, 1 );

  uint8_t gpio_state = data[0];
  for( int idx=0; idx<=3; idx++)
    if (mask & (1<<idx)) {       // should we reset the idx'th bit
      gpio_state &= ~(1 << idx); // Clear the bit
    }

  data[0] = gpio_state;
  reg_write( i2c, PCA9536_ADDR, REG_OUTPUT, data, 1 );
  return true;
}

bool pca9536_input_io( i2c_inst_t *i2c, uint8_t io ) {
  // read state an input gpio
  if( io>IO_3 )
    return false;

  uint8_t data[2];
  reg_read( i2c, PCA9536_ADDR, REG_INPUT, data, 1 );

  return (data[0] & (1<<io)) ? true : false ;
}

void init_i2c_bus(){
	//Initialize I2C port at 400 kHz
	i2c_init(i2c_bus, 400 * 1000);

	// Initialize I2C pins
	gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);

}

void deinit_i2c_bus(){
	// de-Initialize I2C pins
	gpio_set_function(SDA_PIN, GPIO_FUNC_NULL);
	gpio_set_function(SCL_PIN, GPIO_FUNC_NULL);

	// de-initialize I2C hardware block
	i2c_deinit(i2c_bus);
}

// Write 1 byte to the specified register
int reg_write(  i2c_inst_t *i2c,
                const uint addr,
                const uint8_t reg,
                uint8_t *buf,
                const uint8_t nbytes) {

    int num_bytes_write = 0;
    uint8_t msg[nbytes + 1];

    // Check to make sure caller is sending 1 or more bytes
    if (nbytes < 1) {
        return 0;
    }

    // Append register address to front of data packet
    msg[0] = reg;
    for (int i = 0; i < nbytes; i++) {
        msg[i + 1] = buf[i];
    }

    // Write data to register(s) over I2C
    num_bytes_write = i2c_write_blocking(i2c, addr, msg, (nbytes + 1), false);

    return num_bytes_write;
}

// Read byte(s) from specified register. If nbytes > 1, read from consecutive
// registers. Returns the number for bytes readed.
int reg_read(  i2c_inst_t *i2c,
                const uint addr,
                const uint8_t reg,
                uint8_t *buf,
                const uint8_t nbytes) {

    int num_bytes_read = 0;

    // Check to make sure caller is asking for 1 or more bytes
    if (nbytes < 1) {
        return (0);
    }

    // Read data from register(s) over I2C
    i2c_write_blocking(i2c, addr, &reg, 1, true);
    num_bytes_read = i2c_read_blocking(i2c, addr, buf, nbytes, false);

    return (num_bytes_read);
}

// Read byte(s) from specified register. If nbytes > 1, read from consecutive
// registers. Returns the number for bytes readed.
int reg_read_timeout(  i2c_inst_t *i2c,
                const uint addr,
                const uint8_t reg,
                uint8_t *buf,
                const uint8_t nbytes,
                const uint timeout_us ) {

    int num_bytes_read = 0;

    // Check to make sure caller is asking for 1 or more bytes
    if (nbytes < 1) {
        return (0);
    }

    // Read data from register(s) over I2C
    i2c_write_timeout_us(i2c,addr,&reg,1,true,timeout_us);
    num_bytes_read=i2c_read_timeout_us(i2c,addr,buf,nbytes,false,timeout_us);

    return (num_bytes_read);
}
