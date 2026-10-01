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
  
  const char *ap_name = WIFI_SSID;
  const char *password = WIFI_PASSWORD;
  const uint PORT = UDP_PORT;

  cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);

  ip4_addr_t mask;
  ip4_addr_t gw;

  IP4_ADDR(&gw, 192, 168, 4, 1);
  IP4_ADDR(&mask, 255, 255, 255, 0);

  dhcp_server_t dhcp_server;
  dhcp_server_init(&dhcp_server, &gw, &mask);

  dns_server_t dns_server;
  dns_server_init(&dns_server, &gw);

  sleep_ms(2000);
  printf("Hotspot '%s' is now active.\n", ap_name);

  int count = 0;
  Read_Temperature reader;
  Meter * meter_storage = Meter::get_singleton();

  UDP_Client_Server server;    // make a metering/NTP server
  printf("UDP server created\n");
  server.setup_udp_server();   // do the setup
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
