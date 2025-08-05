#include <stdio.h>
#include <time.h>
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/uart.h"
#include "TinyGPS.h"

#define UART_ID uart0
#define BAUD_RATE 4800
#define UART_TX_PIN 0
#define UART_RX_PIN 1

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

int main() {
  bool state = false;
  const int LED = CYW43_WL_GPIO_LED_PIN;
  cyw43_arch_init();
  stdio_init_all();
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
      }
    }
  }
  printf("end of data detected\n");
  while (true) {
    sleep_ms(5000);
  }
  return 0;
}
