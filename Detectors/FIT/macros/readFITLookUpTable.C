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

/// \file  readFITLookUpTable.C
/// \brief Macro to read FIT LookUpTables from CCDB, CSV, JSON, or ROOT files,
///        print them, and optionally save to CSV, JSON, or ROOT format.
///
/// \author Andreas Molander <andreas.molander@cern.ch>, University of Jyvaskyla, Finland

#include <fairlogger/Logger.h>
#if !defined(__CLING__) || defined(__ROOTCLING__)

#include "CCDB/BasicCCDBManager.h"
#include "CCDB/CCDBTimeStampUtils.h"
#include "DataFormatsFIT/LookUpTable.h"

#include <TFile.h>
#include <TSystem.h>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#endif

#include "Framework/Logger.h"

#include <boost/algorithm/string.hpp>

using Table_t = std::vector<o2::fit::EntryFEE>;

std::string expandPath(const std::string& path)
{
  if (path.empty()) {
    return path;
  }
  TString expanded = path.c_str();
  gSystem->ExpandPathName(expanded);
  return std::string(expanded.Data());
}

Table_t loadFromCSV(const std::string& path)
{
  Table_t table;
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    LOGP(error, "Cannot open CSV file: {}", path);
    return table;
  }

  // Parse header to find column indices
  std::string headerLine;
  std::getline(ifs, headerLine);
  std::vector<std::string> headers;
  {
    std::istringstream iss(headerLine);
    std::string col;
    while (std::getline(iss, col, ',')) {
      boost::trim(col);
      headers.push_back(col);
    }
  }

  // Map column names to indices
  auto colIndex = [&headers](const std::string& name) -> int {
    for (int i = 0; i < static_cast<int>(headers.size()); i++) {
      if (headers[i] == name) {
        return i;
      }
    }
    return -1;
  };

  // Required columns
  int iLinkID = colIndex("LinkID");
  int iEndPointID = colIndex("EndPointID");
  int iCRUID = colIndex("CRUID");
  int iFEEID = colIndex("FEEID");
  int iModuleType = colIndex("ModuleType");
  int iLocalChannelID = colIndex("LocalChannelID");
  int iChannelID = colIndex("channel #");
  // Optional columns
  int iModule = colIndex("Module");
  int iHVBoard = colIndex("HV board");
  int iHVChannel = colIndex("HV channel");
  int iMCPSN = colIndex("MCP S/N");
  int iHVCable = colIndex("HV cable");
  int iSignalCable = colIndex("signal cable");

  if (iLinkID < 0 || iEndPointID < 0 || iCRUID < 0 || iFEEID < 0 ||
      iModuleType < 0 || iLocalChannelID < 0 || iChannelID < 0) {
    LOGP(error, "CSV file missing required columns. Need: LinkID, EndPointID, CRUID, FEEID, ModuleType, LocalChannelID, channel #");
    return table;
  }

  std::string line;
  int lineNum = 1;
  while (std::getline(ifs, line)) {
    lineNum++;
    if (line.empty()) {
      continue;
    }
    std::vector<std::string> fields;
    std::istringstream iss(line);
    std::string field;
    while (std::getline(iss, field, ',')) {
      boost::trim(field);
      fields.push_back(field);
    }
    // Pad with empty strings so optional trailing fields are handled
    fields.resize(headers.size());

    o2::fit::EntryFEE entry;
    entry.mEntryCRU.mLinkID = std::stoi(fields[iLinkID]);
    entry.mEntryCRU.mEndPointID = std::stoi(fields[iEndPointID]);
    entry.mEntryCRU.mCRUID = std::stoi(fields[iCRUID]);
    entry.mEntryCRU.mFEEID = std::stoi(fields[iFEEID]);
    entry.mModuleType = fields[iModuleType];
    entry.mLocalChannelID = fields[iLocalChannelID];
    entry.mChannelID = fields[iChannelID];
    if (iModule >= 0) {
      entry.mModuleName = fields[iModule];
    }
    if (iHVBoard >= 0) {
      entry.mBoardHV = fields[iHVBoard];
    }
    if (iHVChannel >= 0) {
      entry.mChannelHV = fields[iHVChannel];
    }
    if (iMCPSN >= 0) {
      entry.mSerialNumberMCP = fields[iMCPSN];
    }
    if (iHVCable >= 0) {
      entry.mCableHV = fields[iHVCable];
    }
    if (iSignalCable >= 0) {
      entry.mCableSignal = fields[iSignalCable];
    }
    table.push_back(entry);
  }

  ifs.close();
  LOGP(info, "Loaded {} entries from CSV: {}", table.size(), path);
  return table;
}

void saveAsCSV(const Table_t& table, const std::string& path)
{
  std::ofstream ofs(path);
  if (!ofs.is_open()) {
    LOGP(error, "Cannot open file for writing: {}", path);
    return;
  }
  ofs << "LinkID,EndPointID,CRUID,FEEID,ModuleType,LocalChannelID,channel #,Module,HV board,HV channel,MCP S/N,HV cable,signal cable\n";
  for (const auto& entry : table) {
    ofs << entry.mEntryCRU.mLinkID << ","
        << entry.mEntryCRU.mEndPointID << ","
        << entry.mEntryCRU.mCRUID << ","
        << entry.mEntryCRU.mFEEID << ","
        << entry.mModuleType << ","
        << entry.mLocalChannelID << ","
        << entry.mChannelID << ","
        << entry.mModuleName << ","
        << entry.mBoardHV << ","
        << entry.mChannelHV << ","
        << entry.mSerialNumberMCP << ","
        << entry.mCableHV << ","
        << entry.mCableSignal << "\n";
  }
  ofs.close();
  LOGP(info, "Saved {} entries to CSV: {}", table.size(), path);
}

void saveAsJSON(const Table_t& table, const std::string& path)
{
  boost::property_tree::ptree root;
  int idx = 0;
  for (const auto& entry : table) {
    boost::property_tree::ptree node;
    node.put("LinkID", entry.mEntryCRU.mLinkID);
    node.put("EndPointID", entry.mEntryCRU.mEndPointID);
    node.put("CRUID", entry.mEntryCRU.mCRUID);
    node.put("FEEID", entry.mEntryCRU.mFEEID);
    node.put("ModuleType", entry.mModuleType);
    node.put("LocalChannelID", entry.mLocalChannelID);
    node.put("channel #", entry.mChannelID);
    node.put("Module", entry.mModuleName);
    node.put("HV board", entry.mBoardHV);
    node.put("HV channel", entry.mChannelHV);
    node.put("MCP S/N", entry.mSerialNumberMCP);
    node.put("HV cable", entry.mCableHV);
    node.put("signal cable", entry.mCableSignal);
    root.push_back(std::make_pair("", node));
  }
  boost::property_tree::write_json(path, root);
  LOGP(info, "Saved {} entries to JSON: {}", table.size(), path);
}

void saveAsROOT(const Table_t& table, const std::string& path)
{
  TFile file(path.c_str(), "RECREATE");
  if (file.IsZombie()) {
    LOGP(error, "Cannot open file for writing: {}", path);
    return;
  }
  file.WriteObjectAny(&table, TClass::GetClass(typeid(table)), "ccdb_object");
  file.Close();
  LOGP(info, "Saved {} entries to ROOT: {}", table.size(), path);
}

/// \brief Read and print a FIT LookUpTable.
/// \param detectorName Detector name: "FT0", "FDD", or "FV0"
/// \param source       Data source: "ccdb", "csv", "json", or "root"
/// \param inputPath    File path for "json" or "root" sources (ignored for "ccdb")
/// \param timestamp    CCDB timestamp in ms (-1 = current time)
/// \param ccdbUrl      CCDB server URL
/// \param outputFile   If non-empty, save the table to this file
/// \param outputFormat Output file format: "csv", "json", or "root"
void readFITLookUpTable(std::string detectorName = "FT0",
                        std::string source = "ccdb",
                        std::string inputPath = "",
                        long timestamp = -1,
                        const std::string& ccdbUrl = "http://alice-ccdb.cern.ch",
                        const std::string& outputFile = "",
                        const std::string& outputFormat = "csv")
{
  // Validate detector name
  boost::to_upper(detectorName);
  if (detectorName != "FT0" && detectorName != "FV0" && detectorName != "FDD") {
    LOGP(fatal, "Invalid detector name: '{}'. Use FT0, FV0, or FDD.", detectorName);
    return;
  }

  // Validate source
  boost::to_lower(source);
  if (source != "ccdb" && source != "csv" && source != "json" && source != "root") {
    LOGP(fatal, "Invalid source: '{}'. Use ccdb, csv, json, or root.", source);
    return;
  }

  inputPath = expandPath(inputPath);

  Table_t table;

  // --- Load from source ---
  if (source == "ccdb") {
    const std::string ccdbPath = detectorName + "/Config/LookupTable";
    if (timestamp < 0) {
      timestamp = o2::ccdb::getCurrentTimestamp();
    }
    LOGP(info, "Fetching {} LookUpTable from {}/{} at timestamp {}", detectorName, ccdbUrl, ccdbPath, timestamp);

    auto& mgr = o2::ccdb::BasicCCDBManager::instance();
    mgr.setURL(ccdbUrl);
    auto* tablePtr = mgr.getForTimeStamp<Table_t>(ccdbPath, timestamp);
    LOGP(info, "CCDB query completed. Fetched size: {} bytes", mgr.getFetchedSize());
    if (!tablePtr || tablePtr->empty()) {
      LOGP(fatal, "LookUpTable not found or empty in {}/{} for timestamp {}.", ccdbUrl, ccdbPath, timestamp);
      return;
    }
    table = *tablePtr;
  } else if (source == "csv") {
    if (inputPath.empty()) {
      LOGP(fatal, "inputPath must be provided for source 'csv'.");
      return;
    }
    LOGP(info, "Reading {} LookUpTable from CSV file: {}", detectorName, inputPath);
    table = loadFromCSV(inputPath);
    if (table.empty()) {
      LOGP(fatal, "Failed to load any entries from CSV file: {}", inputPath);
      return;
    }
  } else if (source == "json") {
    if (inputPath.empty()) {
      LOGP(fatal, "inputPath must be provided for source 'json'.");
      return;
    }
    LOGP(info, "Reading {} LookUpTable from JSON file: {}", detectorName, inputPath);
    o2::fit::LookupTableBase<> lut(inputPath);
    table = lut.getVecMetadataFEE();
  } else if (source == "root") {
    if (inputPath.empty()) {
      LOGP(fatal, "inputPath must be provided for source 'root'.");
      return;
    }
    LOGP(info, "Reading {} LookUpTable from ROOT file: {}", detectorName, inputPath);
    TFile file(inputPath.c_str(), "READ");
    if (file.IsZombie()) {
      LOGP(fatal, "Cannot open ROOT file: {}", inputPath);
      return;
    }
    auto* tablePtr = static_cast<Table_t*>(file.GetObjectChecked("ccdb_object", TClass::GetClass(typeid(Table_t))));
    if (!tablePtr) {
      LOGP(fatal, "Object 'ccdb_object' not found in ROOT file: {}", inputPath);
      return;
    }
    table = *tablePtr;
    file.Close();
  }

  // --- Build the LUT to get derived info ---
  o2::fit::LookupTableBase<> lut(&table);

  LOGP(info, "Successfully loaded {} entries for {} LookUpTable from {}.", table.size(), detectorName, source);

  // --- Print summary ---
  const auto& pmMap = lut.getMapEntryPM2ChannelID();
  const auto& cruMap = lut.getMapEntryCRU2ModuleType();
  const auto& tcm = lut.getEntryCRU_TCM();

  int nPM = 0, nPM_LCS = 0, nTCM = 0;
  for (const auto& [cru, moduleType] : cruMap) {
    if (moduleType == o2::fit::EModuleType::kPM) {
      nPM++;
    } else if (moduleType == o2::fit::EModuleType::kPM_LCS) {
      nPM_LCS++;
    } else if (moduleType == o2::fit::EModuleType::kTCM) {
      nTCM++;
    }
  }

  LOGP(info, "=== {} LookUpTable ===", detectorName);
  LOGP(info, "Total FEE entries:  {}", table.size());
  LOGP(info, "PM channels:        {}", pmMap.size());
  LOGP(info, "CRU links:          {} (PM: {}, PM-LCS: {}, TCM: {})", cruMap.size(), nPM, nPM_LCS, nTCM);
  LOGP(info, "TCM link:           LinkID={}, EndPointID={}", tcm.mLinkID, tcm.mEndPointID);
  LOGP(info, "========================");

  // --- Print full table ---
  for (const auto& entry : table) {
    LOG(info) << entry;
  }

  // --- Save to file ---
  if (!outputFile.empty()) {
    std::string outPath = expandPath(outputFile);
    std::string fmt = outputFormat;
    boost::to_lower(fmt);
    if (fmt == "csv") {
      saveAsCSV(table, outPath);
    } else if (fmt == "json") {
      saveAsJSON(table, outPath);
    } else if (fmt == "root") {
      saveAsROOT(table, outPath);
    } else {
      LOGP(error, "Unknown output format: '{}'. Use csv, json, or root.", fmt);
    }
  }
}
