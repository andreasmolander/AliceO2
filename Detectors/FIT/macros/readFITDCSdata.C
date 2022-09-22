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

/// \file readFITDCSdata.C
/// \brief ROOT macro for reading the FIT DCS data from CCDB
///
/// \author Andreas Molander <andreas.molander@cern.ch>, University of Jyvaskyla, Finland

#if !defined(__CLING__) || defined(__ROOTCLING__)

#include "CCDB/CcdbApi.h"
#include "CCDB/CCDBTimeStampUtils.h"
#include "DataFormatsFIT/DCSDPValues.h"
#include "DetectorsDCS/DataPointIdentifier.h"
#include "DetectorsDCS/DeliveryType.h"
#include "Framework/Logger.h"
#include "TAxis.h"
#include "TCanvas.h"
#include "TFile.h"
#include "TGraph.h"
#include "TLegend.h"
#include "TMultiGraph.h"
#include "TStyle.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <time.h>
#include <unordered_map>
#include <vector>

#endif

#include <boost/algorithm/string.hpp>

const std::string epochToReadable(const long timestamp);
std::vector<std::string> getAliases(const std::string& input, const o2::ccdb::CcdbApi& ccdbApi, const std::string& detectorName, const long timestamp);
void plotFITDCSmultigraph(std::unique_ptr<TMultiGraph>& multiGraph);

/// ROOT macro for reading FIT DCS data from CCDB.
/// \param detectorName     FIT subdetector, i.e. FT0/FV0/FDD, for which to query the data points.
/// \param dataPointAliases A string specifying what data points to query.
///                         It can be a semicolon separated list of data point aliases, e.g. "FT0/HV/FT0_A/MCP_A1/actual/iMon;FT0/HV/FT0_A/MCP_A2/actual/iMon"
///                         or a relative path to a file where the aliases are listed (one alias on each line), e.g. "aliases.txt" or "path/to/aliases.txt".
///                         If left empty, all data points defined in [ccdbUrl]/[detectorName]/Config/DCSDPconfig are queried.
/// \param timeStart        UNIX timestamp (in ms) for start of data point query. If omitted, one hour before the end time is used.
/// \param timeEnd          UNIX timestamp (in ms) for end of data point query. If omitted, current time is used.
/// \param ccdbUrl          CCDB url.
/// \param plot             Plot the data point values.
/// \param rootOutput       If specified, a plot and raw values are stored in [rootOutput].
/// \param print            Print data point values to console.
/// \param textOutput       If specified the data point values are stored in '[textOutput]'. TODO: implement
/// \param verbose          Verbose mode for debugging.
void readFITDCSdata(std::string detectorName = "FT0",
                    const std::string& dataPointAliases = "",
                    long timeStart = -1,
                    long timeEnd = -1,
                    const std::string& ccdbUrl = "http://alice-ccdb.cern.ch",
                    const bool plot = true,
                    const std::string& rootOutput = "",
                    const bool print = false,
                    const std::string& textOutput = "",
                    const bool verbose = false)
{
  // Parse and check detector name
  boost::to_upper(detectorName);
  if (detectorName != "FT0" && detectorName != "FV0" && detectorName != "FDD") {
    LOGP(fatal, "Invalid detector name provided: '{}'. Please use [FT0/FV0/FDD].", detectorName);
    return;
  }
  LOGP(info, "Fetching DCS data points for {}.", detectorName);

  // Init CCDB stuff
  o2::ccdb::CcdbApi ccdbApi;
  ccdbApi.init(ccdbUrl);
  const std::string ccdbPath = detectorName + "/Calib/DCSDPs";
  const std::map<std::string, std::string> metadata;

  // Set query time interval
  if (timeEnd < 0) {
    timeEnd = o2::ccdb::getCurrentTimestamp();
  }
  if (timeStart < 0) {
    timeStart = std::max(timeEnd - 1 * o2::ccdb::CcdbObjectInfo::HOUR, 0L);
  }
  if (timeStart > timeEnd) {
    long timeTmp = timeEnd;
    timeEnd = timeStart;
    timeStart = timeTmp;
  }
  LOG(info) << "Querying data points for time interval:";
  LOGP(info, "START {} ({})", timeStart, epochToReadable(timeStart));
  LOGP(info, "END   {} ({})", timeEnd, epochToReadable(timeEnd));

  // Define what data points to query
  std::vector<std::string> requestedDPaliases = getAliases(dataPointAliases, ccdbApi, detectorName, timeStart);

  if (verbose) {
    LOG(info) << "Querying datapoints:";
    for (auto& alias : requestedDPaliases) {
      LOG(info) << alias;
    }
  } else {
    LOGP(info, "Querying {} datapoints", requestedDPaliases.size());
  }

  // Set up data point value storage
  std::map<std::string, o2::fit::DCSDPValues> dataSeries;
  for (std::string& alias : requestedDPaliases) {
    dataSeries[alias] = o2::fit::DCSDPValues();
  }

  // Query data points
  long queryTimeStamp = timeStart;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> headersPrev;
  std::map<std::string, std::string> headersLatest;
  std::unordered_map<o2::dcs::DataPointIdentifier, o2::fit::DCSDPValues>* ccdbMap = nullptr;     // Pointer to the CCDB objects
  std::unordered_map<o2::dcs::DataPointIdentifier, o2::fit::DCSDPValues>* ccdbMapPrev = nullptr; // Pointer to the previously queried CCDB object
  // Pointer to the last CCDB object for the queried time period
  std::unordered_map<o2::dcs::DataPointIdentifier, o2::fit::DCSDPValues>* ccdbMapLatest = ccdbApi.retrieveFromTFileAny<std::unordered_map<o2::dcs::DataPointIdentifier, o2::fit::DCSDPValues>>(ccdbPath, metadata, timeEnd, &headersLatest);
  o2::dcs::DataPointIdentifier dpIdTmp;                                                          // DataPointIdentifier object used as CCDB map key
  o2::fit::DCSDPValues* ccdbDPValuesPointer = nullptr;                                           // Pointer to the CCDB map values

  while (queryTimeStamp <= timeEnd) {
    ccdbMap = ccdbApi.retrieveFromTFileAny<std::unordered_map<o2::dcs::DataPointIdentifier, o2::fit::DCSDPValues>>(ccdbPath, metadata, queryTimeStamp, &headers);

    if (!ccdbMap) {
      // TODO: Improve functionality when data not found
      if (queryTimeStamp == timeStart) {
        LOGP(fatal, "No CCDB object found for start time {}. Aborting.", queryTimeStamp);
        return;
      } else {
        LOGP(error, "No CCDB object found for timestamp {}. Stopping here.", queryTimeStamp);
        break;
      }
    } else if (headers["Location"] == headersPrev["Location"]) {
      // CCDB was maybe not updated as it should after the last CCDB object (i.e. there should always be a new object after 10 mins)
      // Either the O2 workflow was stopped for some time, or this is the last CCDB object.

      if (headers["Location"] == headersLatest["Location"]) {
        LOGP(warning, "No newer CCDB objects for query period found. The O2 workflow might have been stopped, or the query end time is in the future.");
        break;
      } else {
        LOGP(warning, "No new CCDB object for time {} ({}). The O2 workflow might have been stopped temporarily.", queryTimeStamp, epochToReadable(queryTimeStamp));
        // There are newer objects for the query period, move on to the next query
        // Currently the CCDB is updated every 10 mins
        queryTimeStamp += 10 * o2::ccdb::CcdbObjectInfo::MINUTE;
        continue;
      }
    } else {
      if (verbose) {
        LOGP(info, "CCDB object for timestamp {} found", queryTimeStamp);
      }
      headersPrev = headers;
    }

    // The CCDB object should always contain values for all datapoints. This is just to check that.
    if ((detectorName == "FT0" && ccdbMap->size() != 477)
        || (detectorName == "FV0" && ccdbMap->size() != 147)
        || (detectorName == "FDD" && ccdbMap->size() != 76)) {
      LOGP(error, 
           "Wrong number of DCS datapoints fetched for {}, got {}. There is a bug, please send output of this script, with input parameters, to andreas.molander@cern.ch.",
           detectorName, ccdbMap->size());
    }

    for (std::string& alias : requestedDPaliases) {
      // Need to fetch data point based on its type. To avoid specifying/knowing the type of each data point, try all used types instead.
      // TODO: This is not very nice, the dataformat in CCDB should be changed.
      o2::dcs::DataPointIdentifier::FILL(dpIdTmp, alias, o2::dcs::DeliveryType::DPVAL_DOUBLE);
      if (ccdbMap->find(dpIdTmp) != ccdbMap->end()) {
        ccdbDPValuesPointer = &ccdbMap->at(dpIdTmp);
      } else {
        o2::dcs::DataPointIdentifier::FILL(dpIdTmp, alias, o2::dcs::DeliveryType::DPVAL_UINT);
        if (ccdbMap->find(dpIdTmp) != ccdbMap->end()) {
          ccdbDPValuesPointer = &ccdbMap->at(dpIdTmp);
        } else {
          ccdbDPValuesPointer = nullptr;
        }
      }

      if (ccdbDPValuesPointer) {
        // If there are no values read for this DP yet, or there are newer values in the current CCDB object
        if (dataSeries[alias].empty() || (dataSeries[alias].values.back().first < ccdbDPValuesPointer->values.back().first)) {
          // if (verbose) {
          //   LOGP(info, "Newer values found.");
          // }

          // Iterate through the values in the current CCDB object
          for (auto& it : ccdbDPValuesPointer->values) {
            if ((it.first >= timeStart && it.first <= timeEnd) && (dataSeries[alias].empty() || it.first > dataSeries[alias].values.back().first)) {
              dataSeries[alias].add(it.first, it.second);
            }
          }
        }
      } else {
        LOGP(error, "Requested DP '{}' not found in CCDB object for timestamp {}.", alias, queryTimeStamp);
      }
    }

    // Currently the CCDB is updated every 10 mins
    queryTimeStamp += 10 * o2::ccdb::CcdbObjectInfo::MINUTE;
  }

  if (print) {
    LOG(info) << "Printing data point values:";
    for (auto& it : dataSeries) {
      LOGP(info, "{}", it.first);
      LOGP(info, "{} value(s):", it.second.values.size());
      if (verbose) {
        for (auto& value : it.second.values) {
          LOGP(info, "TIME = {} ({}), VALUE = {}", value.first, epochToReadable(value.first), value.second);
        }
      } else {
        LOGP(info, "First value:");
        LOGP(info, "TIME = {} ({}), VALUE = {}", it.second.values.front().first, epochToReadable(it.second.values.front().first), it.second.values.front().second);
        LOGP(info, "Last value:");
        LOGP(info, "TIME = {} ({}), VALUE = {}", it.second.values.back().first, epochToReadable(it.second.values.back().first), it.second.values.back().second);
      }
      LOG(info);
    }
  }

  if (plot || !rootOutput.empty()) {
    std::unique_ptr<TMultiGraph> multiGraph(new TMultiGraph);
    multiGraph->SetName("mgDCSDPTrends");
    std::vector<TGraph*> graphs;
    int pointCounter = 0;

    for (auto& dp : dataSeries) {
      if (!dp.second.empty()) {
        graphs.push_back(new TGraph);
        graphs.back()->SetName(dp.first.c_str());
        graphs.back()->GetXaxis()->SetTimeDisplay(1);
        graphs.back()->GetXaxis()->SetTimeFormat("#splitline{%d.%m.%y}{%H:%M:%S}");
        graphs.back()->SetMarkerStyle(20);
        pointCounter = 0;

        for (size_t i = 0; i < dp.second.values.size(); i++) {
          if (i > 0) {
            graphs.back()->SetPoint(pointCounter++, dp.second.values.at(i).first / 1000., dp.second.values.at(i - 1).second);
          }
          graphs.back()->SetPoint(pointCounter++, dp.second.values.at(i).first / 1000., dp.second.values.at(i).second);
        }
        // for (auto& dpValue : dp.second.values) {
        //   graphs.back()->SetPoint(pointCounter++, dpValue.first / 1000., dpValue.second);
        // }
        multiGraph->Add(graphs.back());
      } else {
        LOGP(warning, "No CCDB data found for alias '{}'. Data point excluded from graph.", dp.first);
      }
    }

    multiGraph->GetXaxis()->SetTimeDisplay(1);
    multiGraph->GetXaxis()->SetTimeFormat("#splitline{%d.%m.%y}{%H:%M:%S}");

    if (plot) {
      plotFITDCSmultigraph(multiGraph);
    }

    if (!rootOutput.empty()) {
      LOGP(info, "Storing trends in '{}'", rootOutput);
      std::unique_ptr<TFile> rootFile(TFile::Open(rootOutput.c_str(), "RECREATE"));
      multiGraph->Write();
      rootFile->WriteObject(&dataSeries, "DCSDPValues");
    }

    if (!textOutput.empty()) {
      LOG(info) << "Storing data point values to text file is not implemented yet.";
    }
  }  
}

void plotFITDCSdataFromFile(const std::string& fileName)
{
  std::unique_ptr<TFile> file(TFile::Open(fileName.c_str()));
  if (!file || file->IsZombie()) {
    LOGP(fatal, "Error opening file '{}'", fileName);
    return;
  }
  
  std::unique_ptr<TMultiGraph> multiGraph(file->Get<TMultiGraph>("mgDCSDPTrends"));
  if (!multiGraph) {
    LOGP(fatal, "Cannot find TMultiGraph 'mgDCSDPTrends' in {}", fileName);
    return;
  }

  plotFITDCSmultigraph(multiGraph);
}

void plotFITDCSmultigraph(std::unique_ptr<TMultiGraph>& multiGraph)
{
  gStyle->SetPalette(kRainBow);
  gStyle->SetTimeOffset(0);
  gStyle->SetLabelOffset(0.03);

  TCanvas* canvas = new TCanvas("canvas", "FIT DCS DP trends");
  multiGraph->DrawClone("apl pmc plc");
  canvas->BuildLegend();
}

const std::string epochToReadable(const long timestamp)
{
  std::string readableTime;
  time_t timeSeconds = timestamp / 1000;
  readableTime = std::string(asctime(localtime(&timeSeconds)));
  readableTime.pop_back();
  return readableTime;
}

std::vector<std::string> getAliases(const std::string& input, const o2::ccdb::CcdbApi& ccdbApi, const std::string& detectorName, const long timestamp)
{
  std::vector<std::string> aliases;
  std::string alias;
  std::ifstream file(input);

  if (file.is_open()) {
    LOGP(info, "Parsing input file '{}'", input);
    while(std::getline(file, alias)) {
      if (!alias.empty()) {
        aliases.push_back(alias);
      }
    }
  } else if (!input.empty()) {
    LOGP(info, "Parsing input string '{}'", input);
    std::stringstream ss(input);
    while(std::getline(ss, alias, ';')) {
      aliases.push_back(alias);
    }
  } else {
    // Input string empty, fetching all datapoints
    LOGP(info, "Data point input empty, fetching data point definitions from CCDB");
    std::string ccdbPath = detectorName + "/Config/DCSDPconfig";
    std::map<std::string, std::string> metadata;
    std::unordered_map<o2::dcs::DataPointIdentifier, std::string>* dpConfig = ccdbApi.retrieveFromTFileAny<std::unordered_map<o2::dcs::DataPointIdentifier, std::string>>(ccdbPath, metadata, timestamp);
    if (dpConfig) {
      for (auto& it : *dpConfig) {
        aliases.push_back(it.first.get_alias());
      }
    } else {
      LOGP(fatal, "No data point input provided, and can't fetch data point definitions from {}/{} for timestamp {}", ccdbApi.getURL(), ccdbPath, timestamp);
    }
  }
  return aliases;
}
