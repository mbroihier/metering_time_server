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
#include <initializer_list>
#include <map>
#include "dhcpserver.h"
#include "dnsserver.h"
#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include "pico/util/datetime.h"
#include "pico/util/queue.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/udp.h"

#ifndef UDP_CLIENT_SERVER_H_
#define UDP_CLIENT_SERVER_H_

#define UDP_TX_PACKET_MAX_SIZE 1000
#define UDP_RX_PACKET_MAX_SIZE 1000


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
 protected:
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

  UDP_T * state = 0;
  UDP_T * ntp_state = 0;
  UDP_T * service_state = 0;
  UDP_T * find_state = 0;

  uint16_t service;
  ip_addr_t remote_address;

  dhcp_server_t dhcp_server;
  dns_server_t dns_server;

  struct udp_pcb * client_pcb;
  struct udp_pcb * client_ntp_pcb;
  struct udp_pcb * service_pcb;
  struct udp_pcb * find_pcb;

  std::map<uint16_t, UDP_T *> service_info;
  static void packet_receive(void * arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t * addr,
                             uint16_t port);
  virtual void background(uint64_t &t) = 0;

 public:
  // buffer for receiving and sending data
  static char packetBufferT[UDP_TX_PACKET_MAX_SIZE + 1];
  static char packetBufferR[UDP_RX_PACKET_MAX_SIZE + 1];
  // queues of received message information
  static queue_t receiveMessageQueue;
  static queue_t receiveContextQueue;
  static void setup_wifi();
  void setup_wifi_ap();
  void setup_udp_server(std::initializer_list<uint16_t> ports);
  void setup_udp_client();
  void setup_udp_find_service(uint16_t service);
  void setup_udp_service_broadcast(uint16_t service);
  bool find_server(uint32_t delay = 0);
  virtual void run() = 0;
  void send_packet(ip_addr_t remote_ip_address, uint16_t remote_port, uint32_t reply_timeout,
                   uint8_t *buffer, int buffer_size);
  void broadcast_services();
  ip_addr_t get_remote_ip_addr() { return remote_address; }
  UDP_Client_Server();
};
#endif  // UDP_CLIENT_SERVER_H_
