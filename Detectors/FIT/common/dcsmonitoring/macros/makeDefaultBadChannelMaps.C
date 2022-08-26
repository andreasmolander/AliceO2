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

/// \file makeDefaultBadChannelMaps.C
/// \brief Macro for uploading default bad channel maps for the FIT detectors
///
/// \author Andreas Molander <andreas.molander@cern.ch>, University of Jyvaskyla, Finland

#include "CCDB/CcdbApi.h"
#include "DataFormatsFIT/BadChannelMap.h"
#include "FDDBase/Constants.h"
#include "FV0Base/Constants.h"
#include "TFile.h"

#include <string>

int makeDefaultBadChannelMaps(const std::string& ccdbUrl = "http://localhost:8080",
                              const std::string& detectorName = "",
                              const std::string& fileName = "")
{
  const int nChannelsFT0 = 212;                                  // 208 channels + 4 reference channels
  const int nChannelsFV0 = o2::fv0::Constants::nFv0Channels + 1; // + 1 reference channel
  const int nChannelsFDD = o2::fdd::Nchannels + 3;               // + 3 reference channels

  bool ft0 = false;
  bool fv0 = false;
  bool fdd = false;

  o2::fit::BadChannelMap ft0BChM;
  o2::fit::BadChannelMap fv0BChM;
  o2::fit::BadChannelMap fddBChM;

  if (detectorName.empty()) {
    ft0 = fv0 = fdd = true;
  } else if (detectorName == "FT0") {
    ft0 = true;
  } else if (detectorName == "FV0") {
    fv0 = true;
  } else if (detectorName == "FDD") {
    fdd = true;
  } else {
    LOG(error) << "Please specify detector name FT0/FV0/FDD, or leave empty to do all.";
    return -1;
  }

  LOGP(info, "Creating default bad channel maps for {}", detectorName.empty() ? "FT0, FV0 and FDD" : detectorName);

  if (ft0) {
    for (int chId = 0; chId < nChannelsFT0; chId++) {
      ft0BChM.setChannelGood(chId, true);
    }
  }
  if (fv0) {
    for (int chId = 0; chId < nChannelsFV0; chId++) {
      fv0BChM.setChannelGood(chId, true);
    }
  }
  if (fdd) {
    for (int chId = 0; chId < nChannelsFDD; chId++) {
      fddBChM.setChannelGood(chId, true);
    }
  }

  if (!ccdbUrl.empty()) {
    o2::ccdb::CcdbApi ccdbApi;
    ccdbApi.init(ccdbUrl);
    std::map<std::string, std::string> metadata;
    metadata["default"] = "true";
    long validityStart = 1640991600000; // January 1, 2022 00:00:00 UTC
    long validityEnd = 1830293999000; // December 31, 2027 23:59:59 UTC

    if (ft0) {
      LOGP(info, "Storing default bad channel map on {}/{}", ccdbUrl, "FT0/Calib/BadChannelMap");
      ccdbApi.storeAsTFileAny(&ft0BChM, "FT0/Calib/BadChannelMap", metadata, validityStart, validityEnd);
    }
    if (fv0) {
      LOGP(info, "Storing default bad channel map on {}/{}", ccdbUrl, "FV0/Calib/BadChannelMap");
      ccdbApi.storeAsTFileAny(&fv0BChM, "FV0/Calib/BadChannelMap", metadata, validityStart, validityEnd);
    }
    if (fdd) {
      LOGP(info, "Storing default bad channel map on {}/{}", ccdbUrl, "FDD/Calib/BadChannelMap");
      ccdbApi.storeAsTFileAny(&fddBChM, "FDD/Calib/BadChannelMap", metadata, validityStart, validityEnd);
    }
  }

  if (!fileName.empty()) {
    if (ft0) {
      LOGP(info, "Storing bad channel map locally in {}", fileName + "FT0");
      TFile ft0File((fileName + "FT0").c_str(), "recreate");
      ft0File.WriteObjectAny(&ft0BChM, "o2::fit::BadChannelMap", "FT0BadChannelMap");
      ft0File.Close();
    }
    if (fv0) {
      LOGP(info, "Storing bad channel map locally in {}", fileName + "FV0");
      TFile fv0File((fileName + "FV0").c_str(), "recreate");
      fv0File.WriteObjectAny(&fv0BChM, "o2::fit::BadChannelMap", "FV0BadChannelMap");
      fv0File.Close();
    }
    if (fdd) {
      LOGP(info, "Storing bad channel map locally in {}", fileName + "FDD");
      TFile fddFile((fileName + "FDD").c_str(), "recreate");
      fddFile.WriteObjectAny(&fddBChM, "o2::fit::BadChannelMap", "FDDBadChannelMap");
      fddFile.Close();
    }
  }

  return 0;
}
