/*
 *      Meter - Class for making a metering object
 *
 *      Copyright (C) 2026
 *          Mark Broihier
 *
 */
/* ---------------------------------------------------------------------- */
#include "Meter.h"

Meter::Meter() {
  for (int i = 0; i < 256; i++) {
    values[i] = 0x8000000000000000;  // mark as unassigned (most negative possible value)
  }
}

Meter * Meter::get_singleton() {
  if (!singleton) {
    singleton = new Meter();
  }
  return singleton;
}

int64_t Meter::get_temperature_int() {
  return values[TEMPERATURE];
}

void Meter::set_temperature_int(int64_t temperature) {
  values[TEMPERATURE] = temperature;
}

int64_t Meter::get_count(int channel) {
  int count = 0;
  if (channel >= 0 && channel < 256) {
    count =  counts[channel];
  }
  return count;
}

void Meter::reset_count(int channel) {
  if (channel >= 0 && channel < 256) {
    counts[channel] = 0;
  }
}

void Meter::increment_count(int channel) {
  if (channel >= 0 && channel < 256) {
    counts[channel]++;
  }
}
