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
// setup_udp_server - does the necessary setup for a UDP server - all
//                    server ports must be setup here
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::setup_udp_server() {
  ntp_state = reinterpret_cast<UDP_T*>(calloc(1, sizeof(UDP_T)));
  if (!ntp_state) {
    printf("failed to allocate state structure for NTP UDP sessions\n");
    return;
  }
  ntp_state->recv_data.pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  if (!ntp_state->recv_data.pcb) {
    printf("UDP initialization failed to create NTP PCB\n");
    return;
  }
  err_t err = udp_bind(ntp_state->recv_data.pcb, IP_ADDR_ANY, UDP_PORT);
  if (ERR_OK != err) {
    printf("UDP failed to bind to port %d\n", UDP_PORT);
    return;
  }
  printf("Setup of NTP UDP server on port %d was successful\n", UDP_PORT);
  
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
  err = udp_bind(state->recv_data.pcb, IP_ADDR_ANY, UDP_PORT2);
  if (ERR_OK != err) {
    printf("UDP failed to bind\n");
    return;
  }
  printf("Setup of UDP server (metering) on port %d was successful\n", UDP_PORT2);
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
    queue_try_add(&receiveMessageQueue, packetBufferR);
    queue_try_add(&receiveContextQueue, ctr);
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
    printf("Sending NTP service broadcast\n");
    (reinterpret_cast<uint16_t *>(packetBufferT))[0] = UDP_PORT;
    broadcast_service(reinterpret_cast<uint8_t *>(packetBufferT), 2);
    (reinterpret_cast<uint16_t *>(packetBufferT))[0] = 567;
    printf("Sending metering service broadcast\n");
    broadcast_service(reinterpret_cast<uint8_t *>(packetBufferT), 2);
    last_time_broadcast = NTP_Util::now();
  }
  sleep_ms(10);
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
  Meter * meter_singleton = Meter::get_singleton();
  Read_Temperature reader;
  udp_rxdata context_info;
  udp_rxdata ntp_context_info;
  memset(&context_info, 0, sizeof(context_info));
  memset(&ntp_context_info, 0, sizeof(context_info));
  cyw43_arch_lwip_begin();
  udp_recv(state->recv_data.pcb, packet_receive, &context_info);  // setup to receive NTP or metering packets
  udp_recv(ntp_state->recv_data.pcb, packet_receive, &ntp_context_info);  // setup to receive NTP packets
  cyw43_arch_lwip_end();
  printf("udp server ready to receive messages at ports 123 and 567\n");
  int old_packet_count = context_info.rx_cnt;
  int old_ntp_packet_count = ntp_context_info.rx_cnt;
  uint32_t last_time_broadcast = 0;
  NTP_Util::NTPTime packet_receive_time = NTP_Util::make_reference_time();
  int count = 0;
  sleep_ms(5000);  // put in for wifi setup while it is in AP mode
  meter_singleton->reset_count(Meter::TEMPERATURE);
  printf("Entering service loop - examine messages and send information, %lu\n", NTP_Util::now());
  while (true) {
    while ((old_packet_count == context_info.rx_cnt) && (old_ntp_packet_count == ntp_context_info.rx_cnt)) {
      cyw43_arch_lwip_begin();
      cyw43_arch_poll();  // see if there is a udp packet
      cyw43_arch_lwip_end();
      packet_receive_time = NTP_Util::make_reference_time();
      background(last_time_broadcast);
      if (count++ == 1000) {
        printf("Server waiting for message\n");
        count = 0;
        meter_singleton->set_temperature_int(reader.get_temperature_int());
      }
    }
    printf("Service loop detected an incoming message\n");
    if (context_info.rx_cnt != old_packet_count) {
      old_packet_count = context_info.rx_cnt;
      if (context_info.rx_bytes < 48 && packetBufferR[0] == 0) {
        meter_singleton->increment_count(Meter::TEMPERATURE);
        int64_t * packetPtr = reinterpret_cast<int64_t *>(packetBufferT);
        *packetPtr++ = meter_singleton->get_temperature_int();
        *packetPtr++ = meter_singleton->get_count(Meter::TEMPERATURE);
        udp_pcb *tpcb = udp_new();
        struct pbuf *reply_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
        reply_pbuf->next = 0;

        memcpy(reply_pbuf->payload, packetBufferT, 16);
        reply_pbuf->tot_len = 16;
        reply_pbuf->len = 16;

        cyw43_arch_lwip_begin();
        int err = udp_sendto(state->recv_data.pcb, reply_pbuf, &context_info.remote_ip_addr, context_info.remote_port);
        printf("sent packet to %s port %d, status: %d\n", ip4addr_ntoa(&context_info.remote_ip_addr),
               context_info.remote_port, err);
        cyw43_arch_lwip_end();
        pbuf_free(reply_pbuf);
      } else if (context_info.rx_bytes == 48) {  // this is an NTP packet
        NTP_Util::translate_incoming_packet_to_outgoing_packet((NTP_Util::NTPPacket *)packetBufferR,
                                                               (NTP_Util::NTPPacket *)packetBufferT,
                                                               reference, packet_receive_time);
        udp_pcb *tpcb = udp_new();
        struct pbuf *reply_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
        reply_pbuf->next = 0;

        memcpy(reply_pbuf->payload, packetBufferT, sizeof(NTP_Util::NTPPacket));
        reply_pbuf->tot_len = sizeof(NTP_Util::NTPPacket);
        reply_pbuf->len = sizeof(NTP_Util::NTPPacket);

        cyw43_arch_lwip_begin();
        int err = udp_sendto(state->recv_data.pcb, reply_pbuf, &context_info.remote_ip_addr, context_info.remote_port);
        printf("sent packet to %s port %d, status: %d\n", ip4addr_ntoa(&context_info.remote_ip_addr),
               context_info.remote_port, err);
        cyw43_arch_lwip_end();
        pbuf_free(reply_pbuf);
      } else {
        printf("Error in received UDP request - ignoring incoming packet\n");
      }
    } else {  // must be an NTP packet request
      old_ntp_packet_count = ntp_context_info.rx_cnt;
      NTP_Util::translate_incoming_packet_to_outgoing_packet((NTP_Util::NTPPacket *)packetBufferR,
                                                             (NTP_Util::NTPPacket *)packetBufferT,
                                                             reference, packet_receive_time);
      udp_pcb *tpcb = udp_new();
      struct pbuf *reply_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
      reply_pbuf->next = 0;

      memcpy(reply_pbuf->payload, packetBufferT, sizeof(NTP_Util::NTPPacket));
      reply_pbuf->tot_len = sizeof(NTP_Util::NTPPacket);
      reply_pbuf->len = sizeof(NTP_Util::NTPPacket);

      cyw43_arch_lwip_begin();
      int err = udp_sendto(ntp_state->recv_data.pcb, reply_pbuf, &ntp_context_info.remote_ip_addr,
                           ntp_context_info.remote_port);
      printf("sent packet to %s port %d, status: %d\n", ip4addr_ntoa(&ntp_context_info.remote_ip_addr),
             ntp_context_info.remote_port, err);
      cyw43_arch_lwip_end();
      pbuf_free(reply_pbuf);
    }
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
bool UDP_Client_Server::find_server(uint32_t delay) {
  udp_rxdata context_info;
  memset(&context_info, 0, sizeof(context_info));
  cyw43_arch_lwip_begin();
  udp_recv(find_state->recv_data.pcb, packet_receive, &context_info);  // setup to receive
  cyw43_arch_lwip_end();
  printf("udp ready to find service server\n");
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
        if (service == does_this_service) {
          remote_address = localContext.remote_ip_addr;
          printf("found a server for %d, exiting find_server\n", service);
          continue_processing = false;
        }
      }
    }
  }
  udp_remove(find_state->recv_data.pcb);  // remove this pcb so it will not respond to write to port 9720
  return true;
}
//---------------------------------------------------------------------- */
//
// broadcast_service - used by a server that supports a service - broadcasts
//               a list of services that are support (one at the moment)
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void UDP_Client_Server::broadcast_service(uint8_t *buffer, int buffer_size) {
  udp_rxdata context_info;
  memset(&context_info, 0, sizeof(context_info));
  int old_packet_count = context_info.rx_cnt;
  cyw43_arch_lwip_begin();
  struct pbuf *send_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
  cyw43_arch_lwip_end();
  send_pbuf->next = 0;
  memcpy(send_pbuf->payload, buffer, buffer_size);
  send_pbuf->tot_len = buffer_size;
  send_pbuf->len = buffer_size;
  uint16_t service_value = (reinterpret_cast<uint16_t *>(buffer))[0];
  printf("sending message: %hu\n", service_value);
  ip4_addr_t remote_ip_address;
  ip4addr_aton("192.168.4.255", &remote_ip_address);
  int retry = 0;
  bool tryAgain = false;
  
  do {
    cyw43_arch_lwip_begin();
    int err = udp_sendto(service_state->recv_data.pcb, send_pbuf, &remote_ip_address, 9720);
    cyw43_arch_poll();  // do a poll to send?
    cyw43_arch_lwip_end();
    printf("sent packet to %s port %d, status: %d\n", ip4addr_ntoa(&remote_ip_address), 9720,
           err);
    if (err == -16) {  // retry the send
      retry++;
      if (retry > 10) {
        tryAgain = false;
        printf("broadcast failed\n");
        sleep_ms(1);
      } else {
        tryAgain = true;
      }
    }
  } while (tryAgain);
      
  cyw43_arch_lwip_begin();
  pbuf_free(send_pbuf);
  cyw43_arch_lwip_end();
}
//---------------------------------------------------------------------- */
//
// send_packet - used by a client to send a message to a server - generally
//               this method should pick up a reply, too because this is
//               sending to a UDP server.
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
        uint32_t packets_to_look_at = context_info.rx_cnt - old_packet_count;
        uint8_t localPacketBuffer[sizeof(packetBufferR)];
        udp_rxdata localContext;
        for (int i = 0; i < packets_to_look_at; i++) {
          //  purge all packets (should be just one, but...
          if (!queue_try_remove(&receiveMessageQueue, localPacketBuffer) ||
              !queue_try_remove(&receiveContextQueue, &localContext)) {
            printf("remove from queue failure\n");
          }
        }
        // note that this processing will only pick up the last packet
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
uint8_t * UDP_Client_Server::get_packetBufferT_addr() {
  return reinterpret_cast<uint8_t *>(packetBufferT);
}
uint8_t * UDP_Client_Server::get_packetBufferR_addr() {
  return reinterpret_cast<uint8_t *>(packetBufferR);
}
UDP_Client_Server::UDP_Client_Server() {
  queue_init(&receiveMessageQueue, sizeof(packetBufferR), 5);
  queue_init(&receiveContextQueue, sizeof(udp_rxdata), 5);
}
