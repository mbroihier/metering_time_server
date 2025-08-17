#include <cstdint>
#include <cstring>
#include <time.h>
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "TinyGPS.h"
#ifndef NTP_UTIL_H_
#define NTP_UTIL_H_

#define NTP_get_leap_indicator(value)    ((value>>6)&0x03)
#define NTP_get_version(value)  ((value>>3)&0x07)
#define NTP_get_mode(value)  (value&0x07)

#define NTP_set_leap_indicator(value)    ((value&0x03)<<6)
#define NTP_set_version(value)  ((value&0x07)<<3)
#define NTP_set_mode(value)  ((value&0x07))
#define UDP_TX_PACKET_MAX_SIZE 4096

#define LI_NONE         0
#define LI_SIXTY_ONE    1
#define LI_FIFTY_NINE   2
#define LI_NOSYNC       3

#define MODE_RESERVED   0
#define MODE_ACTIVE     1
#define MODE_PASSIVE    2
#define MODE_CLIENT     3
#define MODE_SERVER     4
#define MODE_BROADCAST  5
#define MODE_CONTROL    6
#define MODE_PRIVATE    7

#define NTP_VERSION     4

#define REF_ID          "GPS "

#define YEARS70   2208988800L
#define to_UNIX_epoch(t)      ((uint32_t)t-YEARS70) // 1970
#define to_NTP_epoch(t)        ((uint32_t)t+YEARS70) // 1900

class NTP_Util {
public:
  typedef struct ntp_time
  {
    uint32_t seconds;
    uint32_t fraction;
  } NTPTime;

  // NTP data structures
  typedef struct ntp_packet
  {
    uint8_t  flags;
    uint8_t  stratum;
    uint8_t  poll;
    int8_t   precision;
    uint32_t delay;
    uint32_t dispersion;
    uint8_t  ref_id[4];
    NTPTime  ref_time;
    NTPTime  orig_time;
    NTPTime  recv_time;
    NTPTime  xmit_time;
  } NTPPacket;

  static uint32_t now();
  static uint64_t millis();
  static NTPTime make_reference_time();

  static void translate_incoming_packet_to_outgoing_packet(NTPPacket * in, NTPPacket * out,
                                                           NTPTime reference, NTPTime packet_receive_time);
};
#endif
