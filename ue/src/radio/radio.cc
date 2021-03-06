/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of the srsUE library.
 *
 * srsUE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsUE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "srslte/srslte.h"
extern "C" {
#include "srslte/rf/rf.h"
}
#include "radio/radio.h"
#include <string.h>

namespace srslte {

bool radio::init(char *args, char *devname)
{
  if (srslte_rf_open_devname(&rf_device, devname, args)) {
    fprintf(stderr, "Error opening RF device\n");
    return false;
  }
  
  tx_adv_negative = false; 
  agc_enabled = false; 
  burst_preamble_samples = 0; 
  burst_preamble_time_rounded = 0; 
  cur_tx_srate = 0; 
  is_start_of_burst = true; 
  
  
  tx_adv_auto = true; 
  /* Set default preamble length each known device */
  if (!strcmp(srslte_rf_name(&rf_device), "UHD")) {
    burst_preamble_sec = uhd_default_burst_preamble_sec;
  } else if (!strcmp(srslte_rf_name(&rf_device), "bladeRF")) {
    burst_preamble_sec = blade_default_burst_preamble_sec;
  }
  
  return true;    
}

void radio::set_manual_calibration(rf_cal_t* calibration)
{
  srslte_rf_cal_t tx_cal; 
  tx_cal.dc_gain  = calibration->tx_corr_dc_gain;
  tx_cal.dc_phase = calibration->tx_corr_dc_phase;
  tx_cal.iq_i     = calibration->tx_corr_iq_i;
  tx_cal.iq_q     = calibration->tx_corr_iq_q;
  srslte_rf_set_tx_cal(&rf_device, &tx_cal);
}

void radio::set_tx_rx_gain_offset(float offset) {
  srslte_rf_set_tx_rx_gain_offset(&rf_device, offset);  
}

void radio::set_burst_preamble(double preamble_us)
{
  burst_preamble_sec = (double) preamble_us/1e6; 
}

void radio::set_tx_adv(int nsamples)
{
  tx_adv_auto = false;
  tx_adv_nsamples = nsamples;
  if (!nsamples) {
    tx_adv_sec = 0; 
  }
  
}

void radio::set_tx_adv_neg(bool tx_adv_is_neg) {
  tx_adv_negative = tx_adv_is_neg; 
}

void radio::tx_offset(int offset_)
{
  offset = offset_; 
}

bool radio::start_agc(bool tx_gain_same_rx)
{
  if (srslte_rf_start_gain_thread(&rf_device, tx_gain_same_rx)) {
    fprintf(stderr, "Error opening RF device\n");
    return false;
  }

  agc_enabled = true; 
 
  return true;    
}
bool radio::rx_at(void* buffer, uint32_t nof_samples, srslte_timestamp_t rx_time)
{
  fprintf(stderr, "Not implemented\n");
  return false; 
}

bool radio::rx_now(void* buffer, uint32_t nof_samples, srslte_timestamp_t* rxd_time)
{
  if (srslte_rf_recv_with_time(&rf_device, buffer, nof_samples, true, 
    rxd_time?&rxd_time->full_secs:NULL, rxd_time?&rxd_time->frac_secs:NULL) > 0) {
    return true; 
  } else {
    return false; 
  }
}

void radio::get_time(srslte_timestamp_t *now) {
  srslte_rf_get_time(&rf_device, &now->full_secs, &now->frac_secs);  
}

// TODO: Use Calibrated values for this 
float radio::set_tx_power(float power)
{
  if (power > 10) {
    power = 10; 
  }
  if (power < -50) {
    power = -50; 
  }
  float gain = power + 74;
  srslte_rf_set_tx_gain(&rf_device, gain);
  return gain; 
}

float radio::get_max_tx_power()
{
  return 10;
}

float radio::get_rssi()
{
  return srslte_rf_get_rssi(&rf_device);  
}

bool radio::has_rssi()
{
  return srslte_rf_has_rssi(&rf_device);
}

bool radio::tx(void* buffer, uint32_t nof_samples, srslte_timestamp_t tx_time)
{
  if (!tx_adv_negative) {
    srslte_timestamp_sub(&tx_time, 0, tx_adv_sec);
  } else {
    srslte_timestamp_add(&tx_time, 0, tx_adv_sec);
  }
  
  if (is_start_of_burst) {
    if (burst_preamble_samples != 0) {
      srslte_timestamp_t tx_time_pad; 
      srslte_timestamp_copy(&tx_time_pad, &tx_time);
      srslte_timestamp_sub(&tx_time_pad, 0, burst_preamble_time_rounded); 
      save_trace(1, &tx_time_pad);
      srslte_rf_send_timed2(&rf_device, zeros, burst_preamble_samples, tx_time_pad.full_secs, tx_time_pad.frac_secs, true, false);
      is_start_of_burst = false; 
    }        
  }
  
  // Save possible end of burst time 
  srslte_timestamp_copy(&end_of_burst_time, &tx_time);
  srslte_timestamp_add(&end_of_burst_time, 0, (double) nof_samples/cur_tx_srate); 
  
  save_trace(0, &tx_time);
  int ret = srslte_rf_send_timed2(&rf_device, buffer, nof_samples+offset, tx_time.full_secs, tx_time.frac_secs, is_start_of_burst, false);
  offset = 0; 
  is_start_of_burst = false; 
  if (ret > 0) {
    return true; 
  } else {
    return false; 
  }
}

uint32_t radio::get_tti_len()
{
  return sf_len; 
}

void radio::set_tti_len(uint32_t sf_len_)
{
  sf_len = sf_len_; 
}

void radio::tx_end()
{
  if (!is_start_of_burst) {
    save_trace(2, &end_of_burst_time);
    srslte_rf_send_timed2(&rf_device, zeros, 0, end_of_burst_time.full_secs, end_of_burst_time.frac_secs, false, true);
    is_start_of_burst = true; 
  }
}

void radio::start_trace() {
  trace_enabled = true; 
}

void radio::set_tti(uint32_t tti_) {
  tti = tti_; 
}

void radio::write_trace(std::string filename)
{
  tr_local_time.writeToBinary(filename + ".local");
  tr_is_eob.writeToBinary(filename + ".eob");
  tr_usrp_time.writeToBinary(filename + ".usrp");
  tr_tx_time.writeToBinary(filename + ".tx");
}

void radio::save_trace(uint32_t is_eob, srslte_timestamp_t *tx_time) {
  if (trace_enabled) {
    tr_local_time.push_cur_time_us(tti);
    srslte_timestamp_t usrp_time; 
    srslte_rf_get_time(&rf_device, &usrp_time.full_secs, &usrp_time.frac_secs);
    tr_usrp_time.push(tti, srslte_timestamp_uint32(&usrp_time));
    tr_tx_time.push(tti, srslte_timestamp_uint32(tx_time));
    tr_is_eob.push(tti, is_eob);
  }
}

void radio::set_rx_freq(float freq)
{
  rx_freq = srslte_rf_set_rx_freq(&rf_device, freq);
}

void radio::set_rx_gain(float gain)
{
  srslte_rf_set_rx_gain(&rf_device, gain);
}

double radio::set_rx_gain_th(float gain)
{
  return srslte_rf_set_rx_gain_th(&rf_device, gain);
}

void radio::set_master_clock_rate(float rate)
{
  srslte_rf_set_master_clock_rate(&rf_device, rate);
}

void radio::set_rx_srate(float srate)
{
  srslte_rf_set_rx_srate(&rf_device, srate);
}

void radio::set_tx_freq(float freq)
{
  tx_freq = srslte_rf_set_tx_freq(&rf_device, freq);  
}

void radio::set_tx_gain(float gain)
{
  srslte_rf_set_tx_gain(&rf_device, gain);
}

float radio::get_rx_freq()
{
  return rx_freq;
}

float radio::get_tx_freq()
{
  return tx_freq; 
}

float radio::get_tx_gain()
{
  return srslte_rf_get_tx_gain(&rf_device);
}

float radio::get_rx_gain()
{
  return srslte_rf_get_rx_gain(&rf_device);
}

void radio::set_tx_srate(float srate)
{
  cur_tx_srate = srslte_rf_set_tx_srate(&rf_device, srate);
  burst_preamble_samples = (uint32_t) (cur_tx_srate * burst_preamble_sec);
  if (burst_preamble_samples > burst_preamble_max_samples) {
    burst_preamble_samples = burst_preamble_max_samples;
    fprintf(stderr, "Error setting TX srate %.1f MHz. Maximum frequency for zero prepadding is 30.72 MHz\n", srate*1e-6);
  }
  burst_preamble_time_rounded = (double) burst_preamble_samples/cur_tx_srate;  
  
  /* Set time advance for each known device if in auto mode */
  if (tx_adv_auto) {
    if (!strcmp(srslte_rf_name(&rf_device), "UHD")) {
      double srate_khz = round(cur_tx_srate/1e3);
      if (srate_khz == 1.92e3) {
        tx_adv_sec = 105*16*SRSLTE_LTE_TS;
      } else if (srate_khz == 3.84e3) {
        tx_adv_sec = 48*16*SRSLTE_LTE_TS;
      } else if (srate_khz == 5.76e3) {
        tx_adv_sec = 36*16*SRSLTE_LTE_TS;
      } else if (srate_khz == 11.52e3) {
        tx_adv_sec = 26*16*SRSLTE_LTE_TS;
      } else if (srate_khz == 15.36e3) {
        tx_adv_sec = 19*16*SRSLTE_LTE_TS;
      } else if (srate_khz == 23.04e3) {
        tx_adv_sec = 14*16*SRSLTE_LTE_TS;
      } else {
        /* Interpolate from known values */
        tx_adv_sec = uhd_default_tx_adv_samples * (1/cur_tx_srate) + uhd_default_tx_adv_offset_sec;        
      }    } else if (!strcmp(srslte_rf_name(&rf_device), "bladeRF")) {
      double srate_khz = round(cur_tx_srate/1e3);
      if (srate_khz == 1.92e3) {
        tx_adv_sec = 27*16*SRSLTE_LTE_TS;
      } else if (srate_khz == 3.84e3) {
        tx_adv_sec = 12*16*SRSLTE_LTE_TS;
      } else if (srate_khz == 5.76e3) {
        tx_adv_sec = 11*16*SRSLTE_LTE_TS;
      } else if (srate_khz == 11.52e3) {
        tx_adv_sec = 5*16*SRSLTE_LTE_TS;
      } else if (srate_khz == 15.36e3) {
        tx_adv_sec = 2*16*SRSLTE_LTE_TS;
      } else if (srate_khz == 23.04e3) {
        tx_adv_sec = 2*16*SRSLTE_LTE_TS;
      } else {
        printf("interpolating, srate=%f kHz\n", srate_khz);
        /* Interpolate from known values */
        tx_adv_sec = blade_default_tx_adv_samples * (1/cur_tx_srate) + blade_default_tx_adv_offset_sec;        
      }
    }
  } else {
    tx_adv_sec = tx_adv_nsamples * (1/cur_tx_srate);
    printf("tx_adv_sec=%f us\n", tx_adv_sec*1e6);
  }
}

void radio::start_rx()
{
  srslte_rf_start_rx_stream(&rf_device);
}

void radio::stop_rx()
{
  srslte_rf_stop_rx_stream(&rf_device);
}

void radio::register_error_handler(srslte_rf_error_handler_t h)
{
  srslte_rf_register_error_handler(&rf_device, h);
}

  
}

