#include <stdio.h>
#include <time.h>
#include "pico/cyw43_arch.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "pico/util/datetime.h"
#include "pico/util/queue.h"
#include "hardware/uart.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/rtc.h"
#include "ntp_util.h"
#include "TinyGPSPlus.h"
#include "udp_client_server.h"
#include "Read_Temperature.h"
#include "Meter.h"

#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1

uint8_t cs(char c, uint8_t oldCS) {
  return oldCS ^= c;
}
static queue_t queue;
static bool full = false;

// RX interrupt handler
void on_uart_rx() {
  char r = 'X';
    while (uart_is_readable(UART_ID)) {
      uint8_t ch = uart_getc(UART_ID);
      if (!queue_try_add(&queue, &ch)) {
        full = true;
      } else {
        full = false;
      }
    }
}
  
NTP_Util::NTPTime reference = {.seconds = 0, .fraction = 0};
bool rtc_never_set = true;
bool rtc_ready = false;
void core1_entry() {
  uint64_t time_base = 0;
  const int LED = CYW43_WL_GPIO_LED_PIN;
  int16_t year;
  int8_t month, day, hour, minute, second, hundredths;
  uint32_t age;
  TinyGPSPlus gpsD;
  sleep_ms(5000);
  datetime_t t = { .year = 2025,
                   .month = 1,
                   .day = 1,
                   .hour = 0,
                   .min = 0,
                   .sec = 0 };
  gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
  gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
  int baud = uart_init(UART_ID, BAUD_RATE);
  bool encoder_state = false;
  sleep_ms(5000);
  printf("Initialized - UART set to: %d baud\n", baud);
  int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

  // And set up and enable the interrupt handlers
  irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
  irq_set_enabled(UART_IRQ, true);

  if (uart_is_enabled(UART_ID)) {
    printf("UART is ok\n");
    uart_putc(UART_ID, '\n');
    uart_putc(UART_ID, '\n');
  } else {
    printf("UART is not enabled!\n");
  }
  rtc_init();
  rtc_set_datetime(&t);
  time_base = get_absolute_time();
  reference = NTP_Util::make_reference_time();
  sleep_ms(5000);
  char eof[4];
  bool end_of_file = false;
  char c = 'A';
  int ready_count = 0;
  // Now enable the UART to send interrupts - RX only
  uart_set_irq_enables(UART_ID, true, false);
  while (! end_of_file) {
    while (!queue_try_remove(&queue, &c)) {
      sleep_us(150);  // tweak load on system by resting between messages
      //printf("-");
    }
    if (full) {
      printf("gps input buffer full!!\n");
    }
    //printf("%c", c);
    eof[0] = eof[1];
    eof[1] = eof[2];
    eof[2] = eof[3];
    eof[3] = c;
    if (strncmp(eof, "EOF", 3) == 0) {
      end_of_file = true;
      encoder_state = true;
    } else {
      encoder_state = gpsD.encode(c);
      if (!encoder_state) continue;
      if (gpsD.time.isUpdated() && gpsD.date.isUpdated()) {
	char ts[32];
        year = gpsD.date.year();
        month = gpsD.date.month();
        day = gpsD.date.day();
        hour = gpsD.time.hour();
        minute = gpsD.time.minute();
	second = gpsD.time.second();
	sprintf(ts, "%02d/%02d/%02d %02d:%02d:%02d\n", month, day, year % 100, hour, minute, second);
	printf("%s", ts);
	if (rtc_never_set) {
	  rtc_never_set = false;
	  datetime_t t = { .year = year,
                           .month = month,
			   .day = day,
			   .hour = hour,
			   .min = minute,
			   .sec = second };
	  rtc_set_datetime(&t);
	  time_base = get_absolute_time();
          sleep_us(64);  // delay long enough for hardware to be updated
          reference = NTP_Util::make_reference_time();
	} else {
	  datetime_t t;
	  rtc_get_datetime(&t);
	  char ts[32];
	  sprintf(ts, "%02d/%02d/%02d %02d:%02d:%02d expected\n", t.month, t.day, t.year % 100, t.hour, t.min, t.sec);
	  printf("%s", ts);
	  uint64_t new_time_base = get_absolute_time();
          uint64_t delta_time = new_time_base - time_base;
	  printf("delta time: %llu\n", delta_time);
	  time_base = new_time_base;
          if (t.month == month && t.day == day && t.year == year && t.hour == hour && t.min == minute &&
              t.sec == second) {
            ready_count++;
            rtc_ready |= ready_count >= 3;
          } else {
            ready_count = 0;
            if (delta_time > 800000 && delta_time < 1200000) { // if about every second
              datetime_t t = { .year = year,
                               .month = month,
                               .day = day,
                               .hour = hour,
                               .min = minute,
                               .sec = second };
              rtc_set_datetime(&t);  // reset rtc
              sleep_us(64);  // delay long enough for hardware to be updated
              reference = NTP_Util::make_reference_time();
              printf("rtc updated\n");
            }
          } 
	}	  
      }
    }
  }
  printf("end of data detected - this should not happen unless we are in a simulation mode\n");
  while (true) {
    sleep_ms(5000);
  }
}

queue_t UDP_Client_Server::receiveMessageQueue;
queue_t UDP_Client_Server::receiveContextQueue;
char UDP_Client_Server::packetBufferT[UDP_TX_PACKET_MAX_SIZE + 1];
char UDP_Client_Server::packetBufferR[UDP_RX_PACKET_MAX_SIZE + 1];

class UDP_Server : public UDP_Client_Server {
public:
  void background(uint64_t &t) {
    if ((time_us_64() - t) > 30000000) {
      broadcast_services();
      t = time_us_64();
    }
    sleep_ms(100);
  }
  void run() {
    extern NTP_Util::NTPTime reference;
    Meter * meter_singleton = Meter::get_singleton();
    Read_Temperature reader;
    udp_pcb * receivedByPCB[2];
    udp_rxdata context_info[2];  // two ports being serviced
    memset(&context_info, 0, sizeof(context_info));
    cyw43_arch_lwip_begin();
    int index = 0;
    for (const auto& [port, tupple] : service_info) {
      receivedByPCB[index] = service_info[port]->recv_data.pcb;
      udp_recv(service_info[port]->recv_data.pcb, packet_receive, &context_info[index++]);  // setup to receive
    }
    cyw43_arch_lwip_end();
    printf("udp server ready to receive messages at ports 123 and 567\n");
    int old_packet_count = context_info[1].rx_cnt;
    int old_ntp_packet_count = context_info[0].rx_cnt;
    uint64_t last_time_broadcast = 0;
    NTP_Util::NTPTime packet_receive_time = NTP_Util::make_reference_time();
    int count = 0;
    sleep_ms(5000);  // put in for wifi setup while it is in AP mode
    meter_singleton->reset_count(Meter::TEMPERATURE);
    printf("Entering service loop - examine messages and send information, %lu\n", NTP_Util::now());
    while (true) {
      while ((old_packet_count == context_info[1].rx_cnt) && (old_ntp_packet_count == context_info[0].rx_cnt)) {
        cyw43_arch_lwip_begin();
        cyw43_arch_poll();  // see if there is a udp packet
        cyw43_arch_lwip_end();
        packet_receive_time = NTP_Util::make_reference_time();
        background(last_time_broadcast);
        if (count++ == 1000) {
          printf("Server waiting for message\n");
          count = 0;
        }
      }
      meter_singleton->set_temperature_int(reader.get_temperature_int());
      printf("Service loop detected an incoming message\n");
      if (context_info[1].rx_cnt != old_packet_count) {
        old_packet_count = context_info[1].rx_cnt;
        if (context_info[1].rx_bytes < 48 && packetBufferR[0] == 0) {
          meter_singleton->increment_count(Meter::TEMPERATURE);
          int64_t * packetPtr = reinterpret_cast<int64_t *>(packetBufferT);
          *packetPtr++ = meter_singleton->get_temperature_int();
          *packetPtr++ = meter_singleton->get_count(Meter::TEMPERATURE);
          struct pbuf *reply_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
          reply_pbuf->next = 0;

          memcpy(reply_pbuf->payload, packetBufferT, 16);
          reply_pbuf->tot_len = 16;
          reply_pbuf->len = 16;

          cyw43_arch_lwip_begin();
          int err = udp_sendto(receivedByPCB[1], reply_pbuf, &context_info[1].remote_ip_addr,
                               context_info[1].remote_port);
          printf("sent packet to %s port %d, status: %d\n", ip4addr_ntoa(&context_info[1].remote_ip_addr),
                 context_info[1].remote_port, err);
          cyw43_arch_lwip_end();
          pbuf_free(reply_pbuf);
        } else if (context_info[1].rx_bytes == 48) {  // this is an NTP packet
          NTP_Util::translate_incoming_packet_to_outgoing_packet((NTP_Util::NTPPacket *)packetBufferR,
                                                                 (NTP_Util::NTPPacket *)packetBufferT,
                                                                 reference, packet_receive_time);
          struct pbuf *reply_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
          reply_pbuf->next = 0;

          memcpy(reply_pbuf->payload, packetBufferT, sizeof(NTP_Util::NTPPacket));
          reply_pbuf->tot_len = sizeof(NTP_Util::NTPPacket);
          reply_pbuf->len = sizeof(NTP_Util::NTPPacket);

          cyw43_arch_lwip_begin();
          int err = udp_sendto(receivedByPCB[0], reply_pbuf, &context_info[0].remote_ip_addr,
                               context_info[0].remote_port);
          printf("sent packet to %s port %d, status: %d\n", ip4addr_ntoa(&context_info[0].remote_ip_addr),
                 context_info[0].remote_port, err);
          cyw43_arch_lwip_end();
          pbuf_free(reply_pbuf);
        } else {
          printf("Error in received UDP request - ignoring incoming packet\n");
        }
      } else {  // must be an NTP packet request
        old_ntp_packet_count = context_info[0].rx_cnt;
        NTP_Util::translate_incoming_packet_to_outgoing_packet((NTP_Util::NTPPacket *)packetBufferR,
                                                               (NTP_Util::NTPPacket *)packetBufferT,
                                                               reference, packet_receive_time);
        struct pbuf *reply_pbuf = pbuf_alloc(PBUF_TRANSPORT, sizeof(packetBufferT), PBUF_RAM);
        reply_pbuf->next = 0;

        memcpy(reply_pbuf->payload, packetBufferT, sizeof(NTP_Util::NTPPacket));
        reply_pbuf->tot_len = sizeof(NTP_Util::NTPPacket);
        reply_pbuf->len = sizeof(NTP_Util::NTPPacket);

        cyw43_arch_lwip_begin();
        int err = udp_sendto(receivedByPCB[0], reply_pbuf, &context_info[0].remote_ip_addr,
                             context_info[0].remote_port);
        printf("sent packet to %s port %d, status: %d\n", ip4addr_ntoa(&context_info[0].remote_ip_addr),
               context_info[0].remote_port, err);
        cyw43_arch_lwip_end();
        pbuf_free(reply_pbuf);
      }
    }
  }
};


#define PIO0 0
#define PIO1 1

Meter * Meter::singleton = 0;

int main() {
  stdio_init_all();
  sleep_ms(2000);
  if (cyw43_arch_init()) {
    printf("failed to initialize CYW43 architecture\n");
    return 1;
  }
  queue_init(&queue, 1, 4096);
  UDP_Server server;    // make a metering/NTP server
  server.setup_wifi_ap();
  int count = 0;
  Read_Temperature reader;
  Meter * meter_storage = Meter::get_singleton();

  printf("UDP server created\n");
  server.setup_udp_server({UDP_PORT, UDP_PORT2});   // do the setup
  printf("UDP server setup complete\n");
  server.setup_udp_service_broadcast(123);  // broadcast its location on the local network
  printf("broadcast of services 123 and 567 setup complete\n");
  multicore_launch_core1(core1_entry);
  while (true) {
    while(!rtc_ready) {
      sleep_ms(5);
    }
    printf("Starting NTP and Metering server\n");
    server.run();
    printf("Error, should not have come back\n");
  }
  return 0;
}
