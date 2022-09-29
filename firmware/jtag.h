/*
  Copyright (c) 2017 Jean THOMAS.

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the Software
  is furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/**
 * @brief Handle a DirtyJTAG command
 *
 * @param usbd_dev USB device
 * @param transfer Received packet
 * @return Command needs to send data back to host
 */
void cmd_handle(uint8_t* rxbuf, uint32_t count, uint8_t* tx_buf);

static int tdi_gpio = 16;
static int tdo_gpio = 17;
static int tck_gpio = 18;
static int tms_gpio = 19;

#define JTAG_ITF     1

#define LED_PIN      25

// How does this 'feature' even work? Perhaps the 'slew rate' on Raspberry Pi
// GPIO pins (not Pico?) is slow enough to require these delays? Or the "GPIO
// engine" on Raspberry Pi is slow to register GPIO actions?
#define jtag_delay   3
// #define jtag_delay   5
// #define jtag_delay   50
