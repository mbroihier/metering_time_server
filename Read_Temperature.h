/*
 *      Read_Temperature - Class that handles reading Pico temperature
 *
 *      Copyright (C) 2026
 *          Mark Broihier
 *
 */
/* ---------------------------------------------------------------------- */
#include "pico/stdlib.h"
#include "hardware/adc.h"
#ifndef READ_TEMPERATURE_H_
#define READ_TEMPERATURE_H_
class Read_Temperature {
 public:
  Read_Temperature();
  float get_temperature();
  int64_t get_temperature_int();
};
#endif  // READ_TEMPERATURE_H_
