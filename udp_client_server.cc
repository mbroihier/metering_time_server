#include "./udp_client_server.h"
#define DEBUG 0
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
//              authentication, and get the local IP when in a station
//              mode
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::setup_wifi() {
  bool not_connected = true;
  uint32_t failed_count = 0;
  cyw43_arch_enable_sta_mode();  // setup as a station
  while (not_connected) {
    not_connected = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000);
    if (not_connected) {
      printf(".");
      sleep_ms(500);
      failed_count++;
      if (failed_count > 5) {
        cyw43_arch_deinit();
        watchdog_enable(20,1);
        while (true);  // let watchdog restart the pico
      }
    }
    if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
      ip_addr_t ip_address = cyw43_state.netif[CYW43_ITF_STA].ip_addr;
      char *ip_str = ip4addr_ntoa(&ip_address);
      printf("Local IP address is: %s\n", ip_str);
    } else {
      printf("Error - IP address is not set\n");
    }
  }
  printf("WIFI Connected!\n");
}
//---------------------------------------------------------------------- */
//
//
// setup_wifi_ap - do WIFI setup for a pico W - this will do the connection,
//                 authentication, and get the local IP when in a AP
//                 mode
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::setup_wifi_ap() {
  const char *ap_name = WIFI_SSID;
  const char *password = WIFI_PASSWORD;

  sleep_ms(4000);
  cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);

  ip4_addr_t mask;
  ip4_addr_t gw;

  IP4_ADDR(&gw, 192, 168, 4, 1);
  IP4_ADDR(&mask, 255, 255, 255, 0);

  dhcp_server_init(&dhcp_server, &gw, &mask);

  dns_server_init(&dns_server, &gw);

  sleep_ms(2000);
  printf("Hotspot '%s' is now active.\n", ap_name);
}
//---------------------------------------------------------------------- */
//
//
// setup_udp_server - does the necessary setup for a UDP server - all
//                    server ports must be setup here
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::setup_udp_server(std::initializer_list<uint16_t> ports) {
  if (DEBUG) printf("entering setup_udp_server, number of entries in service_info: %d\n", service_info.size());
  for (auto port : ports) {
    if (!(service_info[port] = reinterpret_cast<UDP_T*>(calloc(1, sizeof(UDP_T))))) {
      printf("failed to allocate state structure when working on port %d\n", port);
      return;
    }
    if (!(service_info[port]->recv_data.pcb = udp_new_ip_type(IPADDR_TYPE_ANY))) {
      printf("failed to create PCB for port %d\n", port);
      return;
    }
    err_t err =udp_bind(service_info[port]->recv_data.pcb, IP_ADDR_ANY, port);
    if (err != ERR_OK) {
      printf("failed to bind to UDP server port %d, port\n");
      return;
    }
    printf("UDP server port initialization for port %d was successful\n", port);
  }  
  if (DEBUG) printf("exiting setup_udp_server, number of entries in service_info: %d\n", service_info.size());
  return;
}
//---------------------------------------------------------------------- */
//
//
// setup_udp_client - does the necessary setup for a UDP client - client
//                    PCBs should be allocated here
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::setup_udp_client() {
  client_pcb = udp_new();
  return;
}
//---------------------------------------------------------------------- */
//
//
// setup_udp_service_broadcast -     this method supports the
//                                   broadcasting of a UDP server that
//                                   supports a service identified by
//                                   port number
//
//    Copyright (C) 2026
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
  this->service = service;
  return;
}
//---------------------------------------------------------------------- */
//
//
// setup_udp_find_service -     this method supports clients by
//                              finding servers that support the service
//                              they want
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::setup_udp_find_service(uint16_t service) {
  if (!find_state) {
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
  }
  printf("Client is initialized to find services\n");
  this->service = service;
  return;
}

//---------------------------------------------------------------------- */
//
//
// packet_receive - call back used to collect incoming messages
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

  if (DEBUG) {
    if (p->next != 0) {
      printf("pbuf has an unexpected link with value %p\n", p->next);
      printf("p->len for this block is: %d\n", p->len);
    }
  }
  
  if (p) {
    struct pbuf * pbuf_to_free = p;
    int offset = 0;
    ctr->rx_bytes = 0;
    do {
      ctr->rx_bytes += p->len;
      if (ctr->rx_bytes >= UDP_RX_PACKET_MAX_SIZE) {
        int trim = ctr->rx_bytes - UDP_RX_PACKET_MAX_SIZE;
        memcpy(packetBufferR+offset, p->payload, p->len - trim);
        if (DEBUG) printf("Going to mark end of data at location %d - full\n", UDP_RX_PACKET_MAX_SIZE - 1);
        packetBufferR[UDP_RX_PACKET_MAX_SIZE - 1] = 0;
      } else {
        memcpy(packetBufferR+offset, p->payload, p->len);
        if (DEBUG) printf("Going to mark end of data at location %d - offset was %d, len was %d\n",
               p->len+offset, offset, p->len);
        packetBufferR[p->len+offset] = 0;
      }
      offset += p->len;
      p = p->next;

      if (DEBUG) {
        if (p) {
          printf("The next block has %d bytes\n", p->len);
        } else {
          printf("All links traversed\n");
        }
      }
      
    } while (p != 0 && ctr->rx_bytes < UDP_RX_PACKET_MAX_SIZE);
    pbuf_free(pbuf_to_free);
    if (!queue_try_add(&receiveMessageQueue, packetBufferR)) {
      if (DEBUG) printf("adding of message to queue failed\n");
    } else {
      if (DEBUG) printf("added message (%s) to queue\n", packetBufferR);
      if (DEBUG) {
        uint8_t checkMessage[sizeof(packetBufferR)];
        queue_try_remove(&receiveMessageQueue, checkMessage);
        printf("queued message was: %s\n", checkMessage);
        queue_try_add(&receiveMessageQueue, checkMessage);
      }
    }
    if (!queue_try_add(&receiveContextQueue, ctr)) {
      if (DEBUG) printf("adding of context to queue failed\n");
    } else {
      if (DEBUG) printf("added context to queue\n");
    }
  } else {
    printf("empty packet received\n");
    packetBufferR[0] = 0;
  }
}
//---------------------------------------------------------------------- */
//
//
// find_server - find a server for the desired service
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
bool UDP_Client_Server::find_server(uint32_t delay) {
  udp_rxdata context_info;
  memset(&context_info, 0, sizeof(context_info));
  cyw43_arch_lwip_begin();
  udp_recv(find_state->recv_data.pcb, packet_receive, &context_info);  // setup to receive
  cyw43_arch_lwip_end();
  printf("udp ready to receive server broadcasts\n");
  int old_packet_count = context_info.rx_cnt;
  uint32_t working_delay = delay * 100;  // seconds of delay
  bool continue_processing = true;
  while (continue_processing) {
    while (old_packet_count == context_info.rx_cnt) {
      cyw43_arch_lwip_begin();
      cyw43_arch_poll();  // see if there is a udp packet
      cyw43_arch_lwip_end();
      sleep_ms(10);
      if (working_delay) {  // see if we should timeout
        working_delay--;
        if (working_delay == 0) {
          printf("Did not find server for service in time - terminating find.\n");
          udp_remove(find_state->recv_data.pcb);
          return false;
        }
      }
    }
    printf("Packet(s) received, checking for the type of service\n");
    uint32_t packets_to_look_at = context_info.rx_cnt - old_packet_count;
    old_packet_count = context_info.rx_cnt;
    uint8_t localPacketBuffer[sizeof(packetBufferR)];
    udp_rxdata localContext;
    for (int i = 0; i < packets_to_look_at; i++) {
      if (queue_try_remove(&receiveMessageQueue, localPacketBuffer) &&
          queue_try_remove(&receiveContextQueue, &localContext)) {
        if (DEBUG) printf("%s ", localPacketBuffer);
        if (DEBUG) printf(" from remote IP addr %s, port %d, payload length %d\n",
                          ip4addr_ntoa(&context_info.remote_ip_addr),
                          localContext.remote_port,
                          localContext.rx_bytes);
        uint16_t does_this_service = (reinterpret_cast<uint16_t *>(localPacketBuffer))[0];
        if ((service == does_this_service) && (localContext.rx_bytes == 2)){
          remote_address = localContext.remote_ip_addr;
          printf("found a server for %d, exiting find_server\n", service);
          continue_processing = false;
        } else {
          printf("server for %d was seen, looking for %d - packet was size: %d\n", does_this_service,
                 service, localContext.rx_bytes);
        }
      } else {
        printf("There is nothing in the message queue or context queue - this is an internal error\n");
      }
    }
  }
  //udp_remove(find_state->recv_data.pcb);  // since this might be used more than once, do not remove pcb
  udp_recv(find_state->recv_data.pcb, NULL, &context_info);  // take out call back
  return true;
}
//---------------------------------------------------------------------- */
//
// broadcast_services - used by a server that supports a service - broadcasts
//               a list of services that are supported
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::broadcast_services() {
  if (DEBUG) printf("entering broadcast_services, number of entries in service_info: %d\n", service_info.size());
  UDP_T * service_state = reinterpret_cast<UDP_T *>(calloc(1, sizeof(UDP_T)));
  if (!service_state) {
    printf("failed to allocate service state structure\n");
    return;
  }
  service_state->recv_data.pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (!service_state->recv_data.pcb) {
    printf("UDP initialization failed to create PCB for service broadcast\n");
    return;
  }
  for (const auto& [port, tupple] : service_info) {
    
    cyw43_arch_lwip_begin();
    // Note pbuf needs to be allocated with each message sent, but I'm not sure why.  If you don't
    // you get a return code of -2 from upd_sendto on the second call and subsequent calls are weird
    // message lengths.
    struct pbuf *send_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
    send_pbuf->next = 0;
    uint16_t copy_port = port;
    if (DEBUG) printf("current send_pbuf payload address %p\n", &send_pbuf->payload);
    memcpy(send_pbuf->payload, &copy_port, 2);
    send_pbuf->tot_len = 2;
    send_pbuf->len = 2;
    cyw43_arch_lwip_end();
    printf("sending message: %hu\n", port);
    ip4_addr_t remote_ip_address;
    ip4addr_aton("255.255.255.255", &remote_ip_address);
    int retry = 0;
    bool tryAgain = false;
  
    do {
      cyw43_arch_lwip_begin();
      printf("sending packet to %s port %d\n", ip4addr_ntoa(&remote_ip_address), 9720);
      int err = udp_sendto(service_state->recv_data.pcb, send_pbuf, &remote_ip_address, 9720);
      cyw43_arch_poll();  // do a poll to send?
      cyw43_arch_lwip_end();
      printf("sent packet to %s port %d, status: %d\n", ip4addr_ntoa(&remote_ip_address), 9720, err);
      if (err != 0) {  // retry the send
        retry++;
        if (retry > 10) {
          tryAgain = false;
          printf("broadcast failed\n");
          sleep_ms(1);
        } else {
          tryAgain = true;
        }
      } else {
        tryAgain = false;
      }
    } while (tryAgain);
    cyw43_arch_lwip_begin();
    pbuf_free(send_pbuf);
    cyw43_arch_lwip_end();
  }
  cyw43_arch_lwip_begin();
  udp_remove(service_state->recv_data.pcb);
  cyw43_arch_lwip_end();
  free(service_state);
}
//---------------------------------------------------------------------- */
//
// send_packet - used by a client to send a message to a server - generally
//               this method should pick up a reply, too because this is
//               sending to a UDP server implying it wants something
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::send_packet(ip_addr_t remote_ip_address, uint16_t remote_port, uint32_t reply_timeout,
                                    uint8_t *buffer, int buffer_size) {
  udp_rxdata context_info;
  memset(&context_info, 0, sizeof(context_info));
  cyw43_arch_lwip_begin();
  int old_packet_count = context_info.rx_cnt;
  udp_recv(client_pcb, packet_receive, &context_info);  // setup to receive a reply
  cyw43_arch_lwip_end();
  struct pbuf *send_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
  send_pbuf->next = 0;
  memcpy(send_pbuf->payload, buffer, buffer_size);
  send_pbuf->tot_len = buffer_size;
  send_pbuf->len = buffer_size;
  if (DEBUG) printf("sending message of %d bytes", buffer_size);
  cyw43_arch_lwip_begin();
  int err = udp_sendto(client_pcb, send_pbuf, &remote_ip_address, remote_port);
  cyw43_arch_poll();  // do a poll to send?
  cyw43_arch_lwip_end();
  if (DEBUG) printf("sent packet to %s port %d, status: %d\n", ip4addr_ntoa(&remote_ip_address), remote_port,
         err);
  if (reply_timeout) {
    if (DEBUG) printf("packet should get a reply, waiting %d counts of 10 msec\n", reply_timeout);
    do {
      cyw43_arch_lwip_begin();
      cyw43_arch_poll();  // see if there is a udp packet
      cyw43_arch_lwip_end();
      sleep_ms(10);
      if (old_packet_count != context_info.rx_cnt) {
        old_packet_count = context_info.rx_cnt;
        if (remote_port == context_info.remote_port) {
          if (DEBUG) printf("got a reply message of size %d bytes\n", context_info.rx_bytes);
          // got a reply from the expected port
          break;  // return to caller
        } else {
          if (DEBUG) printf("expecting a message from port %d but got one from %d, size %d - rejecting\n",
                            remote_port, context_info.remote_port, context_info.rx_bytes);
        }
      } else {
        if (DEBUG) printf("reply timeout: %d\n", reply_timeout);
      }
    }  while (reply_timeout-- > 1);  // note, timeout is unsigned, do not let it go "negative"
    if (reply_timeout <= 0) {
      printf("no message received in the allowed time\n");
    }
  }
  pbuf_free(send_pbuf);
}
//---------------------------------------------------------------------- */
//
// UDP_Client_Server - contructor of base class - initializes the queues
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
UDP_Client_Server::UDP_Client_Server() {
  printf("Initializing Queues\n");
  queue_init(&receiveMessageQueue, sizeof(packetBufferR), 5);
  queue_init(&receiveContextQueue, sizeof(udp_rxdata), 5);
  if (DEBUG) printf("receiveMessageQueue address: %p\n", &receiveMessageQueue);
}
