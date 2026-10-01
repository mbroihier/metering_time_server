/*
 *      Meter - Class for creating metering objects
 *
 *      Copyright (C) 2026
 *          Mark Broihier
 *
 */
/* ---------------------------------------------------------------------- */
#include "pico/stdlib.h"
#ifndef METER_H_
#define METER_H_
class Meter {
 private:
  Meter();
  int64_t values[256];
  int64_t counts[256];
 public:
  // defined channels of metered data
  static const int TEMPERATURE = 0;
  // singleton info
  static Meter * singleton;
  static Meter * get_singleton();
  // methods
  int64_t get_temperature_int();
  int64_t get_count(int channel);
  void reset_count(int channel);
  void increment_count(int channel);
  void set_temperature_int(int64_t temperature);
};
#endif  // METER_H_
