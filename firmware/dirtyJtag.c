#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "bsp/board.h"
#include "tusb.h"
#include "cmd.h"
#include "get_serial.h"

// #define MULTICORE

typedef uint8_t cmd_buffer[64];
static uint wr_buffer_number = 0;
static uint rd_buffer_number = 0;
typedef struct buffer_info
{
  volatile uint8_t count;
  volatile uint8_t busy;
  cmd_buffer buffer;
} buffer_info;

#define n_buffers (4)

buffer_info buffer_infos[n_buffers];

static cmd_buffer tx_buf;

void jtag_main_task()
{
#ifdef MULTICORE
  if (multicore_fifo_rvalid())
  {
    //some command processing has been done
    uint rx_num = multicore_fifo_pop_blocking();
    buffer_info* bi = &buffer_infos[rx_num];
    bi->busy = false;

  }
#endif
  if ((buffer_infos[wr_buffer_number].busy == false))
  {
    //If tud_task() is called and tud_vendor_read isn't called immediately (i.e before calling tud_task again)
    //after there is data available, there is a risk that data from 2 BULK OUT transaction will be (partially) combined into one
    //The DJTAG protocol does not tolerate this.
    tud_task();// tinyusb device task
    if (tud_vendor_available())
    {
      uint bnum = wr_buffer_number;
      uint count = tud_vendor_read(buffer_infos[wr_buffer_number].buffer, 64);
      if (count != 0)
      {
        buffer_infos[bnum].count = count;
        buffer_infos[bnum].busy = true;
        wr_buffer_number = wr_buffer_number + 1; //switch buffer
        if (wr_buffer_number == n_buffers)
        {
          wr_buffer_number = 0;
        }
#ifdef MULTICORE
        multicore_fifo_push_blocking(bnum);
#endif
      }
    }

  }

}

void jtag_task()
{
#ifndef MULTICORE
  jtag_main_task();
#endif
}

#ifdef MULTICORE
void core1_entry() {

  while (1)
  {
    uint rx_num = multicore_fifo_pop_blocking();
    buffer_info* bi = &buffer_infos[rx_num];
    assert (bi->busy);
    cmd_handle(bi->buffer, bi->count, tx_buf);
    multicore_fifo_push_blocking(rx_num);
  }

}
#endif

void fetch_command()
{
#ifndef MULTICORE
  if (buffer_infos[rd_buffer_number].busy)
  {
    cmd_handle(buffer_infos[rd_buffer_number].buffer, buffer_infos[rd_buffer_number].count, tx_buf);
    buffer_infos[rd_buffer_number].busy = false;
    rd_buffer_number++; //switch buffer
    if (rd_buffer_number == n_buffers)
    {
      rd_buffer_number = 0;
    }
  }
#endif
}

//this is to work around the fact that tinyUSB does not handle setup request automatically
//Hence this boiler plate code
bool tud_vendor_control_xfer_cb(__attribute__((unused)) uint8_t rhport, uint8_t stage, __attribute__((unused)) tusb_control_request_t const * request)
{
  if (stage != CONTROL_STAGE_SETUP) return true;
  return false;
}

int main()
{
  board_init();
  usb_serial_init();
  tusb_init();

  // GPIO init
  gpio_init(tdi_gpio);
  gpio_init(tdo_gpio);
  gpio_init(tck_gpio);
  gpio_init(tms_gpio);
  gpio_set_dir(tdi_gpio, GPIO_OUT);
  gpio_set_dir(tdo_gpio, GPIO_IN);
  gpio_set_dir(tck_gpio, GPIO_OUT);
  gpio_set_dir(tms_gpio, GPIO_OUT);
  // Initial state
  gpio_put(tdi_gpio, 0);
  gpio_put(tck_gpio, 0);
  gpio_put(tms_gpio, 1);

  // LED config
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

#ifdef MULTICORE
  multicore_launch_core1(core1_entry);
#endif
  while (1) {
    jtag_main_task();
    fetch_command();//for unicore implementation
  }
}
