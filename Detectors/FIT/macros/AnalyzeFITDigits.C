#include <gsl/span>
#if !defined(__CLING__) || defined(__ROOTCLING__)

#include <cstddef>

#include <iostream>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "TCanvas.h"
#include "TFile.h"
#include "TH1.h"
#include "TLegend.h"
#include "TPaveStats.h"
#include "TTree.h"
#include "TTreeReader.h"

#include "DataFormatsFIT/Triggers.h"

#include "DataFormatsFT0/ChannelData.h"
#include "DataFormatsFT0/Digit.h"
#include "DataFormatsFV0/ChannelData.h"
#include "DataFormatsFV0/Digit.h"
#include "DataFormatsFDD/ChannelData.h"
#include "DataFormatsFDD/Digit.h"

#endif

// #include "DataFormatsFT0/ChannelData.h"
// #include "DataFormatsFT0/Digit.h"
// #include "DataFormatsFV0/ChannelData.h"
// #include "DataFormatsFV0/Digit.h"
// #include "DataFormatsFDD/ChannelData.h"
// #include "DataFormatsFDD/Digit.h"

// class o2::ft0::Digit;
// class o2::ft0::ChannelData;

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

// Read one digit file
template<typename Digit, typename ChannelData>
int ReadFITDigits(std::string detector = "FT0",
                  std::string fileName = "ft0digits.root",
									const bool draw = false,
									const bool writeToFile = true) {
	const bool isFT0 = detector == "FT0";
	const bool isFV0 = detector == "FV0";
	const bool isFDD = detector == "FDD";

	if (!isFT0 && !isFV0 && !isFDD) {
		std::cout << "Detector must be FT0/FV0/FDD" << std::endl;
		return 1;
	}

	// Strip the file name of the .root extension
	size_t pos = fileName.find(".root");
	if (pos != std::string::npos) {
		fileName.erase(pos);
	}

	// Open input digit file and tree
	std::unique_ptr<TFile> digitFile(TFile::Open(Form("%s.root", fileName.c_str()), "READ"));
	if (!digitFile || digitFile->IsZombie()) {
		std::cout << "Could not open file " << fileName << std::endl;
		return 1;
	}

	std::unique_ptr<TTree> digitTree(dynamic_cast<TTree*>(digitFile->Get("o2sim")));
	if (!digitTree) {
		std::cout << "Could not find tree o2sim in file " << fileName << std::endl;
		return 1;
	}

	std::vector<Digit> digits;
	std::vector<Digit>* pDigits = &digits;
	std::vector<ChannelData> channelData;
	std::vector<ChannelData>* pChannels = &channelData;

	std::string digitBranchName = isFT0 ? "FT0DIGITSBC" : isFV0 ? "FV0DigitBC" : isFDD ? "FDDDIGITBC" : "";
	std::string channelBranchName = isFT0 ? "FT0DIGITSCH" : isFV0 ? "FV0DigitCh" : isFDD ? "FDDDIGITSCH" : "";

	digitTree->SetBranchAddress(digitBranchName.c_str(), &pDigits);
	digitTree->SetBranchAddress(channelBranchName.c_str(), &pChannels);

	// Declare histograms
	TH1I* hBcTimeA = new TH1I("hBcTimeA", "BC time A;Time [TDC]", 1001, -500, 500);
	TH1I* hBcTimeC = new TH1I("hBcTimeC", "BC time C;Time [TDC]", 1001, -500, 500);
	TH1I* hChID = new TH1I("hChID", "Channel ID;Channel ID;counts", 208, 0, 208);

	for (Long64_t iEntry = 0; iEntry < digitTree->GetEntries(); iEntry++) {
		digitTree->GetEntry(iEntry);

		for (auto& digit : digits) {
			hBcTimeA->Fill(digit.mTriggers.getTimeA());
			hBcTimeC->Fill(digit.mTriggers.getTimeC());

			const gsl::span<const ChannelData> channels = digit.getBunchChannelData(channelData);
			for (const auto& channel : channels) {
				hChID->Fill(channel.ChId);
			}
		}
	}

	// Draw histogams
	if (draw) {
		TCanvas* cTimeA = new TCanvas("cBcTimeA", "BC Time A");
		hBcTimeA->DrawCopy();

		TCanvas* cTimeC = new TCanvas("cBcTimeC", "BC Time C");
		hBcTimeC->DrawCopy();

		TCanvas* cChID = new TCanvas("cChID", "Channel ID");
		hChID->DrawCopy();
	}

	// Write histograms to file
	if (writeToFile) {
		std::string outputFileName = Form("%sread.root", fileName.c_str());
		std::cout << "Writing histograms to file " << outputFileName << std::endl;
		TFile* fOutputFile = new TFile(outputFileName.c_str(), "RECREATE");

		fOutputFile->cd();
		hBcTimeA->Write();
		hBcTimeC->Write();
		hChID->Write();

		fOutputFile->Close();
	}

	return 0;
}

// Read one (or more) digit file(s) (and compare them in case of many)
int AnalyzeFITDigits(std::string detector = "FT0",
                     const bool read = true,
										 const bool writeToFile = true,
										 std::initializer_list<std::string> fileNames = {"ft0digits.root"},
										 std::initializer_list<std::string> labels = {"1"}) {
	const bool isFT0 = detector == "FT0";
	const bool isFV0 = detector == "FV0";
	const bool isFDD = detector == "FDD";

	if (!isFT0 && !isFV0 && !isFDD) {
		std::cout << "Detector must be FT0/FV0/FDD" << std::endl;
		return 1;
	}

	const int nSets = fileNames.size();
	
	std::vector<std::string> digitFileNames;
	digitFileNames.insert(digitFileNames.end(), fileNames.begin(), fileNames.end());

	std::vector<std::string> setLabels;
	setLabels.insert(setLabels.end(), labels.begin(), labels.end());

	if (digitFileNames.size() != setLabels.size()) {
		std::cout << "Number of file names and labels must be the same" << std::endl;
		return 1;
	}

	// Strip the file names of the .root extension
	for (auto& digitFileName : digitFileNames) {
		size_t pos = digitFileName.find(".root");
		if (pos != std::string::npos) {
			digitFileName.erase(pos);
		}
	}

	if (read) {
		for (int iSet = 0; iSet < nSets; iSet++) {
			std::cout << "Reading " << digitFileNames.at(iSet) << std::endl;
			bool readResult = 1;
			if (isFT0) {
				readResult = ReadFITDigits<o2::ft0::Digit, o2::ft0::ChannelData>("FT0", digitFileNames.at(iSet), false, true);
			} else if (isFV0) {
				readResult = ReadFITDigits<o2::fv0::Digit, o2::fv0::ChannelData>("FV0", digitFileNames.at(iSet), false, true);
			// } else if (isFDD) {
			// 	readResult = ReadFITDigits<o2::fdd::Digit, o2::fdd::ChannelData>("FDD", digitFileNames.at(iSet), false, true);
			}

			if (readResult) {
				std::cout << "Could not read file " << digitFileNames.at(iSet) << std::endl;
				return 1;
			}
		}
	}

	std::vector<std::unique_ptr<TFile>> digitFiles;
	std::vector<TH1I*> hTimeA;
	std::vector<TH1I*> hTimeC;
	std::vector<TH1I*> hChID;

	for (int iSet = 0; iSet < nSets; iSet++) {
		std::cout << "Set up files and trees etc." << std::endl;
		digitFiles.push_back(std::unique_ptr<TFile>(TFile::Open(TString::Format("%sread.root", digitFileNames.at(iSet).c_str()), "READ")));
		if (!digitFiles.back() || digitFiles.back()->IsZombie()) {
			std::cout << "Could not open file " << digitFileNames.at(iSet) << std::endl;
			return 1;
		}

		hTimeA.push_back(dynamic_cast<TH1I*>(digitFiles.back()->Get("hBcTimeA")));
		hTimeA.back()->SetName(hTimeA.back()->GetName() + TString::Format("_%s", setLabels.at(iSet).c_str()));

		hTimeC.push_back(dynamic_cast<TH1I*>(digitFiles.back()->Get("hBcTimeC")));
		hTimeC.back()->SetName(hTimeC.back()->GetName() + TString::Format("_%s", setLabels.at(iSet).c_str()));

		hChID.push_back(dynamic_cast<TH1I*>(digitFiles.back()->Get("hChID")));
		hChID.back()->SetName(hChID.back()->GetName() + TString::Format("_%s", setLabels.at(iSet).c_str()));
	}

	// Draw histograms

	// Time A
	TCanvas* cTimeA = new TCanvas("cTimeA", "Time A");
	TLegend* legTimeA = new TLegend(0.1, 0.7, 0.48, 0.9);
	for (int iSet = 0; iSet < nSets; iSet++) {
		TH1* h = hTimeA[iSet]->DrawCopy(iSet == 0 ? "" : "sames", "");
		h->SetLineColor(iSet + 2);
		legTimeA->AddEntry(h, setLabels.at(iSet).c_str(), "l");
		cTimeA->Update();
		UpdateStatBox(h, iSet);
	}
	legTimeA->Draw();

	// Time C
	TCanvas* cTimeC = new TCanvas("cTimeC", "Time C");
	TLegend* legTimeC = new TLegend(0.1, 0.7, 0.48, 0.9);
	for (int iSet = 0; iSet < nSets; iSet++) {
		TH1* h = hTimeC[iSet]->DrawCopy(iSet == 0 ? "" : "sames", "");
		h->SetLineColor(iSet + 2);
		legTimeC->AddEntry(h, setLabels.at(iSet).c_str(), "l");
		cTimeC->Update();
		UpdateStatBox(h, iSet);
	}
	legTimeC->Draw();

	// Channel ID
	TCanvas* cChID = new TCanvas("cChID", "Channel ID");
	TLegend* legChID = new TLegend(0.1, 0.7, 0.48, 0.9);
	for (int iSet = 0; iSet < nSets; iSet++) {
		TH1* h = hChID[iSet]->DrawCopy(iSet == 0 ? "" : "sames", "");
		h->SetLineColor(iSet + 2);
		legChID->AddEntry(h, setLabels.at(iSet).c_str(), "l");
		cChID->Update();
		UpdateStatBox(h, iSet);
	}
	legChID->Draw();

	// Write histograms to file
	if (writeToFile) {
		TString outputFileName = TString::Format("comparedDigits.root");
		std::cout << "Writing histograms to file " << outputFileName.Data() << std::endl;
		TFile* fOutputFile = new TFile(outputFileName, "RECREATE");
		fOutputFile->cd();
		for (int iSet = 0; iSet < nSets; iSet++) {
			hTimeA[iSet]->Write();
			hTimeC[iSet]->Write();
		}
		cTimeA->Write();
		cTimeC->Write();
		fOutputFile->Close();
	}

	return 0;
}
