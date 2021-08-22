/*
  Copyright (c) 2017 Jean THOMAS.
  Copyright (c) 2021 Dhiru Kholia.

  Uses code from https://github.com/kholia/xvcpi/blob/master/xvcpi.c file.

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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <pico/stdlib.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>

#include "tusb.h"
#include "cmd.h"

// Modified
enum CommandIdentifier {
  CMD_STOP = 0x00,
  CMD_XFER = 0x03,
  CMD_WRITE = 0x04,
};

static void led_on()
{
  gpio_put(LED_PIN, 1);
}

static void led_off()
{
  gpio_put(LED_PIN, 0);
}

static void gpio_write(int tck, int tms, int tdi)
{
  gpio_put(tck_gpio, tck);
  gpio_put(tms_gpio, tms);
  gpio_put(tdi_gpio, tdi);

  for (unsigned int i = 0; i < jtag_delay; i++)
    asm volatile("nop");
}

static int gpio_read(void)
{
  return gpio_get(tdo_gpio);
}

// Handler for "gpio_xfer" on the host side
static void cmd_xfer(const uint8_t *commands, uint8_t* tx_buffer)
{
  uint32_t tdo = 0;
  int header_offset = 0;
  // Replace these with memcpy
  uint32_t n = (commands[4] << 24) | (commands[3] << 16) | (commands[2] << 8) | (commands[1] << 0);
  uint32_t tms = (commands[8] << 24) | (commands[7] << 16) | (commands[6] << 8) | (commands[5] << 0);
  uint32_t tdi = (commands[12] << 24) | (commands[11] << 16) | (commands[10] << 8) | (commands[9] << 0);
  int bytes = (n + 7) / 8;

  for (uint32_t i = 0; i < n; i++) {
    gpio_write(0, tms & 1, tdi & 1);
    tdo |= gpio_read() << i;
    gpio_write(1, tms & 1, tdi & 1);
    tms >>= 1;
    tdi >>= 1;
  }

  tx_buffer[header_offset++] = (tdo >> 0) & 0xFF;  // uint32_t to bytes
  tx_buffer[header_offset++] = (tdo >> 8) & 0xFF;
  tx_buffer[header_offset++] = (tdo >> 16) & 0xFF;
  tx_buffer[header_offset++] = (tdo >> 24) & 0xFF;

  /* Send the transfer response back to host */
  tud_vendor_write(tx_buffer, bytes);

  // debug code
  // led_on();
}

// Handler for "gpio_write" on the host side
static void cmd_write(const uint8_t *commands)
{
  uint8_t tck, tms, tdi;

  tck = commands[1];
  tms = commands[2];
  tdi = commands[3];
  gpio_write(tck, tms, tdi);
}

void cmd_handle(uint8_t *rx_buf, __attribute__((unused)) uint32_t count, uint8_t *tx_buf)
{
  uint8_t *commands = (uint8_t*)rx_buf;

  while (*commands != CMD_STOP) {
    switch ((*commands) & 0x0F) {
      case CMD_XFER:
        cmd_xfer(commands, tx_buf);
        return;
        break;

      case CMD_WRITE:
        cmd_write(commands);
        commands += 3;
        break;

      default:
        return; /* Unsupported command, halt */
        break;
    }

    commands++;
  }

  return;
}
