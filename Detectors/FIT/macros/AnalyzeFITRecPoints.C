#include <RtypesCore.h>
#include <TCanvas.h>
#include <TLegend.h>
#include <gsl/span>
#if !defined(__CLING__) || defined(__ROOTCLING__)

#include "TFile.h"
#include "TH1.h"
#include "TPaveStats.h"
#include "TTree.h"

#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "FT0Base/Constants.h"
#include "DataFormatsFT0/RecPoints.h"

#endif

// Utilities
void UpdateStatBox(TObject* h, const int iSet) {
	TPaveStats* stats = (TPaveStats*)h->FindObject("stats");
	if (stats) {
		stats->SetTextColor(iSet + 2);
		stats->SetX1NDC(0.7);
		stats->SetX2NDC(0.9);
		stats->SetY1NDC(0.7  - iSet * 0.2);
		stats->SetY2NDC(0.9 - iSet * 0.2);
	}
}

// Read one FT0 RecPoint file
int ReadFT0RecPoints(std::string fileName = "o2reco_ft0", const bool draw = false, const bool writeToFile = true) {
	// Strip the file name of the .root extension
	size_t pos = fileName.find(".root");
	if (pos != std::string::npos) {
		fileName.erase(pos);
	}

	// Open input recpoint file and tree
	std::unique_ptr<TFile> recpointFile(TFile::Open(Form("%s.root", fileName.c_str()), "READ"));
	if (!recpointFile || recpointFile->IsZombie()) {
		std::cout << "Could not open file " << fileName << std::endl;
		return 1;
	}

	std::unique_ptr<TTree> recpointTree((TTree*)recpointFile->Get("o2sim"));
	if (!recpointTree) {
		std::cout << "Could not find tree o2sim in file " << fileName << std::endl;
		return 1;
	}

	std::vector<o2::ft0::RecPoints> recpoints;
	std::vector<o2::ft0::RecPoints>* pRecpoints = &recpoints;
	std::vector<o2::ft0::ChannelDataFloat> channelData;
	std::vector<o2::ft0::ChannelDataFloat>* pChannelData = &channelData;

	recpointTree->SetBranchAddress("FT0Cluster", &pRecpoints);
	recpointTree->SetBranchAddress("FT0RecChData", &pChannelData);

	TH1I* hChannels = new TH1I("hChannels", "Channel statistics;channel ID;counts", o2::ft0::Constants::sNTOTAL_CHANNELS_PM, 0, o2::ft0::Constants::sNTOTAL_CHANNELS_PM);

	for (Long64_t iEntry = 0; iEntry < recpointTree->GetEntries(); ++iEntry) {
		recpointTree->GetEntry(iEntry);

		for (const auto& recpoint : recpoints) {
			gsl::span<const o2::ft0::ChannelDataFloat> channels = recpoint.getBunchChannelData(channelData);
			for (const auto& channel : channels) {
				hChannels->Fill(channel.ChId);
			}
		}
	}

	// Draw histograms
	if (draw) {
		TCanvas* cChannels = new TCanvas("cChannels", "Channel statistics");
		hChannels->DrawCopy();
	}

	// Write histgrams to file
	if (writeToFile) {
		std::string outputFileName = Form("%sread.root", fileName.c_str());
		std::cout << "Writing histograms to file " << outputFileName << std::endl;
		std::unique_ptr<TFile> outputFile(TFile::Open(outputFileName.c_str(), "RECREATE"));
		outputFile->cd();
		hChannels->Write();
		outputFile->Close();
	}
	
	return 0;
}

int AnalyzeFITRecPoints(std::string detector = "FT0",
												const bool read = true,
												const bool writeToFile = true,
												std::initializer_list<std::string> fileNames = {"o2reco_ft0.root"},
												std::initializer_list<std::string> labels = {"1"}) {
	const bool isFT0 = detector == "FT0";
	const bool isFV0 = detector == "FV0";
	const bool isFDD = detector == "FDD";

	if (!isFT0 && !isFV0 && !isFDD) {
		std::cout << "Detector must be FT0/FV0/FDD" << std::endl;
		return 1;
	}

	const int nSets = fileNames.size();

	std::vector<std::string> recPointFileNames(fileNames);
	std::vector<std::string> setLabels(labels);

	if (recPointFileNames.size() != setLabels.size()) {
		std::cout << "Number of file names and labels must be the same" << std::endl;
		return 1;
	}

	// Strip the file names of the .root extension
	for (auto& fileName : recPointFileNames) {
		size_t pos = fileName.find(".root");
		if (pos != std::string::npos) {
			fileName.erase(pos);
		}
	}

	if (read) {
		for (int iSet = 0; iSet < nSets; ++iSet) {
			std::cout << "Reading " << recPointFileNames[iSet] << std::endl;
			if (isFT0) {
				ReadFT0RecPoints(recPointFileNames[iSet], false, true);
			}
			else {
				std::cout << "FV0/FDD not implemented" << std::endl;
				return 1;
			}
		}
	}

	std::vector<std::unique_ptr<TFile>> recPointFiles;
	std::vector<TH1I*> hChannels;

	for (int iSet = 0; iSet < nSets; ++iSet) {
		std::string recPointFileName = Form("%sread.root", recPointFileNames[iSet].c_str());
		std::cout << "Reading " << recPointFileName << std::endl;
		recPointFiles.emplace_back(TFile::Open(recPointFileName.c_str(), "READ"));
		if (!recPointFiles[iSet] || recPointFiles[iSet]->IsZombie()) {
			std::cout << "Could not open file " << recPointFileName << std::endl;
			return 1;
		}

		hChannels.emplace_back((TH1I*)recPointFiles[iSet]->Get("hChannels"));
		hChannels.back()->SetName(Form("hChannels_%s", setLabels[iSet].c_str()));
	}

	// Draw histograms
	TCanvas* cChannels = new TCanvas("cChannels", "Channel statistics");
	TLegend* legChannels = new TLegend(0.1, 0.7, 0.48, 0.9);
	for (int iSet = 0; iSet < nSets; ++iSet) {
		TH1* h = hChannels[iSet]->DrawCopy(iSet == 0 ? "" : "sames", "");
		h->SetLineColor(iSet + 2);
		legChannels->AddEntry(h, setLabels[iSet].c_str(), "l");
		cChannels->Update();
		UpdateStatBox(h, iSet);
	}
	legChannels->Draw();

	// Write histgrams to file
	if (writeToFile) {
		std::string outputFileName = Form("%s_comparedrecpoints.root", detector.c_str());
		std::cout << "Writing histograms to file " << outputFileName << std::endl;
		std::unique_ptr<TFile> outputFile(TFile::Open(outputFileName.c_str(), "RECREATE"));
		outputFile->cd();
		for (int iSet = 0; iSet < nSets; ++iSet) {
			hChannels[iSet]->Write();
		}
		outputFile->Close();
	}

	return 0;
}