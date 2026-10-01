/*
 *      Read_Temperature - Class that handles reading Pico temperature
 *
 *      Copyright (C) 2026
 *          Mark Broihier
 *
 */
/* ---------------------------------------------------------------------- */
#include "Read_Temperature.h"

Read_Temperature::Read_Temperature() {
  adc_init();
  adc_set_temp_sensor_enabled(true);
  adc_select_input(4);  // channel 4 is the internal temperature senso
}

float Read_Temperature::get_temperature() {
  uint16_t AToDValue = adc_read();
  float temperature = 27.0 - (AToDValue * (3.3 / 4095.0) - 0.706) / 0.0011721;
  return temperature;
}

int64_t Read_Temperature::get_temperature_int() {
  uint16_t AToDValue = adc_read();
  int64_t temperature = (27.0 - (AToDValue * (3.3 / 4095.0) - 0.706) / 0.0011721) * 65536.0;  // Q16
  return temperature;
}
