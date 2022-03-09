#if !defined(__CLING__) || defined(__ROOTCLING__)
#include "DetectorsCommonDataFormats/DetID.h"
#include "DetectorsCommonDataFormats/DetectorNameConf.h"
#include "DetectorsCommonDataFormats/AlignParam.h"
#include "DetectorsBase/GeometryManager.h"
#include "CCDB/CcdbApi.h"
#include "FV0Base/Geometry.h"
#include <TRandom.h>
#include <TFile.h>
#include <vector>
#include <fmt/format.h>
#endif

using o2::fv0::Geometry;
using AlgPar = std::array<double, 6>;

AlgPar generateMisalignment(double x, double y, double z, double psi, double theta, double phi);
AlgPar generateExactMisalignment(double x, double y, double z, double psi, double theta, double phi);

void FV0Misaligner(const std::string& ccdbHost = "http://ccdb-test.cern.ch:8080", long tmin = 0, long tmax = -1,
                   double x = 0., double y = 0., double z = 0., double psi = 0., double theta = 0., double phi = 0.,
                   const std::string& objectPath = "",
                   const std::string& fileName = "FV0Alignment.root",
                   const int exactAlignment = 0,
                   const std::string& comment = "Alignment object for FV0")
{
  std::vector<o2::detectors::AlignParam> params;
  o2::base::GeometryManager::loadGeometry("", false);
  AlgPar pars;
  bool glo = true;

  o2::detectors::DetID detFV0("FV0");

  if (exactAlignment) {
    pars = generateExactMisalignment(x, y, z, psi, theta, phi);
  } else {
    pars = generateMisalignment(x, y, z, psi, theta, phi);
  }

  for (auto& symName : {Geometry::getDetectorRightSymName(), Geometry::getDetectorLeftSymName()}) {
    params.emplace_back(symName.c_str(), -1, pars[0], pars[1], pars[2], pars[3], pars[4], pars[5], glo);
  }

  if (!ccdbHost.empty()) {
    std::string path = objectPath.empty() ? o2::base::DetectorNameConf::getAlignmentPath(detFV0) : objectPath;
    LOGP(info, "Storing alignment object on {}/{}", ccdbHost, path);
    o2::ccdb::CcdbApi api;
    map<string, string> metadata; // can be empty
    metadata["comment"] = comment;
    api.init(ccdbHost.c_str()); // or http://localhost:8080 for a local installation
    // store abitrary user object in strongly typed manner
    api.storeAsTFileAny(&params, path, metadata, tmin, tmax);
  }

  if (!fileName.empty()) {
    LOGP(info, "Storing FV0 alignment in local file {}", fileName);
    TFile algFile(fileName.c_str(), "recreate");
    algFile.WriteObjectAny(&params, "std::vector<o2::detectors::AlignParam>", "alignment");
    algFile.Close();
  }
}
AlgPar generateMisalignment(double x, double y, double z, double psi, double theta, double phi)
{
  AlgPar pars;
  pars[0] = gRandom->Gaus(0, x);
  pars[1] = gRandom->Gaus(0, y);
  pars[2] = gRandom->Gaus(0, z);
  pars[3] = gRandom->Gaus(0, psi);
  pars[4] = gRandom->Gaus(0, theta);
  pars[5] = gRandom->Gaus(0, phi);
  return std::move(pars);
}

AlgPar generateExactMisalignment(double x, double y, double z, double psi, double theta, double phi)
{
  AlgPar pars;
  pars[0] = x;
  pars[1] = y;
  pars[2] = z;
  pars[3] = psi;
  pars[4] = theta;
  pars[5] = phi;
  return std::move(pars);
}