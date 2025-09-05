#include "./udp_client_server.h"
//---------------------------------------------------------------------- */
//
//
// udp_client_server - pico class for UDP clients and servers
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
//---------------------------------------------------------------------- */
//
//
// setup_wifi - do WIFI setup for a pico W - this will do the connection,
//              authentication, and get the local IP
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::setup_wifi() {
  bool not_connected = true;
  cyw43_arch_enable_sta_mode();  // setup as a station
  while (not_connected) {
    not_connected = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000);
    if (not_connected) {
      printf(".");
      sleep_ms(500);
    }
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
      ip_addr_t ip_address = cyw43_state.netif[CYW43_ITF_STA].ip_addr;
      char *ip_str = ip4addr_ntoa(&ip_address);
      printf("Local IP address is: %s\n", ip_str);
    } else {
      printf("Error - IP address is not set\n");
    }
  }
  printf("Connected!\n");
}
//---------------------------------------------------------------------- */
//
//
// setup_udp_server - does the necessary setup for a UDP server - only
//                    one server is supported
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::setup_udp_server() {
  state = reinterpret_cast<UDP_T*>(calloc(1, sizeof(UDP_T)));
  if (!state) {
    printf("failed to allocate state structure for UDP sessions\n");
    return;
  }
  state->recv_data.pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (!state->recv_data.pcb) {
    printf("UDP initialization failed to create PCB\n");
    return;
  }
  err_t err = udp_bind(state->recv_data.pcb, IP_ADDR_ANY, UDP_PORT);
  if (ERR_OK != err) {
    printf("UDP failed to bind\n");
    return;
  }
  return;
}
//---------------------------------------------------------------------- */
//
//
// setup_udp_client - does the necessary setup for a UDP client
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::setup_udp_client() {
  state = reinterpret_cast<UDP_T *>(calloc(1, sizeof(UDP_T)));
  if (!state) {
    printf("failed to allocate state structure for UDP sessions\n");
    return;
  }
  state->recv_data.pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (!state->recv_data.pcb) {
    printf("UDP initialization failed to create PCB\n");
    return;
  }
  client_pcb = udp_new();
  return;
}
//---------------------------------------------------------------------- */
//
//
// setup_udp_udp_service_broadcast - this class object supports the
//                                   broadcasting of a UDP server that
//                                   supports a service identified by
//                                   port number
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::setup_udp_service_broadcast(uint16_t service) {
  service_state = reinterpret_cast<UDP_T *>(calloc(1, sizeof(UDP_T)));
  if (!service_state) {
    printf("failed to allocate service state structure\n");
    return;
  }
  service_state->recv_data.pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (!service_state->recv_data.pcb) {
    printf("UDP initialization failed to create PCB for service broadcast\n");
    return;
  }
  service_pcb = udp_new();
  this->service = service;
  return;
}
//---------------------------------------------------------------------- */
//
//
// setup_udp_udp_find_service - this class object supports clients by
//                              finding servers that support the service
//                              they want
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::setup_udp_find_service(uint16_t service) {
  find_state = reinterpret_cast<UDP_T *>(calloc(1, sizeof(UDP_T)));
  if (!find_state) {
    printf("failed to allocate state structure for UDP find sessions\n");
    return;
  }
  find_state->recv_data.pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (!find_state->recv_data.pcb) {
    printf("UDP initialization failed to create find PCB\n");
    return;
  }
  err_t err = udp_bind(find_state->recv_data.pcb, IP_ADDR_ANY, 9720);
  if (ERR_OK != err) {
    printf("UDP failed to bind find service\n");
    return;
  }
  this->service = service;
  return;
}

//---------------------------------------------------------------------- */
//
//
// packet_receiver - call back used to collect incoming messages
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::packet_receive(void * arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t * addr,
                                       uint16_t port) {
  struct udp_rxdata *ctr = (struct udp_rxdata *)arg;
  ip_addr_copy(ctr->remote_ip_addr, *addr);
  ctr->remote_port = port;
  ctr->rx_cnt++;
  /*
  if (p->next != 0) {
    printf("pbuf has an unexpected link with value %p\n", p->next);
    printf("p->len for this block is: %d\n", p->len);
  }
  */
  if (p) {
    struct pbuf * pbuf_to_free = p;
    int offset = 0;
    ctr->rx_bytes = 0;
    do {
      ctr->rx_bytes += p->len;
      if (ctr->rx_bytes >= UDP_RX_PACKET_MAX_SIZE) {
        int trim = ctr->rx_bytes - UDP_RX_PACKET_MAX_SIZE;
        memcpy(packetBufferR+offset, p->payload, p->len - trim);
        // printf("Going to mark end of data at location %d - full\n", UDP_RX_PACKET_MAX_SIZE - 1);
        packetBufferR[UDP_RX_PACKET_MAX_SIZE - 1] = 0;
      } else {
        memcpy(packetBufferR+offset, p->payload, p->len);
        // printf("Going to mark end of data at location %d - offset was %d, len was %d\n",
        //       p->len+offset, offset, p->len);
        packetBufferR[p->len+offset] = 0;
      }
      offset += p->len;
      p = p->next;
      /*
      if (p) {
	printf("The next block has %d bytes\n", p->len);
      } else {
	printf("All links traversed\n");
      }
      */
    } while (p != 0 && ctr->rx_bytes < UDP_RX_PACKET_MAX_SIZE);
    pbuf_free(pbuf_to_free);
  } else {
    packetBufferR[0] = 0;
  }
}
//---------------------------------------------------------------------- */
//
//
// background - called while waiting for requests to come in to a UDP
//              server
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::background(uint32_t &last_time_broadcast) {
  if (NTP_Util::now() - last_time_broadcast > 30) {
    (reinterpret_cast<uint16_t *>(packetBufferT))[0] = UDP_PORT;
    broadcast_service(reinterpret_cast<uint8_t *>(packetBufferT), 2);
    last_time_broadcast = NTP_Util::now();
  }
  sleep_ms(100);  // started at 10
}
//---------------------------------------------------------------------- */
//
//
// run - run UDP server - this assumes polling
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::run() {
  extern NTP_Util::NTPTime reference;
  udp_rxdata context_info;
  memset(&context_info, 0, sizeof(context_info));
  cyw43_arch_lwip_begin();
  udp_recv(state->recv_data.pcb, packet_receive, &context_info);  // setup to receive
  cyw43_arch_lwip_end();
  printf("udp server ready\n");
  int old_packet_count = context_info.rx_cnt;
  uint32_t last_time_broadcast = 0;
  NTP_Util::NTPTime packet_receive_time = NTP_Util::make_reference_time();
  while (true) {
    while (old_packet_count == context_info.rx_cnt) {
      cyw43_arch_lwip_begin();
      cyw43_arch_poll();  // see if there is a udp packet
      cyw43_arch_lwip_end();
      packet_receive_time = NTP_Util::make_reference_time();
      background(last_time_broadcast);
    }
    old_packet_count = context_info.rx_cnt;
    NTP_Util::translate_incoming_packet_to_outgoing_packet((NTP_Util::NTPPacket *)packetBufferR,
                                                           (NTP_Util::NTPPacket *)packetBufferT,
                                                           reference, packet_receive_time);
    printf("packetBufferT\n");
    uint8_t * p = (uint8_t *)packetBufferT;
    for (int i = 0; i < 48; i++) {
      printf("%2.2x ", *p++);
    }
    printf("packetBufferT\n");
    udp_pcb *tpcb = udp_new();
    struct pbuf *reply_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
    reply_pbuf->next = 0;
    //reply_pbuf->payload = &packetBufferT;
    memcpy(reply_pbuf->payload, packetBufferT, sizeof(NTP_Util::NTPPacket));
    reply_pbuf->tot_len = sizeof(NTP_Util::NTPPacket);
    reply_pbuf->len = sizeof(NTP_Util::NTPPacket);
    printf("to be sent NTP packet of size %d\n", reply_pbuf->tot_len);
    cyw43_arch_lwip_begin();
    udp_sendto(state->recv_data.pcb, reply_pbuf, &context_info.remote_ip_addr, context_info.remote_port);
    cyw43_arch_lwip_end();
    pbuf_free(reply_pbuf);
  }
}
//---------------------------------------------------------------------- */
//
//
// find_server - find a UDP server IP address by receiving a broadcast
//               message that has supported services listed in the incoming
//               payload - when complete, this tears down the pcb looking
//               for the broadcasts
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::find_server() {
  udp_rxdata context_info;
  memset(&context_info, 0, sizeof(context_info));
  cyw43_arch_lwip_begin();
  udp_recv(find_state->recv_data.pcb, packet_receive, &context_info);  // setup to receive
  cyw43_arch_lwip_end();
  printf("udp ready to find service\n");
  int old_packet_count = context_info.rx_cnt;
  while (true) {
    while (old_packet_count == context_info.rx_cnt) {
      cyw43_arch_lwip_begin();
      cyw43_arch_poll();  // see if there is a udp packet
      cyw43_arch_lwip_end();
      sleep_ms(10);
    }
    old_packet_count = context_info.rx_cnt;
    printf("%s ", packetBufferR);
    printf(" from remote IP addr %s, port %d, payload length %d\n",
           ip4addr_ntoa(&context_info.remote_ip_addr),
           context_info.remote_port,
           strlen(packetBufferR));
    uint16_t does_this_service = (reinterpret_cast<uint16_t *>(packetBufferR))[0];
    if (service == does_this_service) {
      remote_address = context_info.remote_ip_addr;
      printf("found a server for this service, exiting find\n");
      break;
    }
  }
  udp_remove(find_state->recv_data.pcb);  // remove this pcb so it will not respond to write to port 9720
}
//---------------------------------------------------------------------- */
//
// broadcast_service - used by a server that supports a service - broadcasts
//               a list of services that are support (one at the moment)
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::broadcast_service(uint8_t *buffer, int buffer_size) {
  udp_rxdata context_info;
  memset(&context_info, 0, sizeof(context_info));
  int old_packet_count = context_info.rx_cnt;
  cyw43_arch_lwip_begin();
  udp_recv(service_pcb, packet_receive, &context_info);  // setup to receive a reply
  cyw43_arch_lwip_end();
  struct pbuf *send_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
  send_pbuf->next = 0;
  memcpy(send_pbuf->payload, buffer, buffer_size);
  send_pbuf->tot_len = buffer_size;
  send_pbuf->len = buffer_size;
  uint16_t service_value = (reinterpret_cast<uint16_t *>(buffer))[0];
  printf("sending message: %hu\n", service_value);
  ip4_addr_t remote_ip_address;
  ip4addr_aton("255.255.255.255", &remote_ip_address);
  cyw43_arch_lwip_begin();
  int err = udp_sendto(service_pcb, send_pbuf, &remote_ip_address, 9720);
  cyw43_arch_poll();  // do a poll to send?
  cyw43_arch_lwip_end();
  printf("sent packet to %s port %d, status: %d\n", ip4addr_ntoa(&remote_ip_address), 9720,
         err);
  pbuf_free(send_pbuf);
}
//---------------------------------------------------------------------- */
//
// send_packet - used by a client to send a message to a server
//
//    Copyright (C) 2025
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::send_packet(ip_addr_t remote_ip_address, uint16_t remote_port, uint32_t reply_timeout,
                                    uint8_t *buffer, int buffer_size) {
  udp_rxdata context_info;
  memset(&context_info, 0, sizeof(context_info));
  int old_packet_count = context_info.rx_cnt;
  cyw43_arch_lwip_begin();
  udp_recv(client_pcb, packet_receive, &context_info);  // setup to receive a reply
  cyw43_arch_lwip_end();
  struct pbuf *send_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
  send_pbuf->next = 0;
  memcpy(send_pbuf->payload, buffer, buffer_size);
  //  send_pbuf->payload = buffer;
  send_pbuf->tot_len = buffer_size;
  send_pbuf->len = buffer_size;
  printf("sending message: %s", send_pbuf->payload);
  cyw43_arch_lwip_begin();
  int err = udp_sendto(client_pcb, send_pbuf, &remote_ip_address, remote_port);
  cyw43_arch_poll();  // do a poll to send?
  cyw43_arch_lwip_end();
  printf("sent packet to %s port %d, status: %d\n", ip4addr_ntoa(&remote_ip_address), remote_port,
         err);
  if (reply_timeout) {
    while ((old_packet_count == context_info.rx_cnt) && (reply_timeout-- > 0)) {
      cyw43_arch_lwip_begin();
      cyw43_arch_poll();  // see if there is a udp packet
      cyw43_arch_lwip_end();
      sleep_ms(10);
    }
    if (old_packet_count != context_info.rx_cnt) {
      old_packet_count = context_info.rx_cnt;
      printf("%s ", packetBufferR);
      printf(" from remote IP addr %s, port %d, payload length %d\n",
             ip4addr_ntoa(&context_info.remote_ip_addr),
             context_info.remote_port,
             strlen(packetBufferR));
    }
  }
  pbuf_free(send_pbuf);
}
