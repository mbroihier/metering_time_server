#include "./udp_client_server.h"
#define DEBUG 0
//---------------------------------------------------------------------- */
//
//
// client - UDP metering client.  Collects remote temperature and labels
//          it with a query count and the time day.
//
//    Copyright (C) 2026
//         Mark Broihier
//
//---------------------------------------------------------------------- */
void client() {
  UDP_Client_Server client;
  client.setup_udp_client();
  printf("did setup for client processing\n");
  client.setup_udp_find_service(123);
  if (client.find_server(60)) {  // wait 1 minute for the NTP server
    if (DEBUG) {
      printf("found ntp server\n");
      printf("packetBufferR address: %p\n", client.get_packetBufferR_addr());
      uint8_t * ptr = client.get_packetBufferR_addr();
      for (int i = 0; i < 48; i++) {
        printf("%2.2x ", ptr[i]);
        if (i % 16 == 15) printf("\n");
      }
    }
    // request time
    uint8_t packet[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    client.send_packet(client.get_remote_ip_addr(), 123, 200, packet, 48);
    if (DEBUG) {
      uint8_t * ptr = client.get_packetBufferR_addr();
      for (int i = 0; i < 48; i++) {
        printf("%2.2x ", ptr[i]);
        if (i % 16 == 15) printf("\n");
      }
    }
  }
  uint64_t base_clock = time_us_64();
  uint8_t seconds_buf[4] = {0};
  memcpy(seconds_buf, client.get_packetBufferR_addr()+40, sizeof(seconds_buf));
  uint32_t seconds_since_1900 = seconds_buf[0] << 24 | seconds_buf[1] << 16 | seconds_buf[2] << 8 |
    seconds_buf[3];
  uint32_t hour = (seconds_since_1900 / 3600) % 24;
  uint32_t minute = (seconds_since_1900 / 60) % 60;
  uint32_t second = seconds_since_1900 % 60;
  printf("time from NTP server: %02d:%02d:%02d\n", hour, minute, second);
  client.setup_udp_find_service(UDP_PORT2);
  client.find_server();  // wait here forever if no temperature metering service
  printf("found metering server\n");
  int count = 0;
  int64_t temperature = 0;
  int64_t meter_count = 0;
  ip4_addr_t addr = client.get_remote_ip_addr();
  printf("messages will be sent to %s port %d\n", ip4addr_ntoa(&addr), UDP_PORT2);
  memset(packetBufferT, 0, sizeof(packetBufferT));
  while (true) {
    snprintf(packetBufferT, sizeof(packetBufferT), "%d", count++);
    uint32_t delta = (time_us_64() - base_clock) / 1000000;
    hour = ((seconds_since_1900 + delta) / 3600) % 24;
    minute = ((seconds_since_1900 + delta) / 60) % 60;
    second = (seconds_since_1900 + delta) % 60;
    uint8_t channel = 0;
    printf("Query %s, meter index: %hhu, time: %02d:%02d:%02d", packetBufferT, channel, hour, minute, second);
    packetBufferT[0] = channel;
    client.send_packet(addr, UDP_PORT2, 1000, reinterpret_cast<uint8_t *>(packetBufferT), 1);
    memcpy(&temperature, client.get_packetBufferR_addr(), sizeof(temperature));
    memcpy(&meter_count, client.get_packetBufferR_addr()+8, sizeof(meter_count));
    float t = temperature / 65536.0;
    printf(", temperature: %6.2f C, count: %" PRId64 "\n", t, meter_count);
    sleep_ms(10000);
  }
}
int main() {
  stdio_init_all();
  cyw43_arch_init();
  UDP_Client_Server::setup_wifi();
  printf("in main and have setup the wifi\n");
  sleep_ms(1000);
  client();
  return 0;
}
