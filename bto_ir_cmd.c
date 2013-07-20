#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libusb-1.0/libusb.h>

#define VENDOR_ID  0x22ea
#define PRODUCT_ID 0x001e
#define BTO_EP_IN  0x84
#define BTO_EP_OUT 0x04

#define MAX_SIZE 64
#define IR_DATA_SIZE     7
#define IR_DATA_SIZE_EX 35

#define BTO_CMD_ECHO_BACK             0x41
#define BTO_CMD_GET_DATA              0x50
#define BTO_CMD_SET_RECEIVE_MODE      0x51
#define BTO_CMD_GET_DATA_EX           0x52
#define BTO_CMD_SET_RECEIVE_MODE_EX   0x53
#define BTO_CMD_GET_FIRMWARE_VERSION  0x56
#define BTO_CMD_SET_DATA              0x60
#define BTO_CMD_SET_DATA_EX           0x61

#define RECEIVE_WAIT_MODE_NONE  0
#define RECEIVE_WAIT_MODE_WAIT  1

#define ECHO_BACK    0
#define GET_DATA     1
#define RECEIVE_MODE 2
#define GET_VERSION  3
#define SET_DATA     4

static char bto_commands[] = {
  BTO_CMD_ECHO_BACK,
  BTO_CMD_GET_DATA,
  BTO_CMD_SET_RECEIVE_MODE,
  BTO_CMD_GET_FIRMWARE_VERSION,
  BTO_CMD_SET_DATA
};
static char bto_commands_ex[] = {
  BTO_CMD_ECHO_BACK,
  BTO_CMD_GET_DATA_EX,
  BTO_CMD_SET_RECEIVE_MODE_EX,
  BTO_CMD_GET_FIRMWARE_VERSION,
  BTO_CMD_SET_DATA_EX
};

char get_command(int no, int extend) {
  char cmd;
  if (extend)
    cmd = bto_commands_ex[no];
  else
    cmd = bto_commands[no];
  return cmd;
}

int get_data_length(int extend) {
  if (extend)
    return IR_DATA_SIZE_EX;
  else
    return IR_DATA_SIZE;
}

void close_device(libusb_context *ctx, libusb_device_handle *devh) {
  libusb_close(devh);
  libusb_exit(ctx);
}

libusb_device_handle* open_device(libusb_context *ctx) {
  struct libusb_device_handle *devh = NULL;
  libusb_device *dev;
  libusb_device **devs;

  int r = 1;
  int i = 0;
  int cnt = 0;

  libusb_set_debug(ctx, 3);
  
  if ((libusb_get_device_list(ctx, &devs)) < 0) {
    perror("no usb device found");
    exit(1);
  }

  /* check every usb devices */
  while ((dev = devs[i++]) != NULL) {
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) < 0) {
      perror("failed to get device descriptor\n");
    }
    /* count how many device connected */
    if (desc.idVendor == VENDOR_ID && desc.idProduct == PRODUCT_ID) {
      cnt++;
    }
  }

  /* device not found */
  if (cnt == 0) {
    fprintf(stderr, "device not connected\n");
    exit(1);
  }

  if (cnt > 1) {
    fprintf(stderr, "multi device is not implemented yet\n");
    exit(1);
  }


  /* open device */
  if ((devh = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID)) < 0) {
    perror("can't find device\n");
    close_device(ctx, devh);
    exit(1);
  } 

  /* detach kernel driver if attached. */
  r = libusb_kernel_driver_active(devh, 3);
  if (r == 1) {
    /* detaching kernel driver */
    r = libusb_detach_kernel_driver(devh, 3);
    if (r != 0) {
      perror("detaching kernel driver failed");
      exit(1);
    }
  }

  r = libusb_claim_interface(devh, 3);
  if (r < 0) {
    fprintf(stderr, "claim interface failed (%d): %s\n", r, strerror(errno));
    exit(1);
  }

  return devh;
}

void write_device(struct libusb_device_handle *devh, unsigned char *cmd, int len) {
  int i, r;
  uint8_t buf[MAX_SIZE];

  int size = 0;

  memset(buf, 0xff, sizeof(buf));
  for (i = 0; i < len; i++) {
    buf[i] = cmd[i];
  }
  
  r = libusb_interrupt_transfer(devh, BTO_EP_OUT, buf, sizeof(buf) ,&size, 1000);
  if (r < 0) {
    fprintf(stderr, "libusb_interrupt_transfer (%d): %s\n", r, strerror(errno));
    exit(1);
  }
}

int read_device(struct libusb_device_handle *devh, unsigned char *buf, int bufsize) {
  int size = 0;
  memset(buf, 0x00, bufsize);

  int r = libusb_interrupt_transfer(devh, BTO_EP_IN, buf, bufsize, &size, 1000);
  if (r < 0) {
    fprintf(stderr, "libusb_interrupt_transfer (%d): %s\n", r, strerror(errno));
    exit(1);
  }

  return size;
}

void clear_device_buffer(struct libusb_device_handle *devh) {
  /* clear read buffer */
  unsigned char buf[MAX_SIZE];
  memset(buf, 0xff, sizeof(buf));
  buf[0] = BTO_CMD_ECHO_BACK;
  write_device(devh, buf, MAX_SIZE);
  read_device(devh, buf, MAX_SIZE);
}

int receive_ir(struct libusb_device_handle *devh, unsigned char *data, int length, int extend) {
  unsigned char buf[MAX_SIZE];
  char cmd;
  int i;
  int retval = 0;

  clear_device_buffer(devh);

  memset(buf, 0xff, sizeof(buf));
  cmd = get_command(RECEIVE_MODE, extend);
  buf[0] = cmd;
  buf[1] = RECEIVE_WAIT_MODE_WAIT;
  write_device(devh, buf, MAX_SIZE);
  read_device(devh, buf, MAX_SIZE);

  while (1) {
    memset(buf, 0xFF, sizeof(buf));
    cmd = get_command(GET_DATA, extend);
    buf[0] = cmd;
    write_device(devh, buf, MAX_SIZE);
    read_device(devh, buf, MAX_SIZE);
    if (buf[0] == cmd && buf[1] != 0) {
      for (i = 0; i < length; i++) {
        data[i] = buf[i+1];
      }
      retval = 1;
      break;
    }
  }

  memset(buf, 0xff, sizeof(buf));
  buf[0] = get_command(RECEIVE_MODE, extend);
  buf[1] = RECEIVE_WAIT_MODE_NONE;
  write_device(devh, buf, MAX_SIZE);
  read_device(devh, buf, MAX_SIZE);

  return retval;
}

void transfer_ir(struct libusb_device_handle *devh, char *data, int length, int extend) {
  unsigned char buf[MAX_SIZE];
  int i;
  char *e, s[3] = "\0\0";

  memset(buf, 0xff, MAX_SIZE);
  buf[0] = get_command(SET_DATA, extend);

  // hex to byte
  for (i = 0; i < length; i++) {
    s[0] = data[i*2];
    s[1] = data[i*2+1];
    buf[i+1] = (unsigned char)strtoul(s, &e, 16);
    if (*e != '\0') {
      break;
    }
  }

  if (i != length) {
    fprintf(stderr, "[%s] is skipped..\n", data);
    return;
  }

  write_device(devh, buf, MAX_SIZE);

  clear_device_buffer(devh);
}

void usage() {
  fprintf(stderr, "usage: bto_ir_cmd <option>\n");
  fprintf(stderr, "  -r       \tReceive Infrared code.\n");
  fprintf(stderr, "  -t <code>\tTransfer Infrared code.\n");
  fprintf(stderr, "  -e       \tExtend Infrared code.\n");
  fprintf(stderr, "           \t  Normal:  7 octets\n");
  fprintf(stderr, "           \t  Extend: 35 octets\n");
}

int main(int argc, char *argv[]) {
  libusb_context *ctx = NULL;
  int r = 1;
  int i;
  unsigned char buf[MAX_SIZE];

  int receive_mode  = 0;
  int transfer_mode = 0;
  int extend = 0;
  int ir_data_size;
  char *ir_data;

  while ((r = getopt(argc, argv, "ehrt:")) != -1) {
    switch(r) {
      case 'r':
        receive_mode = 1;
        break;
      case 't':
        transfer_mode = 1;
        ir_data = optarg;
        break;
      case 'e':
        extend = 1;
        printf("Extend mode on.\n");
        break;
      default:
        usage();
        exit(1);
    }
  }

  if (receive_mode == 1 || transfer_mode == 1) {
    /* libusb initialize*/
    if ((r = libusb_init(&ctx)) < 0) {
      perror("libusb_init\n");
      exit(1);
    } 

    /* open device */
    libusb_device_handle *devh = open_device(ctx);

    ir_data_size = get_data_length(extend);

    if (transfer_mode == 1) {
      printf("Transfer mode\n");
      printf("  Transfer code: %s\n", ir_data);
      transfer_ir(devh, ir_data, ir_data_size, extend);
    }
    else if (receive_mode == 1) {
      printf("Receive mode\n");
      memset(buf, 0x00, ir_data_size);
      r = receive_ir(devh, buf, ir_data_size, extend);
      if (r == 1) {
        printf("  Received code: ");
        for (i = 0; i < ir_data_size; i++) {
          printf("%02X", buf[i]);
        }
        printf("\n");
      }
    }

    /* close device */
    close_device(ctx, devh);
  } 
  else {
    usage();
    exit(1);
  }

  return 0;
}
