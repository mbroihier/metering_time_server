  //---------------------------------------------------------------------- */
  //
  //
  // P2303_Driver - pico class for USB driver to a P2303 device - this is a
  //                USB host class.  It is largely derived from tinyusb
  //                examples and I've therefore included the license below.
  //
  //    Copyright (C) 2025
  //         Mark Broihier
  //
  //---------------------------------------------------------------------- */
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
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
 *
 */

#include "P2303_Driver.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+
#define LANGUAGE_ID 0x0409
//---------------------------------------------------------------------- */
//
//
// _convert_utf16le_to_utf8 
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void P2303_Driver::_convert_utf16le_to_utf8(const uint16_t* utf16, size_t utf16_len, uint8_t* utf8,
                                                   size_t utf8_len) {
  // TODO: Check for runover.
  (void) utf8_len;
  // Get the UTF-16 length out of the data itself.

  for (size_t i = 0; i < utf16_len; i++) {
    uint16_t chr = utf16[i];
    if (chr < 0x80) {
      *utf8++ = chr & 0xffu;
    } else if (chr < 0x800) {
      *utf8++ = (uint8_t) (0xC0 | (chr >> 6 & 0x1F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 0 & 0x3F));
    } else {
      // TODO: Verify surrogate.
      *utf8++ = (uint8_t) (0xE0 | (chr >> 12 & 0x0F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 6 & 0x3F));
      *utf8++ = (uint8_t) (0x80 | (chr >> 0 & 0x3F));
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
}

//---------------------------------------------------------------------- */
//
//
// _count_utf8_bytes 
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
// Count how many bytes a utf-16-le encoded string will take in utf-8.
int P2303_Driver::_count_utf8_bytes(const uint16_t* buf, size_t len) {
  size_t total_bytes = 0;
  for (size_t i = 0; i < len; i++) {
    uint16_t chr = buf[i];
    if (chr < 0x80) {
      total_bytes += 1;
    } else if (chr < 0x800) {
      total_bytes += 2;
    } else {
      total_bytes += 3;
    }
    // TODO: Handle UTF-16 code points that take two entries.
  }
  return (int) total_bytes;
}
//---------------------------------------------------------------------- */
//
//
// print_utf16 
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void P2303_Driver::print_utf16(uint16_t* temp_buf, size_t buf_len) {
  if ((temp_buf[0] & 0xff) == 0) return;  // empty
  size_t utf16_len = ((temp_buf[0] & 0xff) - 2) / sizeof(uint16_t);
  size_t utf8_len = (size_t) _count_utf8_bytes(temp_buf + 1, utf16_len);
  _convert_utf16le_to_utf8(temp_buf + 1, utf16_len, (uint8_t*) temp_buf, sizeof(uint16_t) * buf_len);
  ((uint8_t*) temp_buf)[utf8_len] = '\0';

  printf("%s", (char*) temp_buf);
}
uint16_t P2303_Driver::count_interface_total_len(tusb_desc_interface_t const* desc_itf, uint8_t itf_count, uint16_t max_len)
{
  uint8_t const* p_desc = (uint8_t const*) desc_itf;
  uint16_t len = 0;

  while (itf_count--)
  {
    // Next on interface desc
    len += tu_desc_len(desc_itf);
    p_desc = tu_desc_next(p_desc);

    while (len < max_len)
    {
      // return on IAD regardless of itf count
      if ( tu_desc_type(p_desc) == TUSB_DESC_INTERFACE_ASSOCIATION ) return len;

      if ( (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE) &&
           ((tusb_desc_interface_t const*) p_desc)->bAlternateSetting == 0 )
      {
        break;
      }

      len += tu_desc_len(p_desc);
      p_desc = tu_desc_next(p_desc);
    }
  }

  return len;
}
//---------------------------------------------------------------------- */
//
//
// get_buf - get a buffer from a pool 
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
uint8_t* P2303_Driver::get_buf(uint8_t daddr)
{
  for(size_t i=0; i<BUF_COUNT; i++)
  {
    if (buf_owner[i] == 0)
    {
      buf_owner[i] = daddr;
      return buf_pool[i];
    }
  }

  // out of memory, increase BUF_COUNT
  return NULL;
}

//---------------------------------------------------------------------- */
//
//
// free_buf - free a buffer to the pool
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
uint8_t * P2303_Driver::free_buf(uint8_t daddr)
{
  for(size_t i=0; i<BUF_COUNT; i++)
  {
    if (buf_owner[i] == daddr) buf_owner[i] = 0;
  }
  return 0;
}

//---------------------------------------------------------------------- */
//
//
// vendor_report_received - call back routine for bluk USB input
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void P2303_Driver::vendor_report_received(tuh_xfer_t* xfer)
{
  // Note: not all field in xfer is available for use (i.e filled by tinyusb stack) in callback to save sram
  // For instance, xfer->buffer is NULL. We have used user_data to store buffer when submitted callback
  uint8_t* buf = (uint8_t*) xfer->user_data;

  if (xfer->result == XFER_RESULT_SUCCESS) {
    struct queue_info data;
    data.buffer = (uint8_t *) malloc(xfer->actual_len);
    if (data.buffer == 0) {
      printf("out of memory\n");
    } else {
      memcpy(data.buffer, buf, xfer->actual_len);
      data.buffer_size = xfer->actual_len;
      if (!queue_try_add(&P2303_Driver::get_singleton()->queue, &data)) {
        printf("queue full\n");
      }
    }
  } else {
    printf("report request failed\n");
  }

  // continue to submit transfer, with updated buffer
  // other field remain the same
  xfer->buflen = 64;
  xfer->buffer = buf;

  tuh_edpt_xfer(xfer);
}

//---------------------------------------------------------------------- */
//
//
// control_xfer_cb - call back when control transfer request has completed
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void P2303_Driver::control_xfer_cb(tuh_xfer_t* xfer) {
  P2303_Driver * singleton = get_singleton();
  if (xfer->result != XFER_RESULT_SUCCESS) {
    printf("Control transfer failed\n");
  } else {
    printf("Control request processed successfully\n");
  }
  mutex_enter_blocking(&singleton->lock);
  singleton->processed = true;
  mutex_exit(&singleton->lock);
}

//---------------------------------------------------------------------- */
//
//
// send_control_message - send USB control message to device
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void P2303_Driver::send_control_message(uint8_t dev_addr, uint8_t bRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint16_t wLength) {
  uint8_t buf[64];
  memset(buf, 0, sizeof(buf));
  //0 1 2 3 4 5 6 
  //80250000000008  // this was 9600 baud
  buf[0] = 0xc0;
  buf[1] = 0x12;  // direct method - 4800 Baud
  buf[2] = 0x00;
  buf[3] = 0x00;
  buf[4] = 0x00;
  buf[5] = 0x00;
  buf[6] = 0x08;

    
  tuh_xfer_t setup_packet =
      {
        .daddr       = dev_addr,
        .ep_addr     = 0x00,
        .buflen      = 7,
        .buffer      = buf,
        .complete_cb = control_xfer_cb,
        .user_data   = (uintptr_t) buf, // since buffer is not available in callback, use user data to store the buffer
      };
  tusb_control_request_t control_request;
  control_request.bmRequestType = bRequestType;
  control_request.bRequest = bRequest;
  control_request.wValue = wValue;
  control_request.wIndex = wIndex;
  control_request.wLength = wLength;
  setup_packet.setup = &control_request;
  bool status = tuh_control_xfer(&setup_packet) == true; 
  if (status) {
    printf("Control %s transfer queued, waiting for completion\n", (bRequestType & TUSB_DIR_IN_MASK) ? "IN" : "OUT");
    mutex_enter_blocking(&lock);
    processed = false;
    mutex_exit(&lock);
    bool done = false;
    do {
      tuh_task();
      mutex_enter_blocking(&lock);
      done = processed;
      mutex_exit(&lock);
      sleep_ms(2);
    } while (!done);
  } else {
    printf("Control IN transfer failed to queue\n");
  }
}
//---------------------------------------------------------------------- */
//
//
// init_device - initialize the P2303 USB device
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void P2303_Driver::init_device(uint8_t dev_addr) {
  send_control_message(dev_addr, 0x00, 9, 0x0001, 0x0000, 0);
  uint8_t buf[64];
  // the followin sequence of control messages comes from pl2303.c
  send_control_message(dev_addr, 0xc0, 1, 0x8484, 0x0000, 1);
  send_control_message(dev_addr, 0x40, 1, 0x0404, 0x0000, 0);
  send_control_message(dev_addr, 0xc0, 1, 0x8484, 0x0000, 1);
  send_control_message(dev_addr, 0xc0, 1, 0x8383, 0x0000, 1);
  send_control_message(dev_addr, 0xc0, 1, 0x8484, 0x0000, 1);
  send_control_message(dev_addr, 0x40, 1, 0x0404, 0x0001, 0);
  send_control_message(dev_addr, 0xc0, 1, 0x8484, 0x0000, 1);
  send_control_message(dev_addr, 0xc0, 1, 0x8383, 0x0000, 1);
  send_control_message(dev_addr, 0x40, 1, 0x0000, 0x0001, 0);
  send_control_message(dev_addr, 0x40, 1, 0x0001, 0x0000, 0);
  send_control_message(dev_addr, 0x40, 1, 0x0002, 0x0044, 0);
  //interface settings (?)
  send_control_message(dev_addr, 0x40, 1, 0x0008, 0x0000, 0);
  send_control_message(dev_addr, 0x40, 1, 0x0009, 0x0000, 0);
  send_control_message(dev_addr, 0xa1, 0x21, 0x0000, 0x0000, 7);
  send_control_message(dev_addr, 0x21, 0x20, 0x0000, 0x0000, 7);
  send_control_message(dev_addr, 0xa1, 0x21, 0x0000, 0x0000, 7); // read it back
  send_control_message(dev_addr, 0xc0, 1, 0x0080, 0x0000, 1);
  send_control_message(dev_addr, 0x40, 1, 0x0000, 0x00c1, 0);
}
//---------------------------------------------------------------------- */
//
//
// open_vendor_interface - initialize the device, process the endpoint descriptors
//                         for this device, and prepare driver to receive bulk
//                         packets.
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void P2303_Driver::open_vendor_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len)
{
  printf("opening interface\n");
  //init_device(daddr);
  uint8_t const *p_desc = (uint8_t const *) desc_itf;

  // Vendor descriptor
  //p_desc = tu_desc_next(p_desc);

  // Endpoint descriptor
  p_desc = tu_desc_next(p_desc);
  tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;
  printf("Number of endpoints: %d\n", desc_itf->bNumEndpoints);

  printf("Now open all of the valid bulk input endpoint\n");
  for(int i = 0; i < desc_itf->bNumEndpoints; i++)
  {
    printf("bLength: %d\n", desc_ep->bLength);
    printf("bDescriptorType: %2.2x\n", desc_ep->bDescriptorType);
    printf("bEndpointAddress: %2.2x\n", desc_ep->bEndpointAddress);
    printf("wMaxPacketSize: %4.4x\n", desc_ep->wMaxPacketSize);
    printf("bInterval: %d\n", desc_ep->bInterval);
    if(tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN &&
       desc_ep->bEndpointAddress == 0x83)
    {
      // skip if failed to open endpoint
      if ( ! tuh_edpt_open(daddr, desc_ep) ) {
        printf("failed to open endpoint %d\n", i);
        return;
      }

      uint8_t* buf = get_buf(daddr);
      if (!buf) {
        printf("out of memory\n");
        return; // out of memory
      }

      tuh_xfer_t xfer =
      {
        .daddr       = daddr,
        .ep_addr     = desc_ep->bEndpointAddress,
        .buflen      = 64,
        .buffer      = buf,
        .complete_cb = vendor_report_received,
        .user_data   = (uintptr_t) buf, // since buffer is not available in callback, use user data to store the buffer
      };

      printf("setting up to receive reports from endpoint %2.2x\n", desc_ep->bEndpointAddress);
      // submit transfer for this EP
      tuh_edpt_xfer(&xfer);
      printf("Listening to [dev %u: ep %02x]\r\n", daddr, desc_ep->bEndpointAddress);
    } else {
      printf("Endpoint %2.2x is an output\n", desc_ep->bEndpointAddress);
    }
    p_desc = tu_desc_next(p_desc);
    printf("Debug next p_desc: %p\n", p_desc);
    desc_ep = (tusb_desc_endpoint_t const *) p_desc;
  }
  init_device(daddr);
} 
//---------------------------------------------------------------------- */
//
//
// parse_configure_descriptor 
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void P2303_Driver::parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const* desc_cfg)
{
  uint8_t const* desc_end = ((uint8_t const*) desc_cfg) + tu_le16toh(desc_cfg->wTotalLength);
  uint8_t const* p_desc   = tu_desc_next(desc_cfg);

  printf("Debug desc_end: %p\n", desc_end);
  printf("Debug p_desc: %p\n", p_desc);
  // parse each interfaces
  while( p_desc < desc_end )
  {
    uint8_t assoc_itf_count = 1;

    // Class will always starts with Interface Association (if any) and then Interface descriptor
    if ( TUSB_DESC_INTERFACE_ASSOCIATION == tu_desc_type(p_desc) )
    {
      tusb_desc_interface_assoc_t const * desc_iad = (tusb_desc_interface_assoc_t const *) p_desc;
      assoc_itf_count = desc_iad->bInterfaceCount;

      p_desc = tu_desc_next(p_desc); // next to Interface
      printf("saw an interface association, advancing p_desc: %p\n", p_desc);
    }

    // must be interface from now
    if( TUSB_DESC_INTERFACE != tu_desc_type(p_desc) ) {
      printf("Not an interface from now\n");
      return;
    }
    tusb_desc_interface_t const* desc_itf = (tusb_desc_interface_t const*) p_desc;

    uint16_t const drv_len = count_interface_total_len(desc_itf, assoc_itf_count, (uint16_t) (desc_end-p_desc));

    // probably corrupted descriptor
    if(drv_len < sizeof(tusb_desc_interface_t)) {
      printf("Corrupted interface\n");
      return;
    }
    // only open and listen to PL2303 endpoint IN
    if (desc_itf->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC)
    {
      open_vendor_interface(dev_addr, desc_itf, drv_len);
    } else {
      printf("Interface class: %2.2x\n", desc_itf->bInterfaceClass);
    }

    // next Interface or IAD descriptor
    p_desc += drv_len;  // ???
    printf("going to next interface at p_desc: %p\n", p_desc);
  }
  printf("done parsing, no errors dectected\n");
}


//---------------------------------------------------------------------- */
//
//
// wrapped_tuh_mount_cb - implements the real call back usually processed
//                        by tuh_mount_cb which is generic
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void P2303_Driver::wrapped_tuh_mount_cb(uint8_t dev_addr) {
  CFG_TUH_MEM_SECTION struct {
    TUH_EPBUF_TYPE_DEF(tusb_desc_device_t, device);
    TUH_EPBUF_DEF(serial, 64*sizeof(uint16_t));
    TUH_EPBUF_DEF(buf, 128*sizeof(uint16_t));
  } desc;
  // application set-up
  printf("A device with address %d is mounted\r\n", dev_addr);
  uint8_t xfer_result = tuh_descriptor_get_device_sync(dev_addr, &desc.device, 18);
  if (xfer_result != XFER_RESULT_SUCCESS) {
    printf("failed to get descriptr\n");
    return;
  }
  printf("Device %u: ID %04x:%04x SN ", dev_addr, desc.device.idVendor, desc.device.idProduct);

  xfer_result = XFER_RESULT_FAILED;
  if (desc.device.iSerialNumber != 0) {
    xfer_result = tuh_descriptor_get_serial_string_sync(dev_addr, LANGUAGE_ID, desc.serial, sizeof(desc.serial));
  }
  if (XFER_RESULT_SUCCESS != xfer_result) {
    uint16_t* serial = (uint16_t*)(uintptr_t) desc.serial;
    serial[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * 3 + 2));
    serial[1] = 'n';
    serial[2] = '/';
    serial[3] = 'a';
    serial[4] = 0;
  }
  print_utf16((uint16_t*)(uintptr_t) desc.serial, sizeof(desc.serial)/2);
  printf("\r\n");

  printf("Device Descriptor:\r\n");
  printf("  bLength             %u\r\n", desc.device.bLength);
  printf("  bDescriptorType     %u\r\n", desc.device.bDescriptorType);
  printf("  bcdUSB              %04x\r\n", desc.device.bcdUSB);
  printf("  bDeviceClass        %u\r\n", desc.device.bDeviceClass);
  printf("  bDeviceSubClass     %u\r\n", desc.device.bDeviceSubClass);
  printf("  bDeviceProtocol     %u\r\n", desc.device.bDeviceProtocol);
  printf("  bMaxPacketSize0     %u\r\n", desc.device.bMaxPacketSize0);
  printf("  idVendor            0x%04x\r\n", desc.device.idVendor);
  printf("  idProduct           0x%04x\r\n", desc.device.idProduct);
  printf("  bcdDevice           %04x\r\n", desc.device.bcdDevice);

  // Get String descriptor using Sync API

  printf("  iManufacturer       %u     ", desc.device.iManufacturer);
  if (desc.device.iManufacturer != 0) {
    xfer_result = tuh_descriptor_get_manufacturer_string_sync(dev_addr, LANGUAGE_ID, desc.buf, sizeof(desc.buf));
    if (XFER_RESULT_SUCCESS == xfer_result) {
      print_utf16((uint16_t*)(uintptr_t) desc.buf, sizeof(desc.buf)/2);
    }
  }
  printf("\r\n");

  printf("  iProduct            %u     ", desc.device.iProduct);
  if (desc.device.iProduct != 0) {
    xfer_result = tuh_descriptor_get_product_string_sync(dev_addr, LANGUAGE_ID, desc.buf, sizeof(desc.buf));
    if (XFER_RESULT_SUCCESS == xfer_result) {
      print_utf16((uint16_t*)(uintptr_t) desc.buf, sizeof(desc.buf)/2);
    }
  }
  printf("\r\n");

  printf("  iSerialNumber       %u     ", desc.device.iSerialNumber);
  printf((char*)desc.serial); // serial is already to UTF-8
  printf("\r\n");

  printf("  bNumConfigurations  %u\r\n", desc.device.bNumConfigurations);

  uint16_t temp_buf[128];
  if (XFER_RESULT_SUCCESS == tuh_descriptor_get_configuration_sync(dev_addr, 0, temp_buf, sizeof(temp_buf)))
  {
    printf("Calling parse config descriptor NEW\n");
    parse_config_descriptor(dev_addr, (tusb_desc_configuration_t*) temp_buf);
  }

}
//---------------------------------------------------------------------- */
//
//
// readChar - reads a character from the input queue
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
char P2303_Driver::readChar(void) {
  struct  queue_info data;
  if (buffer_index == buffer_size) {
    if (buffer) {
      free(buffer);
      buffer = 0;
    }
    if (queue_try_remove(&queue, &data)) {
      buffer_index = 0;
      buffer_size = data.buffer_size;
      buffer = data.buffer;
    } else {
      return(0xfe);
    }
  }
  return buffer[buffer_index++];
}
//---------------------------------------------------------------------- */
//
//
// P2303_Driver - constructor
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
P2303_Driver::P2303_Driver (void) {
   singleton = this;
   mutex_init(&lock);
   buffer_index = 0;
   buffer_size = 0;
   buffer = 0;
   queue_init(&queue, sizeof(struct queue_info), 256);
}
//--------------------------------------------------------------------+
// TinyUSB Callbacks - must be external to the class
//--------------------------------------------------------------------+
void tuh_mount_cb(uint8_t dev_addr) {
  printf("This is the way into the new driver\n");
  P2303_Driver * singleton = P2303_Driver::get_singleton();
  singleton->wrapped_tuh_mount_cb(dev_addr);
}
void tuh_umount_cb(uint8_t dev_addr) {
  // application tear-down
  printf("A device with address %d is unmounted \r\n", dev_addr);
}

