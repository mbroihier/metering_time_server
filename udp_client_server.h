/*
 *      udp_client_server - pico class for UDP clients and servers
 *
 *      Copyright (C) 2025 
 *          Mark Broihier
 *
 */
/* ---------------------------------------------------------------------- */
#include <time.h>
#include <cstdint>
#include <cstring>
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/udp.h"
#include "ntp_util.h"
#ifndef UDP_CLIENT_SERVER_H_
#define UDP_CLIENT_SERVER_H_

#define UDP_TX_PACKET_MAX_SIZE 10000
#define UDP_RX_PACKET_MAX_SIZE 10000

// buffer for receiving and sending data
static char packetBufferT[UDP_TX_PACKET_MAX_SIZE + 1];
static char packetBufferR[UDP_RX_PACKET_MAX_SIZE + 1];

class UDP_Client_Server {
  //---------------------------------------------------------------------- */
  //
  //
  // udp_client_server - pico class for UDP clients and servers
  //
  //    Copyright (C) 2025
  //         Mark Broihier
  //
  //---------------------------------------------------------------------- */

  struct udp_rxdata {
    uint32_t rx_cnt;
    uint32_t rx_bytes;
    ip_addr_t remote_ip_addr;
    uint16_t remote_port;
    struct udp_pcb *pcb;
  };

  typedef struct UDP_T_ {
    ip_addr_t remote_address;
    ip_addr_t local_address;
    struct udp_rxdata recv_data;
  } UDP_T;

  UDP_T * state;
  UDP_T * service_state;
  UDP_T * find_state;

  uint16_t service;
  ip_addr_t remote_address;

  struct udp_pcb * pcb;
  struct udp_pcb * client_pcb;
  struct udp_pcb * service_pcb;
  struct udp_pcb * find_pcb;

  static void packet_receive(void * arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t * addr,
                             uint16_t port);
  void background(uint32_t &t);

 public:
  static void setup_wifi();
  void setup_udp_server();
  void setup_udp_client();
  void setup_udp_find_service(uint16_t service);
  void setup_udp_service_broadcast(uint16_t service);
  void find_server();
  void run();
  void send_packet(ip_addr_t remote_ip_address, uint16_t remote_port, uint32_t reply_timeout,
                   uint8_t *buffer, int buffer_size);
  void broadcast_service(uint8_t *buffer, int buffer_size);
  void find();
  ip_addr_t get_remote_ip_addr() { return remote_address; }
};
#endif  // UDP_CLIENT_SERVER_H_
