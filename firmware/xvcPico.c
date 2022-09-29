#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "bsp/board.h"
#include "tusb.h"
#include "xvcPico.h"
#include "jtag.h"
#include "axm.h"

void __time_critical_func(core1_entry)() {
  while (1) {
    if (tud_cdc_n_available(0)) {
      char buf[64];
      uint32_t count = tud_cdc_n_read(0, buf, sizeof(buf));
      tud_cdc_n_read_flush(0);
      for (int i = 0; i < count; i++) {
        uart_putc(UART_ID, buf[i]);
      }
    }

    int i = 0;
    char str[56];
    while (uart_is_readable(UART_ID)) {
      str[i] = uart_getc(UART_ID);
      i++;
    }
    str[i] = 0;
    if (i != 0) {
      tud_cdc_n_write_str(0, str);
      tud_cdc_n_write_flush(0);
    }
  }
}

buffer_info buffer_info_jtag;
buffer_info buffer_info_axm;

static cmd_buffer tx_buf;

void __time_critical_func(from_host_task)() {
  if ((buffer_info_jtag.busy == false)) {
    //If tud_task() is called and tud_vendor_read isn't called immediately (i.e before calling tud_task again)
    //after there is data available, there is a risk that data from 2 BULK OUT transaction will be (partially) combined into one
    //The DJTAG protocol does not tolerate this.
    tud_task();  // tinyusb device task
    if (tud_vendor_n_available(JTAG_ITF)) {
      uint count = tud_vendor_n_read(JTAG_ITF, buffer_info_jtag.buffer, 64);
      if (count != 0) {
        buffer_info_jtag.count = count;
        buffer_info_jtag.busy = true;
      }
    }
  }

  if ((buffer_info_axm.busy == false)) {
    tud_task();
    if (tud_vendor_n_available(AXM_ITF)) {
      uint count = tud_vendor_n_read(AXM_ITF, buffer_info_axm.buffer, 64);
      if (count != 0) {
        buffer_info_axm.count = count;
        buffer_info_axm.busy = true;
      }
    }
  }
}

void fetch_command() {
  if (buffer_info_jtag.busy) {
    cmd_handle(buffer_info_jtag.buffer, buffer_info_jtag.count, tx_buf);
    buffer_info_jtag.busy = false;
  }
}

//this is to work around the fact that tinyUSB does not handle setup request automatically
//Hence this boiler plate code
bool tud_vendor_control_xfer_cb(__attribute__((unused)) uint8_t rhport, uint8_t stage, __attribute__((unused)) tusb_control_request_t const* request) {
  if (stage != CONTROL_STAGE_SETUP) return true;
  return false;
}

int main() {
  board_init();
  tusb_init();

  // JTAG init
  gpio_init(tdi_gpio);
  gpio_init(tdo_gpio);
  gpio_init(tck_gpio);
  gpio_init(tms_gpio);
  gpio_set_dir(tdi_gpio, GPIO_OUT);
  gpio_set_dir(tdo_gpio, GPIO_IN);
  gpio_set_dir(tck_gpio, GPIO_OUT);
  gpio_set_dir(tms_gpio, GPIO_OUT);
  gpio_put(tdi_gpio, 0);
  gpio_put(tck_gpio, 0);
  gpio_put(tms_gpio, 1);

  // Set up our UART with the required speed.
  uart_init(UART_ID, BAUD_RATE);
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
  uart_set_fifo_enabled(UART_ID, true);

  // pmod config
  gpio_init(PCK_PIN);
  gpio_init(PWRITE_PIN);
  gpio_init(PWD0_PIN);
  gpio_init(PWD1_PIN);
  gpio_init(PRD0_PIN);
  gpio_init(PRD1_PIN);
  gpio_init(PWAIT_PIN);
  gpio_set_dir(PCK_PIN, GPIO_OUT);
  gpio_set_dir(PWRITE_PIN, GPIO_OUT);
  gpio_set_dir(PWD0_PIN, GPIO_OUT);
  gpio_set_dir(PWD1_PIN, GPIO_OUT);
  gpio_set_dir(PRD0_PIN, GPIO_IN);
  gpio_set_dir(PRD1_PIN, GPIO_IN);
  gpio_set_dir(PWAIT_PIN, GPIO_IN);
  gpio_put(PCK_PIN, 0);
  gpio_put(PWRITE_PIN, 0);
  gpio_put(PWD0_PIN, 0);
  gpio_put(PWD1_PIN, 0);

  // LED config
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  multicore_launch_core1(core1_entry);
  while (1) {
    from_host_task();
    fetch_command();
    pmod_task();
  }
}
