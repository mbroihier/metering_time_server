#include <stdio.h>
#include <time.h>
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/util/datetime.h"
#include "hardware/uart.h"
#include "hardware/rtc.h"
#include "ntp_util.h"
#include "TinyGPS.h"
#include "udp_client_server.h"

#define UART_ID uart0
#define BAUD_RATE 4800
#define UART_TX_PIN 0
#define UART_RX_PIN 1

NTP_Util::NTPTime reference = {.seconds = 0, .fraction = 0};

char readchar() {
  if (!uart_is_readable(UART_ID)) {
    do {
      printf("not ready, sleeping\n");
      sleep_ms(50);
    } while (!uart_is_readable(UART_ID));
  }
  char c = uart_getc(UART_ID);
  //printf("Got: %c\n", c);
  return c;
}

void core1_entry() {
  // serial I/O to GPS
  bool state = false;
  bool rtc_never_set = true;
  uint64_t time_base = 0;
  const int LED = CYW43_WL_GPIO_LED_PIN;
  int year;
  uint8_t month, day, hour, minute, second, hundredths;
  uint32_t age;
  TinyGPS gps;
  int count = 0;
  cyw43_arch_gpio_put(LED, state);
  gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
  gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
  int baud = uart_init(UART_ID, BAUD_RATE);
  bool encoder_state = false;
  sleep_ms(5000);
  printf("Initialized - UART set to: %d baud\n", baud);
  datetime_t t = { .year = 2025,
                   .month = 1,
                   .day = 1,
                   .hour = 0,
                   .min = 0,
                   .sec = 0 };
  rtc_init();
  rtc_set_datetime(&t);
  time_base = get_absolute_time();
  reference = NTP_Util::make_reference_time();
  if (uart_is_enabled(UART_ID)) {
    printf("UART is ok\n");
    char output = 'C';
    uart_putc(UART_ID, output);
  } else {
    printf("UART is not enabled!\n");
  }
  sleep_ms(5000);
  char eof[4];
  bool end_of_file = false;
  char c;
  while (! end_of_file) {
    c = readchar();
    eof[0] = eof[1];
    eof[1] = eof[2];
    eof[2] = eof[3];
    eof[3] = c;
    if (c == 0) {
      sleep_ms(500);
    }
    state = ! state;
    cyw43_arch_gpio_put(LED, state);
    if (strncmp(eof, "EOF", 3) == 0) {
      end_of_file = true;
      encoder_state = true;
    } else {
      encoder_state = gps.encode(c);
      if (!encoder_state) continue;
      gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
      if (age == TinyGPS::GPS_INVALID_AGE) {
	printf("********* *********\n");
      } else {
	char ts[32];
	sprintf(ts, "%02d/%02d/%02d %02d:%02d:%02d\n", month, day, year, hour, minute, second);
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
          reference = NTP_Util::make_reference_time();
	} else {
	  datetime_t t;
	  rtc_get_datetime(&t);
	  char ts[32];
	  sprintf(ts, "%02d/%02d/%02d %02d:%02d:%02d expected\n", t.month, t.day, t.year, t.hour, t.min, t.sec);
	  printf("%s", ts);
	  uint64_t new_time_base = get_absolute_time();
	  printf("delta time: %llu\n", new_time_base - time_base);
	  time_base = new_time_base;
	}	  
      }
    }
  }
  printf("end of data detected\n");
  while (true) {
    sleep_ms(5000);
  }
}
int main() {
  cyw43_arch_init();
  stdio_init_all();
  UDP_Client_Server::setup_wifi();
  UDP_Client_Server server;
  server.setup_udp_server();
  server.setup_udp_service_broadcast(123);
  printf("time server setup complete, broadcasting service");
  multicore_launch_core1(core1_entry);
  while (true) {
    server.run();
    printf("Error, should not have come back\n");
  }
  return 0;
}
