/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2015 The srsUE Developers. See the
 * COPYRIGHT file at the top-level directory of this distribution.
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

#ifndef USIM_DATA_H
#define USIM_DATA_H

#include "common/common.h"

namespace srsue {

typedef struct{
  uint32 nas_count_ul;
  uint32 nas_count_dl;
  uint8  rand[16];
  uint8  res[8];
  uint8  ck[16];
  uint8  ik[16];
  uint8  autn[16];
  uint8  k_nas_enc[32];
  uint8  k_nas_int[32];
  uint8  k_rrc_enc[32];
  uint8  k_rrc_int[32];
}auth_vector_t;

typedef struct{
  uint64 sqn_he;
  uint64 seq_he;
  uint8  ak[6];
  uint8  mac[8];
  uint8  k_asme[32];
  uint8  k_enb[32];
  uint8  k_up_enc[32];
  uint8  k_up_int[32];
  uint8  ind_he;
}generated_data_t;

} // namespace srsue


#endif // USIM_DATA_H