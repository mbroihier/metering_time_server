#include <stdio.h>
#include <time.h>
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "pico/util/datetime.h"
#include "hardware/uart.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/rtc.h"
#include "ntp_util.h"
#include "TinyGPSPlus.h"
#include "udp_client_server.h"


#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_TX_PIN 0
#define UART_RX_PIN 1

uint8_t cs(char c, uint8_t oldCS) {
  return oldCS ^= c;
}

static char gps_input[4096];
static char * head = &gps_input[0];
static char * tail = &gps_input[0];
static bool full = false;
static char * const end_of_buffer = &gps_input[4096];
// RX interrupt handler
void on_uart_rx() {
    while (uart_is_readable(UART_ID)) {
        uint8_t ch = uart_getc(UART_ID);
        *head++ = ch;
        if (head == tail) full = true;
        if (head == end_of_buffer) {
          head = &gps_input[0];
        }
    }
}
  
char readchar() {
  if (!uart_is_readable(UART_ID)) {
    do {
      //printf("not ready, sleeping\n");
      sleep_ms(50);
    } while (!uart_is_readable(UART_ID));
  }
  
  char c = uart_getc(UART_ID);
  printf("%c", c);
  return c;
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

  // Now enable the UART to send interrupts - RX only
  uart_set_irq_enables(UART_ID, true, false);
  if (uart_is_enabled(UART_ID)) {
    printf("UART is ok\n");
    uart_putc(UART_ID, '\n');
    uart_putc(UART_ID, '\n');
    /*
    char * command = "$PCAS04,7*";
    uint8_t sum = 0;
    for (int i = 0; i < strlen(command); i++) {
      if (command[i] != '$' && command[i] != '*') {
        sum = cs(command[i], sum);
      }
    }
    char fullCommand[128];
    int endOfCommand = sprintf(fullCommand, "%s%2.2X\n", command, sum);
    fullCommand[endOfCommand] = 0;
    printf("Sending %s to GPS\n");
    uart_puts(UART_ID, fullCommand);
    command = "$PCAS01,1*";
    sum = 0;
    for (int i = 0; i < strlen(command); i++) {
      if (command[i] != '$' && command[i] != '*') {
        sum = cs(command[i], sum);
      }
    }
    endOfCommand = sprintf(fullCommand, "%s%2.2X\n", command, sum);
    fullCommand[endOfCommand] = 0;
    */
    //printf("Sending %s to GPS\n");
    //uart_puts(UART_ID, fullCommand);
    //baud = uart_init(UART_ID, 9600);
    //sleep_ms(1000);
    //printf("ReInitialized - UART set to: %d baud\n", baud);
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
  char c;
  int ready_count = 0;
  while (! end_of_file) {
    while (!full && head == tail) {
      sleep_us(64);
    }
    if (full) {
      printf("gps input buffer full!!\n");
    }
    c = *tail++;
    //printf("%c", c);
    if (tail == end_of_buffer) {
      tail = &gps_input[0];
    }
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
int main() {
  stdio_init_all();
  cyw43_arch_init();
  UDP_Client_Server::setup_wifi();
  UDP_Client_Server server;    // make a NTP server
  server.setup_udp_server();   // do the setup
  server.setup_udp_service_broadcast(123);  // broadcast its location on the local network
  printf("time server setup complete\n");
  multicore_launch_core1(core1_entry);
  while (true) {
    while(!rtc_ready) {
      sleep_ms(5);
    }
    printf("Starting NTP server\n");
    server.run();
    printf("Error, should not have come back\n");
  }
  return 0;
}
