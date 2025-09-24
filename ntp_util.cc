#include "ntp_util.h"
uint64_t NTP_Util::millis() {
  return to_ms_since_boot(get_absolute_time());
}

uint32_t NTP_Util::now() {
  datetime_t t;
  rtc_get_datetime(&t);
  struct tm timeinfo = {0};

  timeinfo.tm_year = t.year - 1900;  // oddly, this will be in Unix epoch seconds
  timeinfo.tm_mon = t.month - 1;
  timeinfo.tm_mday = t.day;
  timeinfo.tm_hour = t.hour;
  timeinfo.tm_min = t.min;
  timeinfo.tm_sec = t.sec;
  uint32_t n = mktime(&timeinfo);
  return to_NTP_epoch(n);
}

NTP_Util::NTPTime NTP_Util::make_reference_time() {
  NTPTime r;
  r.seconds = now();
  r.fraction = 0;
  return r;
}

void NTP_Util::translate_incoming_packet_to_outgoing_packet(NTPPacket *in, NTPPacket *out,
                                                            NTPTime reference, NTPTime packet_receive_time) {
  NTPPacket ntp;
  memcpy(&ntp, in, sizeof(ntp));
  NTPPacket ntp_reply;
  // Deal with network byte order
  ntp_reply.delay              = ntohl(ntp.delay);
  ntp_reply.dispersion         = ntohl(ntp.dispersion);
  ntp_reply.orig_time.seconds  = ntohl(ntp.orig_time.seconds);
  ntp_reply.orig_time.fraction = ntohl(ntp.orig_time.fraction);
  ntp_reply.ref_time.seconds   = ntohl(ntp.ref_time.seconds);
  ntp_reply.ref_time.fraction  = ntohl(ntp.ref_time.fraction);
  ntp_reply.recv_time.seconds  = ntohl(ntp.recv_time.seconds);
  ntp_reply.recv_time.fraction = ntohl(ntp.recv_time.fraction);
  ntp_reply.xmit_time.seconds  = ntohl(ntp.xmit_time.seconds);
  ntp_reply.xmit_time.fraction = ntohl(ntp.xmit_time.fraction);

  ntp_reply.flags = NTP_set_leap_indicator(LI_NONE) | NTP_set_version(NTP_VERSION) | NTP_set_mode(MODE_SERVER);
  ntp_reply.stratum  = 1;
  ntp_reply.precision  = 0; // 1 second???
  // TODO: compute actual root delay, and root dispersion
  ntp_reply.delay = 1;      //(uint32)(0.000001 * 65536.0);
  ntp_reply.dispersion = 1; //(uint32_t)(_gps.getDispersion() * 65536.0); // TODO: pre-calculate this?
  strncpy((char*)ntp_reply.ref_id, REF_ID, sizeof(in->ref_id));
  ntp_reply.orig_time.seconds = ntp_reply.xmit_time.seconds;     // WHY?
  ntp_reply.orig_time.fraction = ntp_reply.xmit_time.fraction;
  ntp_reply.recv_time.seconds  = packet_receive_time.seconds;
  ntp_reply.recv_time.fraction  = packet_receive_time.fraction;
  ntp_reply.ref_time.seconds = reference.seconds;
  ntp_reply.ref_time.fraction = reference.fraction;
  ntp_reply.xmit_time.seconds = now();
  ntp_reply.xmit_time.fraction = 0;
  
  ntp_reply.delay              = htonl(ntp_reply.delay);
  ntp_reply.dispersion         = htonl(ntp_reply.dispersion);
  ntp_reply.orig_time.seconds  = htonl(ntp_reply.orig_time.seconds);
  ntp_reply.orig_time.fraction = htonl(ntp_reply.orig_time.fraction);
  ntp_reply.ref_time.seconds   = htonl(ntp_reply.ref_time.seconds);
  ntp_reply.ref_time.fraction  = htonl(ntp_reply.ref_time.fraction);
  ntp_reply.recv_time.seconds  = htonl(ntp_reply.recv_time.seconds);
  ntp_reply.recv_time.fraction = htonl(ntp_reply.recv_time.fraction);
  ntp_reply.xmit_time.seconds  = htonl(ntp_reply.xmit_time.seconds);
  ntp_reply.xmit_time.fraction = htonl(ntp_reply.xmit_time.fraction);  
  memcpy(out, &ntp_reply, sizeof(ntp));
}
