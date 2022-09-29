/*
 * This file is based on a file originally part of the
 * MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * Copyright (c) 2019 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "tusb.h"
#include "pico/unique_id.h"

#define USBD_VID (0x2E8A)  // Raspberry Pi
#define USBD_PID (0x000A)  // Raspberry Pi Pico SDK CDC

#define USBD_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_VENDOR_DESC_LEN + TUD_VENDOR_DESC_LEN)
#define USBD_MAX_POWER_MA (250)

#define USBD_ITF_CDC (0)  // needs 2 interfaces
#define USBD_ITF_NUM_PROBE0 (2)
#define USBD_ITF_NUM_PROBE1 (3)
#define USBD_ITF_MAX (4)

#define USBD_CDC_EP_CMD (0x81)
#define USBD_CDC_EP_OUT (0x02)
#define USBD_CDC_EP_IN (0x82)
#define USBD_CDC_CMD_MAX_SIZE (8)
#define USBD_CDC_IN_OUT_MAX_SIZE (64)

#define USBD_PROBE0_OUT_EP_NUM (0x03)
#define USBD_PROBE0_IN_EP_NUM (0x84)
#define USBD_PROBE1_OUT_EP_NUM (0x05)
#define USBD_PROBE1_IN_EP_NUM (0x86)

#define USBD_STR_0 (0x00)
#define USBD_STR_MANUF (0x01)
#define USBD_STR_PRODUCT (0x02)
#define USBD_STR_SERIAL (0x03)
#define USBD_STR_CDC (0x04)
#define USBD_STR_VENDOR0 (0x05)
#define USBD_STR_VENDOR1 (0x06)

// Note: descriptors returned from callbacks must exist long enough for transfer to complete

static const tusb_desc_device_t usbd_desc_device = {
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = TUSB_CLASS_MISC,
  .bDeviceSubClass = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor = USBD_VID,
  .idProduct = USBD_PID,
  .bcdDevice = 0x0110,
  .iManufacturer = USBD_STR_MANUF,
  .iProduct = USBD_STR_PRODUCT,
  .iSerialNumber = USBD_STR_SERIAL,
  .bNumConfigurations = 1,
};

static const uint8_t usbd_desc_cfg[USBD_DESC_LEN] = {
  TUD_CONFIG_DESCRIPTOR(1, USBD_ITF_MAX, USBD_STR_0, USBD_DESC_LEN, 0, USBD_MAX_POWER_MA),

  TUD_CDC_DESCRIPTOR(USBD_ITF_CDC, USBD_STR_CDC, USBD_CDC_EP_CMD,
                     USBD_CDC_CMD_MAX_SIZE, USBD_CDC_EP_OUT, USBD_CDC_EP_IN, USBD_CDC_IN_OUT_MAX_SIZE),

  TUD_VENDOR_DESCRIPTOR(USBD_ITF_NUM_PROBE0, USBD_STR_VENDOR0, USBD_PROBE0_OUT_EP_NUM, USBD_PROBE0_IN_EP_NUM, 64),  //
  TUD_VENDOR_DESCRIPTOR(USBD_ITF_NUM_PROBE1, USBD_STR_VENDOR1, USBD_PROBE1_OUT_EP_NUM, USBD_PROBE1_IN_EP_NUM, 64),  //
};

static char usbd_serial_str[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];

static const char *const usbd_desc_str[] = {
  [USBD_STR_MANUF] = "Raspberry Pi",
  [USBD_STR_PRODUCT] = "Pico",
  [USBD_STR_SERIAL] = usbd_serial_str,
  [USBD_STR_CDC] = "Board CDC",
  [USBD_STR_VENDOR0] = "Vendor0",
  [USBD_STR_VENDOR1] = "Vendor1",
};

const uint8_t *tud_descriptor_device_cb(void) {
  return (const uint8_t *)&usbd_desc_device;
}

const uint8_t *tud_descriptor_configuration_cb(__unused uint8_t index) {
  return usbd_desc_cfg;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, __unused uint16_t langid) {
#define DESC_STR_MAX (20)
  static uint16_t desc_str[DESC_STR_MAX];

  // Assign the SN using the unique flash id
  if (!usbd_serial_str[0]) {
    pico_get_unique_board_id_string(usbd_serial_str, sizeof(usbd_serial_str));
  }

  uint8_t len;
  if (index == 0) {
    desc_str[1] = 0x0409;  // supported language is English
    len = 1;
  } else {
    if (index >= sizeof(usbd_desc_str) / sizeof(usbd_desc_str[0])) {
      return NULL;
    }
    const char *str = usbd_desc_str[index];
    for (len = 0; len < DESC_STR_MAX - 1 && str[len]; ++len) {
      desc_str[1 + len] = str[len];
    }
  }

  // first byte is length (including header), second byte is string type
  desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);

  return desc_str;
}
