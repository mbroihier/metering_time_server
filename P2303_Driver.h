/*
 *      P2303_Driver - pico class for USB driver to a P2303 serial to USB
 *
 *      Copyright (C) 2025 
 *          Mark Broihier
 *
 */
/* ---------------------------------------------------------------------- */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/cyw43_arch.h"
#include "pico/mutex.h"
#include "pico/util/queue.h"
#include "bsp/board_api.h"
#include "tusb.h"

#ifndef P2303_DRIVER_H_
#define P2303_DRIVER_H_


class P2303_Driver {
  //---------------------------------------------------------------------- */
  //
  //
  // P2303_Driver - pico class for USB driver to a P2303 device - this is a
  //                USB host class
  //
  //    Copyright (C) 2025
  //         Mark Broihier
  //
  //---------------------------------------------------------------------- */

#define BUF_COUNT 4
 private:
  uint8_t buf_pool[BUF_COUNT][64];
  uint8_t buf_owner[BUF_COUNT] = { 0 }; // device address that owns buffer
  mutex_t lock;
  bool processed = false;
  void _convert_utf16le_to_utf8(const uint16_t* utf16, size_t utf16_len, uint8_t* utf8, size_t utf8_len);
  int _count_utf8_bytes(const uint16_t* buf, size_t len);
  uint16_t count_interface_total_len(tusb_desc_interface_t const* desc_itf, uint8_t itf_count, uint16_t max_len);
  void print_utf16(uint16_t* temp_buf, size_t buf_len);
  void init_device(uint8_t dev_addr);
  void open_vendor_interface(uint8_t daddr, tusb_desc_interface_t const *desc_itf, uint16_t max_len);
  void parse_config_descriptor(uint8_t dev_addr, tusb_desc_configuration_t const* desc_cfg);
  uint8_t * get_buf(uint8_t dev_addr);
  uint8_t * free_buf(uint8_t dev_addr);
  static void vendor_report_received(tuh_xfer_t * xfer);
  static void control_xfer_cb(tuh_xfer_t * xfer);
  void send_control_message(uint8_t dev_addr, uint8_t bRequestType, uint8_t bRequest, uint16_t wValue,
                            uint16_t wIndex, uint16_t wLength);


 public:
  struct queue_info {
    uint32_t buffer_size;
    uint8_t * buffer;
  };
  queue_t queue;
  uint8_t * buffer;
  uint32_t buffer_size;
  uint32_t buffer_index;
  static P2303_Driver * singleton;
  char readChar(void);
  static P2303_Driver * get_singleton() { return singleton; };
  void wrapped_tuh_mount_cb(uint8_t dev_addr);
  P2303_Driver();
};
#endif  // P2303_DRIVER_H_
