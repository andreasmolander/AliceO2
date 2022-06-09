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

#ifndef DETECTOR_FT0DCSPROCESSOR_H_
#define DETECTOR_FT0DCSPROCESSOR_H_

#include <memory>
#include <Rtypes.h>
#include <unordered_map>
#include <deque>
#include <numeric>
#include "Framework/Logger.h"
#include "DetectorsDCS/DataPointCompositeObject.h"
#include "DetectorsDCS/DataPointIdentifier.h"
#include "DetectorsDCS/DataPointValue.h"
#include "DetectorsDCS/DeliveryType.h"
#include "CCDB/CcdbObjectInfo.h"
#include "CommonUtils/MemFileHelper.h"
#include "CCDB/CcdbApi.h"
#include <gsl/gsl>

/// @brief Class to process DCS data points

namespace o2
{
namespace ft0
{

using DPID = o2::dcs::DataPointIdentifier;
using DPVAL = o2::dcs::DataPointValue;
using DPCOM = o2::dcs::DataPointCompositeObject;

struct FT0DCSinfo {
  uint64_t time;
  int value;
  FT0DCSinfo()
  {
    time = 0;
    value = -999999999;
  }
  void makeEmpty()
  {
    time = 0;
    value = -999999999;
  }
  void print() const
  {
    LOG(info) << "timestamp = " << time << ", value = " << value;
  }

  ClassDefNV(FT0DCSinfo, 1);
};

class FT0DCSProcessor
{

 public:
  using CcdbObjectInfo = o2::ccdb::CcdbObjectInfo;

  FT0DCSProcessor() = default;
  ~FT0DCSProcessor() = default;

  void init(const std::vector<DPID>& pids);

  int process(const gsl::span<const DPCOM> dps);
  int processDP(const DPCOM& dpcom);
  uint64_t processFlags(uint64_t flag, const char* pid);

  void updateDPsCCDB();

  const CcdbObjectInfo& getccdbDPsInfo() const { return mccdbDPsInfo; }
  CcdbObjectInfo& getccdbDPsInfo() { return mccdbDPsInfo; }
  const std::unordered_map<DPID, FT0DCSinfo>& getFT0DPsInfo() const { return mFT0DCS; }

  void setStartValidity(long t) { mStartValidity = t; }
  void useVerboseMode() { mVerbose = true; }

  void clearDPsinfo()
  {
    mDpsMap.clear();
    mFT0DCS.clear();
  }

 private:
  std::unordered_map<DPID, FT0DCSinfo> mFT0DCS; // this is the object that will go to the CCDB
  std::unordered_map<DPID, bool> mPids;         // contains all PIDs for the processor, the bool
                                                // will be true if the DP was processed at least once
  std::unordered_map<DPID, DPVAL> mDpsMap;      // this is the map that will hold the DPs

  CcdbObjectInfo mccdbDPsInfo;
  long mFirstTime;         // time when a CCDB object was stored first
  long mStartValidity = 0; // TF index for processing, used to store CCDB object
  bool mFirstTimeSet = false;

  bool mVerbose = false;

  ClassDefNV(FT0DCSProcessor, 0);
};
} // namespace ft0
} // namespace o2

#endif
