/*
   Description: Xilinx Virtual Cable Server for Raspberry Pico Board.

   Glued together by Dhiru Kholia (VU3CER) in August, 2021.

   Optimizations by https://github.com/tom01h (October 2021).

   Logic: We already have a working `xvcpi` program. We retain this program
   structure as it is, and replace the 'GPIO functions' with libusb calls. On
   the Raspberry Pico end, we provide functions to match these libusb calls.

   Upstream sources:
   - https://github.com/trabucayre/openFPGALoader/blob/master/src/dirtyJtag.cpp (Apache-2.0)
   - https://github.com/kholia/xvcpi/blob/master/xvcpi.c (CC0 1.0 Universal)
   - https://github.com/phdussud/pico-dirtyJtag/blob/master/cmd.c

   See Licensing information at End of File.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BUFFER_SIZE         2048

#ifdef __CYGWIN__
#include <libusb-1.0/libusb.h>
#else
#include <libusb.h>
#endif
#define DIRTYJTAG_VID       0x1209
#define DIRTYJTAG_PID       0xC0CA
#define DIRTYJTAG_INTF      0
#define DIRTYJTAG_READ_EP   0x82
#define DIRTYJTAG_WRITE_EP  0x01
libusb_context *usb_ctx;
libusb_device_handle *dev_handle;

static char xvcInfo[64];

// Note: Modified!
enum dirtyJtagCmd {
  CMD_STOP =  0x00,
  CMD_XFER =  0x03,
  CMD_WRITE = 0x04,
};

/*
  enum libusb_error {
    LIBUSB_SUCCESS             = 0,
    LIBUSB_ERROR_IO            = -1,
    LIBUSB_ERROR_INVALID_PARAM = -2,
    LIBUSB_ERROR_ACCESS        = -3,
    LIBUSB_ERROR_NO_DEVICE     = -4,
    LIBUSB_ERROR_NOT_FOUND     = -5,
    LIBUSB_ERROR_BUSY          = -6,
    LIBUSB_ERROR_TIMEOUT       = -7,
    LIBUSB_ERROR_OVERFLOW      = -8,
    LIBUSB_ERROR_PIPE          = -9,
    LIBUSB_ERROR_INTERRUPTED   = -10,
    LIBUSB_ERROR_NO_MEM        = -11,
    LIBUSB_ERROR_NOT_SUPPORTED = -12,
    LIBUSB_ERROR_OTHER         = -99,
  };
*/

void gpio_send(_Bool header, uint32_t len, uint32_t n, uint8_t *tms, uint8_t *tdi)
{
  unsigned char tx_buffer[64];
  int actual_length, ret, header_offset = 0;

  int bytes = (n + 7) / 8; // 16 or 32

  if (header) {
    // Replace these with memcpy
    tx_buffer[header_offset++] = CMD_XFER;
    tx_buffer[header_offset++] = (len >> 0) & 0xFF;  // uint32_t to bytes
    tx_buffer[header_offset++] = (len >> 8) & 0xFF;
    tx_buffer[header_offset++] = (len >> 16) & 0xFF;
    tx_buffer[header_offset++] = (len >> 24) & 0xFF;
  }

  for (int i = 0; i < bytes; i++) {
    tx_buffer[header_offset++] = tms[i];
    tx_buffer[header_offset++] = tdi[i];
  }

  actual_length = 0;
  ret = libusb_bulk_transfer(dev_handle, DIRTYJTAG_WRITE_EP, tx_buffer, header_offset, &actual_length, 1000);
  if ((ret < 0) || (actual_length != header_offset)) {
    printf("gpio_xfer_full: usb bulk write failed!\n");
    return;
  }
}

void gpio_recieve(uint32_t n, uint8_t *tdo)
{
  unsigned char result[64];
  int actual_length, ret;

  int bytes = (n + 7) / 8; // 16 or 32

  do {
    // Note: For a full-speed device, a bulk packet is limited to 64 bytes!
    // DirtyJTAG USB -> [32597.543687] usb 3-2.3.1: new full-speed USB device number 81 using xhci_hcd
    ret = libusb_bulk_transfer(dev_handle, DIRTYJTAG_READ_EP, result, bytes, &actual_length, 2000);
    if (ret < 0) {
      printf("gpio_xfer_full: usb bulk read failed!\n");
      printf("[Total Bytes] %d, [Return Code] %d [Actual Length] %d\n", bytes, ret, actual_length);
      return;
    }
  } while (actual_length == 0);

  for (int i = 0; i < bytes; i++) {
    tdo[i] = result[i];
  }
  return;
}

int device_init()
{
  int ret;

  if (libusb_init(&usb_ctx) < 0) {
    printf("[ERROR] libusb init failed!\n");
    return -1;
  }
  dev_handle = libusb_open_device_with_vid_pid(usb_ctx, DIRTYJTAG_VID, DIRTYJTAG_PID);
  if (!dev_handle) {
    printf("[ERROR] failed to open usb device!\n");
    libusb_exit(usb_ctx);
    return -1;
  }
  ret = libusb_claim_interface(dev_handle, DIRTYJTAG_INTF);
  if (ret) {
    printf("[!] libusb error while claiming DirtyJTAG interface\n");
    libusb_close(dev_handle);
    libusb_exit(usb_ctx);
    return -1;
  }

  return 0; // success
}

void device_close()
{
  if (dev_handle)
    libusb_close(dev_handle);
  if (usb_ctx)
    libusb_exit(usb_ctx);
}

// This functions handles the following two functions calls:
// - gpio_write(0, 1, 1);
// - gpio_write(0, 1, 0);
// Command Code -> CMD_WRITE
int gpio_write(int tck, int tms, int tdi)
{
  int actual_length;
  uint8_t buf[8];
  u_int buffer_idx = 0;

  buf[buffer_idx++] = CMD_WRITE;
  buf[buffer_idx++] = tck & 1;
  buf[buffer_idx++] = tms & 1;
  buf[buffer_idx++] = tdi & 1;
  buf[buffer_idx++] = CMD_STOP;
  int ret = libusb_bulk_transfer(dev_handle, DIRTYJTAG_WRITE_EP, buf, buffer_idx, &actual_length, 1000);
  if (ret < 0) {
    printf("gpio_write: usb bulk write failed\n");
    return -EXIT_FAILURE;
  }

  return 0;
}

static int verbose = 0;

static int sread(int fd, void *target, int len) {
  unsigned char *t = target;
  while (len) {
    int r = read(fd, t, len);
    if (r <= 0)
      return r;
    t += r;
    len -= r;
  }
  return 1;
}

int handle_data(int fd) {
  uint32_t len, nr_bytes;

  do {
    char cmd[16];
    unsigned char buffer[BUFFER_SIZE], result[BUFFER_SIZE / 2];
    memset(cmd, 0, 16);

    if (sread(fd, cmd, 2) != 1)
      return 1;

    if (memcmp(cmd, "ge", 2) == 0) {
      if (sread(fd, cmd, 6) != 1)
        return 1;
      memcpy(result, xvcInfo, strlen(xvcInfo));
      if (write(fd, result, strlen(xvcInfo)) != (ssize_t)strlen(xvcInfo)) {
        perror("write");
        return 1;
      }
      if (verbose) {
        printf("%u : Received command: 'getinfo'\n", (int)time(NULL));
        printf("\t Replied with %s\n", xvcInfo);
      }
      break;
    } else if (memcmp(cmd, "se", 2) == 0) {
      if (sread(fd, cmd, 9) != 1)
        return 1;
      memcpy(result, cmd + 5, 4);
      if (write(fd, result, 4) != 4) {
        perror("write");
        return 1;
      }
      if (verbose) {
        printf("%u : Received command: 'settck'\n", (int)time(NULL));
        printf("\t Replied with '%.*s'\n\n", 4, cmd + 5);
      }
      break;
    } else if (memcmp(cmd, "de", 2) == 0) {  // DEBUG CODE
      if (sread(fd, cmd, 3) != 1)
        return 1;
      printf("%u : Received command: 'debug'\n", (int)time(NULL));
      gpio_write(1, 1, 1);
      break;
    } else if (memcmp(cmd, "of", 2) == 0) {  // DEBUG CODE
      if (sread(fd, cmd, 1) != 1)
        return 1;
      printf("%u : Received command: 'off'\n", (int)time(NULL));
      gpio_write(0, 0, 0);
      break;
    } else if (memcmp(cmd, "sh", 2) == 0) {
      if (sread(fd, cmd, 4) != 1)
        return 1;
      if (verbose) {
        printf("%u : Received command: 'shift'\n", (int)time(NULL));
      }
    } else {
      fprintf(stderr, "invalid cmd '%s'\n", cmd);
      return 1;
    }

    // Handling for -> "shift:<num bits><tms vector><tdi vector>"
    if (sread(fd, &len, 4) != 1) {
      fprintf(stderr, "reading length failed\n");
      return 1;
    }

    nr_bytes = (len + 7) / 8;
    if (nr_bytes * 2 > sizeof(buffer)) {
      fprintf(stderr, "buffer size exceeded\n");
      return 1;
    }

    if (sread(fd, buffer, nr_bytes * 2) != 1) {
      fprintf(stderr, "reading data failed\n");
      return 1;
    }
    memset(result, 0, nr_bytes);

    if (verbose) {
      printf("\tNumber of Bits  : %d\n", len);
      printf("\tNumber of Bytes : %d \n", nr_bytes);
      printf("\n");
    }

    // Note
    gpio_write(0, 1, 1);

    int bytesLeft = nr_bytes;
    int bitsLeft = len;
    int byteIndex = 0;
    int byteIndexr = 0;
    uint8_t tdi[32], tms[32], tdo[32];
    _Bool header;
    int size;
    int sizer = 0;

    header = 1;

    while (bytesLeft > 0) {
      tms[0] = 0;
      tdi[0] = 0;
      tdo[0] = 0;
      if (header) {
        size = 16;
      } else {
        size = 32;
      }
      if (bytesLeft >= size) {
        memcpy(tms, &buffer[byteIndex], size);
        memcpy(tdi, &buffer[byteIndex + nr_bytes], size);
        gpio_send(header, len, size * 8, tms, tdi);
        if (!header) {
          gpio_recieve(sizer * 8, tdo);
          memcpy(&result[byteIndexr], tdo, sizer);
        }
        sizer = size;
        byteIndexr = byteIndex;
        bytesLeft -= size;
        bitsLeft -= size * 8;
        byteIndex += size;
        header = 0;
      } else {
        memcpy(tms, &buffer[byteIndex], bytesLeft);
        memcpy(tdi, &buffer[byteIndex + nr_bytes], bytesLeft);
        gpio_send(header, len, bitsLeft, tms, tdi);
        if (!header) {
          gpio_recieve(sizer * 8, tdo);
          memcpy(&result[byteIndexr], tdo, sizer);
        }
        gpio_recieve(bitsLeft, tdo);
        memcpy(&result[byteIndex], tdo, bytesLeft);
        break;
      }
    }

    if (bytesLeft == 0) {
      gpio_recieve(sizer * 8, tdo);
      memcpy(&result[byteIndexr], tdo, sizer);
    }

    gpio_write(0, 1, 0);

    if (write(fd, result, nr_bytes) != nr_bytes) {
      perror("write");
      return 1;
    }

  } while (1);
  /* Note: Need to fix JTAG state updates, until then no exit is allowed */
  return 0;
}

int main() {
  int i;
  int s;
  struct sockaddr_in address;

  // Init
  sprintf(xvcInfo, "xvcServer_v1.0:%d\n", BUFFER_SIZE);
  if (device_init() != 0) {
    return -1;
  }

  fprintf(stderr, "XVCPI is listening now!\n");

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    perror("socket");
    device_close();
    return 1;
  }
  i = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(2542);
  address.sin_family = AF_INET;

  if (bind(s, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
    device_close();
    return 1;
  }

  if (listen(s, 0) < 0) {
    perror("listen");
    device_close();
    return 1;
  }

  fd_set conn;
  int maxfd = 0;
  FD_ZERO(&conn);
  FD_SET(s, &conn);
  maxfd = s;

  while (1) {
    fd_set read = conn, except = conn;
    int fd;

    if (select(maxfd + 1, &read, 0, &except, 0) < 0) {
      perror("select");
      break;
    }

    for (fd = 0; fd <= maxfd; ++fd) {
      if (FD_ISSET(fd, &read)) {
        if (fd == s) {
          int newfd;
          socklen_t nsize = sizeof(address);

          newfd = accept(s, (struct sockaddr *)&address, &nsize);
          if (verbose)
            printf("connection accepted - fd %d\n", newfd);
          if (newfd < 0) {
            perror("accept");
          } else {
            int flag = 1;
            int optResult = setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
            if (optResult < 0)
              perror("TCP_NODELAY error");
            if (newfd > maxfd) {
              maxfd = newfd;
            }
            FD_SET(newfd, &conn);
          }
        } else if (handle_data(fd)) {
          if (verbose)
            printf("connection closed - fd %d\n", fd);
          close(fd);
          FD_CLR(fd, &conn);
        }
      } else if (FD_ISSET(fd, &except)) {
        if (verbose)
          printf("connection aborted - fd %d\n", fd);
        close(fd);
        FD_CLR(fd, &conn);
        if (fd == s)
          break;
      }
    }
  }

  device_close();
  return 0;
}

/*
   This work, "xvcpi.c", is a derivative of "xvcServer.c" (https://github.com/Xilinx/XilinxVirtualCable)
   by Avnet and is used by Xilinx for XAPP1251.

   "xvcServer.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/)
   by Avnet and is used by Xilinx for XAPP1251.

   "xvcServer.c", is a derivative of "xvcd.c" (https://github.com/tmbinc/xvcd)
   by tmbinc, used under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/).

   Portions of "xvcpi.c" are derived from OpenOCD (http://openocd.org)

   "xvcpi.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/)
   by Derek Mulcahy.
*/
