// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file  ChannelsConfig.cxx
/// \brief Structure for storing FEE channels configuration
///
/// \author Andreas Molander <andreas.molander@cern.ch>, University of Jyvaskyla, Finland

#ifndef ALICEO2_FT0_CHANNELCONFIG_H_
#define ALICEO2_FT0_CHANNELCONFIG_H_

#include "Rtypes.h"

#include <cstdint>

namespace o2
{
namespace ft0
{
  struct ChannelsConfig {
    static constexpr uint8_t N_CHANNELS = 212;
    static constexpr uint8_t N_BITMASK_CHUNKS = N_CHANNELS / 64 + (N_CHANNELS % 64 ? 1 : 0);

    static constexpr int16_t DEFAULT_NUMERICAL = -5000;
    static constexpr uint64_t DEFAULT_BITS = 0ull;

    uint8_t bitmaskChunk[N_CHANNELS];
    uint8_t bitmaskChunkBit[N_CHANNELS];

    // Numerical values (one per channel)
    int16_t adcDelay[N_CHANNELS];
    int16_t adc0Range[N_CHANNELS];
    int16_t adc1Range[N_CHANNELS];
    int16_t timeAlign[N_CHANNELS];
    int16_t cfdZero[N_CHANNELS];
    int16_t cfdThreshold[N_CHANNELS];
    int16_t thresholdCalibration[N_CHANNELS];

    // Bitmasks (one bit per channel)
    uint64_t chMaskData[N_BITMASK_CHUNKS];
    uint64_t chMaskTrigger[N_BITMASK_CHUNKS];

    ChannelsConfig();

    bool getBit(const uint64_t *bitmask, int chId) const;
    void setBit(uint64_t *bitmask, int chId, bool value = true);
    bool getChMaskData(int chId) const;
    void setChMaskData(int chId, bool value = true);
    bool getChMaskTrigger(int ChId) const;
    void setChMaskTrigger(int chId, bool value = true);
    void print(bool verbose = false) const;

    ClassDefNV(ChannelsConfig, 1);
  };
} // namespace ft0
} // namespace o2

#endif