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

#include "DataFormatsFT0/ChannelsConfig.h"
#include "Framework/Logger.h"

#include "TString.h"

#include <algorithm>
#include <string>

using namespace o2::ft0;

ChannelsConfig::ChannelsConfig()
{
  // Set up the indexes and bit positions to access the bits for each channel
  for (int i = 0; i < N_CHANNELS; i++) {
    bitmaskChunk[i] = i / 64;
    bitmaskChunkBit[i] = i % 64;
  }

  // Fill arrays with default values
  std::fill_n(adcDelay, N_CHANNELS, DEFAULT_NUMERICAL);
  std::fill_n(adc0Range, N_CHANNELS, DEFAULT_NUMERICAL);
  std::fill_n(adc1Range, N_CHANNELS, DEFAULT_NUMERICAL);
  std::fill_n(timeAlign, N_CHANNELS, DEFAULT_NUMERICAL);
  std::fill_n(cfdZero, N_CHANNELS, DEFAULT_NUMERICAL);
  std::fill_n(cfdThreshold, N_CHANNELS, DEFAULT_NUMERICAL);
  std::fill_n(thresholdCalibration, N_CHANNELS, DEFAULT_NUMERICAL);

  std::fill_n(chMaskData, N_BITMASK_CHUNKS, DEFAULT_BITS);
  std::fill_n(chMaskTrigger, N_BITMASK_CHUNKS, DEFAULT_BITS);
}

bool ChannelsConfig::getBit(const uint64_t *bitmask, int chId) const
{
  return bool(bitmask[bitmaskChunk[chId]] & (1ull << bitmaskChunkBit[chId]));
}

void ChannelsConfig::setBit(uint64_t *bitmask, int chId, bool value)
{
  if (value) {
    bitmask[bitmaskChunk[chId]] |= (1ull << bitmaskChunkBit[chId]);
  } else {
    bitmask[bitmaskChunk[chId]] &= ~(1ull << bitmaskChunkBit[chId]);
  }
}

bool ChannelsConfig::getChMaskData(int chId) const { return getBit(chMaskData, chId); }
void ChannelsConfig::setChMaskData(int chId, bool value) { setBit(chMaskData, chId, value); }
bool ChannelsConfig::getChMaskTrigger(int chId) const { return getBit(chMaskTrigger, chId); }
void ChannelsConfig::setChMaskTrigger(int chId, bool value) { setBit(chMaskTrigger, chId, value); }

void ChannelsConfig::print(bool verbose) const
{
  // Prit only 5 channels by default
  int n = verbose ? N_CHANNELS : std::min(N_CHANNELS, (uint8_t)5);

  std::string text = "";

  auto printArray = [&](const int16_t *array, const char* label) {
    text.append("\n");
    for (int i = 0; i < n; i++) text.append(TString::Format("Ch %i: %i\n", i, array[i]));
    LOG(info) << label << ": ";
    LOG(info) << text;
    text.clear();
  };

  auto printMask = [&](const uint64_t *mask, const char* label) {
    text.append("\n");
    for (int i = 0; i < n; i++) text.append(TString::Format("Ch %i: %i\n", i, getBit(mask, i)));

    LOG(info) << label << ": " << text;
    text.clear();
  };

  printArray(adcDelay, "ADC delay");
  printArray(adc0Range, "ADC0 range");
  printArray(adc1Range, "ADC1 range");
  printArray(timeAlign, "Time align");
  printArray(cfdZero, "CFD zero");
  printArray(cfdThreshold, "CFD threshold");
  printArray(thresholdCalibration, "Threshold calibration");
  printMask(chMaskData, "Channel mask data");
  printMask(chMaskTrigger, "Channel mask trigger");
}
