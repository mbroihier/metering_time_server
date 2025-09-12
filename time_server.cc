#include <stdio.h>
#include <time.h>
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
//#include "pico/stdio_usb.h"
#include "pico/util/datetime.h"
#include "hardware/uart.h"
#include "hardware/rtc.h"
#include "ntp_util.h"
#include "TinyGPS.h"
#include "udp_client_server.h"
#include "P2303_Driver.h"

#define UART_ID uart0
#define BAUD_RATE 4800
#define UART_TX_PIN 0
#define UART_RX_PIN 1

NTP_Util::NTPTime reference = {.seconds = 0, .fraction = 0};

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  printf("Run time error, this call back should never be triggered.  This is only only here due to an unsatisfied"
         " reference in host hidh_xfer_cb\n");
  return;
}

bool rtc_never_set = true;
bool rtc_ready = false;
void core1_entry() {
  // process USB characters from a P2303 device attached to a GPS 
  bool state = true;
  uint64_t time_base = 0;
  const int LED = CYW43_WL_GPIO_LED_PIN;
  int year;
  uint8_t month, day, hour, minute, second, hundredths;
  uint32_t age;
  TinyGPS gpsD;
  cyw43_arch_gpio_put(LED, state);
  bool encoder_state = false;
  sleep_ms(5000);
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
  sleep_ms(5000);
  char eof[4];
  bool end_of_file = false;
  char c;
  P2303_Driver * gpsPtr = P2303_Driver::get_singleton();
  int ready_count = 0;
  while (! end_of_file) {
    do {
      //tuh_task();
      c = gpsPtr->readChar();
    } while (c == 0xfe);  // spin here if we have no data
    eof[0] = eof[1];
    eof[1] = eof[2];
    eof[2] = eof[3];
    eof[3] = c;
    state = ! state;
    cyw43_arch_gpio_put(LED, state);
    if (strncmp(eof, "EOF", 3) == 0) {
      end_of_file = true;
      encoder_state = true;
    } else {
      //printf("%c %2.2x", c, c);
      //printf("%c", c);
      encoder_state = gpsD.encode(c);
      if (!encoder_state) continue;
      gpsD.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
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
          uint64_t delta_time = new_time_base - time_base;
	  printf("delta time: %llu\n", delta_time);
	  time_base = new_time_base;
          if (t.month == month && t.day == day && t.year == year && t.hour == hour && t.min == minute &&
              t.sec == second) {
            ready_count++;
            rtc_ready |= ready_count >= 3;
          } else {
            ready_count = 0;
            if (delta_time > 197000 && delta_time < 2200000) { // if not stale
              datetime_t t = { .year = year,
                               .month = month,
                               .day = day,
                               .hour = hour,
                               .min = minute,
                               .sec = second };
              rtc_set_datetime(&t);  // reset rtc
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
// initialize the class variable that contains the singleton address
P2303_Driver * P2303_Driver::singleton = 0;
int main() {
  cyw43_arch_init();
  stdio_init_all();
  UDP_Client_Server::setup_wifi();
  P2303_Driver gps_driver;  // init driver
  tuh_init(BOARD_TUH_RHPORT);  // once the driver is instantiated, we can start the USB host stack
  UDP_Client_Server server;    // make a NTP server
  server.setup_udp_server();   // do the setup
  server.setup_udp_service_broadcast(123);  // broadcast its location on the local network
  printf("time server setup complete\n");
  multicore_launch_core1(core1_entry);
  while (true) {
    while(!rtc_ready) {
      tuh_task();  // accept messages until realtime clock is set, then we can process NTP messages
      sleep_ms(5);
    }
    printf("Starting NTP server\n");
    server.run();
    printf("Error, should not have come back\n");
  }
  return 0;
}
