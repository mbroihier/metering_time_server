/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "udp_client_server.h"

typedef struct NTP_T_ {
    ip_addr_t ntp_server_address;
    struct udp_pcb *ntp_pcb;
    absolute_time_t ntp_test_time;
    alarm_id_t ntp_resend_alarm;
} NTP_T;

#define NTP_SERVER "pool.ntp.org"
#define NTP_MSG_LEN 48
#define NTP_PORT 123
#define NTP_DELTA 2208988800 // seconds between 1 Jan 1900 and 1 Jan 1970
#define NTP_TEST_TIME (30 * 1000)
#define NTP_RESEND_TIME (10 * 1000)

// Called with results of operation
static void ntp_result(NTP_T* state, int status, time_t *result) {
    if (status == 0 && result) {
      uint32_t t = *result;
      struct tm *utc = gmtime(result);
      printf("got ntp response: %02d/%02d/%04d %02d:%02d:%02d\n", utc->tm_mday, utc->tm_mon + 1, utc->tm_year + 1900,
             utc->tm_hour, utc->tm_min, utc->tm_sec);
    }

    if (state->ntp_resend_alarm > 0) {
        cancel_alarm(state->ntp_resend_alarm);
        state->ntp_resend_alarm = 0;
    }
    state->ntp_test_time = make_timeout_time_ms(NTP_TEST_TIME);
}

static int64_t ntp_failed_handler(alarm_id_t id, void *user_data);

// Make an NTP request
static void ntp_request(NTP_T *state) {
    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    uint8_t *req = (uint8_t *) p->payload;
    memset(req, 0, NTP_MSG_LEN);
    req[0] = 0x1b;
    udp_sendto(state->ntp_pcb, p, &state->ntp_server_address, NTP_PORT);
    pbuf_free(p);
    cyw43_arch_lwip_end();
}

static int64_t ntp_failed_handler(alarm_id_t id, void *user_data)
{
    NTP_T* state = (NTP_T*)user_data;
    printf("ntp request failed\n");
    ntp_result(state, -1, NULL);
    return 0;
}

// NTP data received
static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    NTP_T *state = (NTP_T*)arg;
    uint8_t mode = pbuf_get_at(p, 0) & 0x7;
    uint8_t stratum = pbuf_get_at(p, 1);

    // Check the result
    if (ip_addr_cmp(addr, &state->ntp_server_address) && port == NTP_PORT && p->tot_len == NTP_MSG_LEN &&
        mode == 0x4 && stratum != 0) {
        uint8_t seconds_buf[4] = {0};
        pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);
        uint32_t seconds_since_1900 = seconds_buf[0] << 24 | seconds_buf[1] << 16 | seconds_buf[2] << 8 | seconds_buf[3];
        //uint32_t seconds_since_1900 = ntohl((uint32_t)seconds_buf);
        uint32_t seconds_since_1970 = seconds_since_1900 - NTP_DELTA;
        time_t epoch = seconds_since_1970;
        ntp_result(state, 0, &epoch);
    } else {
      char *ip_str = ip4addr_ntoa(addr);
      printf("invalid ntp response from %s length %d, mode %d, stratum %d\n", ip_str, p->tot_len, mode, stratum);
        ntp_result(state, -1, NULL);
    }
    pbuf_free(p);
}

// Perform initialisation
static NTP_T* ntp_init(void) {
    NTP_T *state = (NTP_T*)calloc(1, sizeof(NTP_T));
    if (!state) {
        printf("failed to allocate state\n");
        return NULL;
    }
    state->ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!state->ntp_pcb) {
        printf("failed to create pcb\n");
        free(state);
        return NULL;
    }
    udp_recv(state->ntp_pcb, ntp_recv, state);
    return state;
}

// Runs ntp test forever
void run_ntp_test(ip_addr_t remote_ip_address) {
    NTP_T *state = ntp_init();
    if (!state)
        return;
    state->ntp_server_address = remote_ip_address;
    ntp_request(state);  //  do initial NTP request
    bool message_sent = true;
    uint64_t next_time_to_send = make_timeout_time_ms(30000);
    uint64_t now = make_timeout_time_ms(0);
    printf("next time to send (ms) is %llu, current time (ms) is %llu\n",
           next_time_to_send, now);
    cyw43_arch_poll();  // send the request
    printf("NTP request sent\n");
    while(true) {
      now = make_timeout_time_ms(0);
      if (now > next_time_to_send) {
        ntp_request(state);
        printf("NTP request will be sent on next poll\n");
        next_time_to_send = make_timeout_time_ms(30000);
        printf("next time to send (ms) is %llu, current time (ms) is %llu\n",
               next_time_to_send, now);
      }
      sleep_ms(100);
      cyw43_arch_poll(); 
    }
    free(state);
}

int main() {
    stdio_init_all();

    if (cyw43_arch_init()) {
      sleep_ms(5000);
      printf("failed to initialize\n");
      while(true) {
	sleep_ms(1000);
      }
    }

    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
      sleep_ms(5000);
      printf("failed to connect\n");
      while(true) {
	sleep_ms(1000);
      }
    }
    UDP_Client_Server client;
    client.setup_udp_find_service(123);
    client.find_server();
    printf("found ntp server\n");
    run_ntp_test(client.get_remote_ip_addr());
    cyw43_arch_deinit();
    printf("done????\n");
    while(true) {
      sleep_ms(1000);
    }
   return 0;
}
