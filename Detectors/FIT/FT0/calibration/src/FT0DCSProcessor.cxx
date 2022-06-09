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

#include <FT0Calibration/FT0DCSProcessor.h>
#include "DetectorsCalibration/Utils.h"
#include "Rtypes.h"
#include <deque>
#include <string>
#include <algorithm>
#include <iterator>
#include <cstring>
#include <bitset>

using namespace o2::ft0;
using namespace o2::dcs;

using DeliveryType = o2::dcs::DeliveryType;
using DPID = o2::dcs::DataPointIdentifier;
using DPVAL = o2::dcs::DataPointValue;

void FT0DCSProcessor::init(const std::vector<DPID>& pids)
{
  // fill the array of the DPIDs that will be used by FT0
  // pids should be provided by CCDB

  for (const auto& it : pids) {
    mPids[it] = false;
    mFT0DCS[it].makeEmpty();
  }
}

int FT0DCSProcessor::process(const gsl::span<const DPCOM> dps)
{
  // first we check which DPs are missing - if some are, it means that
  // the delta map was sent

  if (mVerbose) {
    LOG(info) << "\n\nProcessing new DCS DP map\n-------------------------";
  }
  if (!mFirstTimeSet) {
    mFirstTime = mStartValidity;
    mFirstTimeSet = true;
  }

  if (false) {
    std::unordered_map<DPID, DPVAL> mapin;
    for (auto& it : dps) {
      mapin[it.id] = it.data;
    }
    for (auto& it : mPids) {
      const auto& el = mapin.find(it.first);
      if (el == mapin.end()) {
        LOG(debug) << "DP " << it.first << " not found in map";
      } else {
        LOG(debug) << "DP " << it.first << " found in map";
      }
    }
  }

  // now we process all DPs, one by one
  for (const auto& it : dps) {
    // we process only the DPs defined in the configuration
    const auto& el = mPids.find(it.id);
    if (el == mPids.end()) {
      LOG(info) << "DP " << it.id << " not found in FT0DCSProcessor, we will not process it";
      continue;
    }
    processDP(it);
    mPids[it.id] = true;
  }

  return 0;
}

int FT0DCSProcessor::processDP(const DPCOM& dpcom)
{
  // processing a single DP

  const auto& dpid = dpcom.id;
  const auto& type = dpid.get_type();
  const auto& val = dpcom.data;
  if (mVerbose) {
    if (type == DPVAL_FLOAT) {
      LOG(info) << "Processing DP = " << dpcom << ", with value = " << o2::dcs::getValue<float>(dpcom);
    } else if (type == DPVAL_INT) {
      LOG(info) << "Processing DP = " << dpcom << ", with value = " << o2::dcs::getValue<int32_t>(dpcom);
    }
  }
  auto flags = val.get_flags();
  if (processFlags(flags, dpid.get_alias()) == 0) {
    mDpsMap[dpid] = val; // we store the latest value seen by the FT0DCSProcessor during one CCDB update period
  }
  return 0;
}

uint64_t FT0DCSProcessor::processFlags(const uint64_t flags, const char* pid)
{
  // function to process the flag. the return code zero means that all is fine.
  // anything else means that there was an issue

  // for now, I don't know how to use the flags, so I do nothing

  if (flags & DataPointValue::KEEP_ALIVE_FLAG) {
    LOG(debug) << "KEEP_ALIVE_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::END_FLAG) {
    LOG(debug) << "END_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::FBI_FLAG) {
    LOG(debug) << "FBI_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::NEW_FLAG) {
    LOG(debug) << "NEW_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::DIRTY_FLAG) {
    LOG(debug) << "DIRTY_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::TURN_FLAG) {
    LOG(debug) << "TURN_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::WRITE_FLAG) {
    LOG(debug) << "WRITE_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::READ_FLAG) {
    LOG(debug) << "READ_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::OVERWRITE_FLAG) {
    LOG(debug) << "OVERWRITE_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::VICTIM_FLAG) {
    LOG(debug) << "VICTIM_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::DIM_ERROR_FLAG) {
    LOG(debug) << "DIM_ERROR_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::BAD_DPID_FLAG) {
    LOG(debug) << "BAD_DPID_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::BAD_FLAGS_FLAG) {
    LOG(debug) << "BAD_FLAGS_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::BAD_TIMESTAMP_FLAG) {
    LOG(debug) << "BAD_TIMESTAMP_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::BAD_PAYLOAD_FLAG) {
    LOG(debug) << "BAD_PAYLOAD_FLAG active for DP " << pid;
  }
  if (flags & DataPointValue::BAD_FBI_FLAG) {
    LOG(debug) << "BAD_FBI_FLAG active for DP " << pid;
  }

  return 0;
}

void FT0DCSProcessor::updateDPsCCDB()
{
  // Prepare the object to be sent to CCDB
  LOG(info) << "Finalizing";
  union Converter {
    uint64_t raw_data;
    float float_value;
    int int_value;
  } converter;

  for (auto& it : mPids) {
    auto& ft0dcs = mFT0DCS[it.first];
    if (it.second) {     // we processed the DP at least 1x
      it.second = false; // reset for the next period
      auto& dpval = mDpsMap[it.first];
      ft0dcs.time = dpval.get_epoch_time();
      converter.raw_data = dpval.payload_pt1;
      const auto& type = it.first.get_type();
      if (type == o2::dcs::DPVAL_FLOAT) {
        ft0dcs.value = lround(converter.float_value * 1000); // store as nA
      } else if (type == o2::dcs::DPVAL_INT) {
        ft0dcs.value = converter.int_value;
      }
    }

    if (mVerbose) {
      LOG(info) << "PID = " << it.first.get_alias();
      ft0dcs.print();
    }
  }

  LOG(info) << "Finalizing done";
  std::map<std::string, std::string> md;
  o2::calibration::Utils::prepareCCDBobjectInfo(mFT0DCS, mccdbDPsInfo, "FT0/Calib/DCSDPs", md, mStartValidity, mStartValidity + 3 * o2::ccdb::CcdbObjectInfo::DAY);

  return;
}