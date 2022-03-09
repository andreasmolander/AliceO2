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

#include <filesystem>
#include <TTree.h>
#include "Framework/Logger.h"
#include "DataFormatsFV0/Digit.h"
#include <TFile.h>
#include <cstring>

using namespace o2::fv0;

void logBCpair(const Digit& bcd, const Digit& bcd2);
void logChPair(const ChannelData& ch, const ChannelData& ch2);
bool triggersMatch(const Triggers& t1, const Triggers& t2, const bool isRaw);

int main(int argc, char* argv[])
{
  // Default trigger time if no hits differ between raw and digits -> don't compare if raw and no hits
  bool raw = true;
  bool ctf = false;
  if (argc > 1 && !strcmp(argv[1], "ctf")) {
    ctf = true;
    raw = false;
    LOG(info) << "Digits converted from CTFs";
  } else {
    LOG(info) << "Digits converted from RAW --> Not checking trigger times if no hits in digit.";
  }

  const std::string genDigFile{"fv0digits.root"};
  const std::string decDigFile{"o2_fv0digits.root"};
  const std::string branchBC{"FV0DigitBC"};
  const std::string branchCH{"FV0DigitCh"};

  if (!std::filesystem::exists(genDigFile)) {
    LOG(fatal) << "Generated digits file " << genDigFile << " is absent";
  }
  TFile flIn(genDigFile.c_str());
  std::unique_ptr<TTree> tree((TTree*)flIn.Get("o2sim"));
  if (!flIn.IsOpen() || flIn.IsZombie() || !tree) {
    LOG(fatal) << "Failed to get tree from generated digits file " << genDigFile;
  }
  std::vector<o2::fv0::Digit> digitsBC, *fv0BCDataPtr = &digitsBC;
  std::vector<o2::fv0::ChannelData> digitsCh, *fv0ChDataPtr = &digitsCh;
  tree->SetBranchAddress(branchBC.c_str(), &fv0BCDataPtr);
  tree->SetBranchAddress(branchCH.c_str(), &fv0ChDataPtr);

  if (!std::filesystem::exists(decDigFile)) {
    LOG(fatal) << "Decoded digits file " << decDigFile << " is absent";
  }

  TFile flIn2(decDigFile.c_str());
  std::unique_ptr<TTree> tree2((TTree*)flIn2.Get("o2sim"));
  if (!flIn2.IsOpen() || flIn2.IsZombie() || !tree2) {
    LOG(fatal) << "Failed to get tree from decoded digits file " << genDigFile;
  }
  std::vector<o2::fv0::Digit> digitsBC2, *fv0BCDataPtr2 = &digitsBC2;
  std::vector<o2::fv0::ChannelData> digitsCh2, *fv0ChDataPtr2 = &digitsCh2;
  tree2->SetBranchAddress(branchBC.c_str(), &fv0BCDataPtr2);
  tree2->SetBranchAddress(branchCH.c_str(), &fv0ChDataPtr2);

  // Store the data from the files in these
  std::vector<o2::fv0::Digit> digits;
  // std::vector<std::vector<o2::fv0::ChannelData>> channels;
  std::vector<std::unordered_map<uint8_t, o2::fv0::ChannelData>> channels;

  std::vector<o2::fv0::Digit> digits2;
  // std::vector<std::vector<o2::fv0::ChannelData>> channels2;
  std::vector<std::unordered_map<uint8_t, o2::fv0::ChannelData>> channels2;

  int nCh = 0;
  for (int iEntry = 0; iEntry < tree->GetEntries(); iEntry++) {
    tree->GetEntry(iEntry);
    for (int iBC = 0; iBC < digitsBC.size(); iBC++) {
      o2::fv0::Digit& digit = digitsBC[iBC];
      digits.push_back(digit);
      gsl::span<const o2::fv0::ChannelData> bcChannels = digit.getBunchChannelData(digitsCh);
      // channels.push_back(move(std::vector<o2::fv0::ChannelData>(bcChannels.begin(), bcChannels.end())));
      channels.push_back(std::unordered_map<uint8_t, o2::fv0::ChannelData>());
      for (int iCh = 0; iCh < bcChannels.size(); iCh++) {
        channels.back()[bcChannels[iCh].ChId] = std::move(bcChannels[iCh]);
      }
      nCh += bcChannels.size();
    }
  }

  int nCh2 = 0;
  for (int iEntry = 0; iEntry < tree2->GetEntries(); iEntry++) {
    tree2->GetEntry(iEntry);
    for (int iBC = 0; iBC < digitsBC2.size(); iBC++) {
      o2::fv0::Digit& digit = digitsBC2[iBC];
      digits2.push_back(digit);
      gsl::span<const o2::fv0::ChannelData> bcChannels = digit.getBunchChannelData(digitsCh2);
      // channels2.push_back(move(std::vector<o2::fv0::ChannelData>(bcChannels.begin(), bcChannels.end())));
      channels2.push_back(std::unordered_map<uint8_t, o2::fv0::ChannelData>());
      for (int iCh = 0; iCh < bcChannels.size(); iCh++) {
        channels2.back()[bcChannels[iCh].ChId] = std::move(bcChannels[iCh]);
      }
      nCh2 += bcChannels.size();
    }
  }

  bool logBC = true, logBCdiff = true, logCh = true, logChDiff = true;

  const int nDigits = digits.size();
  const int nDigits2 = digits2.size();
  const int nDigitsCompared = std::min(nDigits, nDigits2); // Number of compared digits
  int nChCompared = 0;                                     // Number of compared channels
  int nDigitsDiffs = 0;                                    // Number of mismatches between digits
  int nChDiffs = 0;                                        // Number of mismatches between channels

  for (int iBC = 0; iBC < nDigitsCompared; iBC++) {
    const Digit& digit = digits[iBC];
    const Digit& digit2 = digits2[iBC];

    // AM TODO: ref.mFirstEntry not to be compared (?) -> Digit::operator== can't be used as it is for the moment
    if (digit.ref.getEntries() != digit2.ref.getEntries()
        || !triggersMatch(digit.mTriggers, digit2.mTriggers, raw)
        || digit.mIntRecord != digit2.mIntRecord) {
      nDigitsDiffs++;

      if (logBCdiff) {
        LOG(info) << Form("First digit mismatch:");
        logBCpair(digit, digit2);
        logBC = false;
        logBCdiff = false;
      }
    }

    if (logBC) {
      LOG(info) << Form("First digits:");
      logBCpair(digit, digit2);
      logBC = false;
    }

    const int nChannels = channels[iBC].size();
    const int nChannels2 = channels2[iBC].size();

    nChCompared += std::min(nChannels, nChannels2);

    // Any difference in number of channels is checked and reported in the digit check above

    // for (int iCh = 0; iCh < std::min(nChannels, nChannels2); iCh++) {
    for (auto& iCh : nChannels <= nChannels2 ? channels[iBC] : channels2[iBC]) {
      // const ChannelData& ch = channels[iBC][iCh];
      // const ChannelData& ch2 = channels2[iBC][iCh];
      const ChannelData& ch = channels[iBC][iCh.first];
      const ChannelData& ch2 = channels2[iBC][iCh.first];

      if (!(ch == ch2)) { // ChannelData::operator!= not implemented
        nChDiffs++;

        if (logChDiff) {
          LOG(info) << Form("First channel mismatch:");
          logChPair(ch, ch2);
          logCh = false;
          logChDiff = false;
        }
      }

      if (logCh) {
        LOG(info) << Form("First channels:");
        logChPair(ch, ch2);
        logCh = false;
      }
    }
  }

  bool pass = true; // whether the test passed

  LOG(info) << Form("Number of digits: simulated = %i, decoded = %i", nDigits, nDigits2);
  if ((nDigits != nDigits2) || !nDigits || !nDigits) {
    pass = false;
  }

  LOG(info) << Form("Number of digit mismatches: %i/%i", nDigitsDiffs, nDigitsCompared);
  if (nDigitsDiffs) {
    pass = false;
  }

  LOG(info) << Form("Number of channels: simulated = %i, decoded %i", nCh, nCh2);
  if (nCh != nCh2) {
    pass = false;
  }

  LOG(info) << Form("Number of channel mismatches: %i/%i", nChDiffs, nChCompared);
  if (nChDiffs) {
    pass = false;
  }

  if (!pass) {
    LOG(fatal) << "Mismatch between simulated and decoded objects!";
  }

  return 0;
}

void logBCpair(const Digit& bcd, const Digit& bcd2)
{
  LOG(info) << "Simulated:";
  bcd.printLog();
  LOG(info) << "Decoded:";
  bcd2.printLog();
}

void logChPair(const ChannelData& ch, const ChannelData& ch2)
{
  LOG(info) << "Simulated:";
  ch.printLog();
  LOG(info) << "Decoded:";
  ch2.printLog();
}

bool triggersMatch(const Triggers& t1, const Triggers& t2, const bool isRaw)
{
  if (isRaw) {
    if (t1 == t2) {
      return true;
    } else {
      if (std::tie(t1.triggersignals, t1.nChanA, t1.nChanC, t1.amplA, t1.amplC) !=
          std::tie(t2.triggersignals, t2.nChanA, t2.nChanC, t2.amplA, t2.amplC)) {
        return false;
      } else {
        // The difference was in time info, ignore and let it pass if there's no hits
        bool passA = false;
        bool passC = false;

        if (!t1.nChanA || (t1.timeA == t2.timeA)) {
          passA = true;
        }

        if (!t1.nChanC || (t1.timeC == t2.timeC)) {
          passC = true;
        }

        return passA && passC;
      }
    }
  } else {
    return t1 == t2;
  }
}
