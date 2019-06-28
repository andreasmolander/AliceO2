// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file Geometry.h
/// \brief Base definition of FIT-V0+ geometry.
///
/// \author Maciej Slupecki, University of Jyvaskyla, Finland

#ifndef ALICEO2_FITV0_GEOMETRY_H_
#define ALICEO2_FITV0_GEOMETRY_H_

#include <vector>
#include <TGeoMatrix.h>
#include <TGeoVolume.h>

namespace o2
{
namespace v0
{
/// FIT-V0+ Geometry
class Geometry
{
 public:
  enum EGeoType {
    eUninitilized,
    eDummy,
    eOnlySensitive,
    eFull
  }; // Geometry type options possible to be initialized

  ///
  /// Default constructor.
  /// It must be kept public for root persistency purposes,
  /// but should never be called by the outside world
  Geometry() { mGeometryType = eUninitilized; };
  /// Standard constructor
  /// \param initType[in]  The type of geometry, that will be initialized
  ///                       -> initType == 0 => only sensitive detector parts
  ///                       -> initType == 1 => sensitive parts and rough structural elements
  ///                       -> initType == 2 => complete, detailed geometry (including screws, etc.)
  /// \return  -
  Geometry(EGeoType initType);
  /// Copy constructor.
  Geometry(const Geometry& geom);

  static constexpr float sEpsilon = 0.01;                  // variable used to make sure one spatial dimension is infinitesimaly larger than the other
  static constexpr float sDrSeparationScint = 0.03 + 0.04; // paint thickness + half of separation gap
  static constexpr float sDzScint = 4;                     // thickness of scintillator
  static constexpr float sGlobalPhiRotation = 0;           // global phi rotation (to enable making two detector halves, possible to separate vertically)
  static constexpr float sDySeparationScint = sDrSeparationScint;
  static constexpr int sBaseNumberOfSectors = 8; // number of sectors
  // TODO: Adjust the sZposition once the simulation geometry is implemented, T0 starts at 328
  // at sZposition==320, there is a gap (to be filled with fibers and support) of 8 cm between the plastic of V0+ and aluminum covers of T0+
  static constexpr float sZposition = 320 - sDzScint / 2;                                                // z-position of the geometrical center of the detectors sensitive part
  static constexpr int sNumberOfRings = 5;                                                               // number of rings
  static constexpr float sRingRadiiScint[sNumberOfRings + 1] = { 4.01, 7.3, 12.9, 21.25, 38.7, 72.115 }; // average ring radii
  static constexpr float sRingInnerRadiusDx = -0.15;                                                     // shift of the inner radius origin
  static constexpr char sCellTypes[sBaseNumberOfSectors] = { 'a', 'b', 'b', 'a', 'a', 'b', 'b', 'a'};

  const std::vector<std::string> getSensitiveVolumeNames() { return mvSensitiveVolumeNames; };

 private:
  void initializeVectors();
  void initializeScintCells();
  void initializeLuts();

  void buildGeometry();
  void assembleScintSectors(TGeoVolumeAssembly* volV0);
  TGeoVolumeAssembly* buildScintSector(uint16_t iSector);

  inline static const std::string sScintSectorName = "SCINTSECTOR";
  inline static const std::string sScintCellName = "SCINTCELL";

  std::vector<std::string> mvSensitiveVolumeNames;

  std::vector<float> mvrAvgScint;         // average ring radii (index 0 -> ring 1 min, index 1 -> ring 1 max and ring 2 min, ... index 5 -> ring 5 max)
  // The following radii include separation between rings
  std::vector<float> mvrMinScint;         // inner radii of a ring (.at(0) -> ring 1, .at(4) -> ring 5)
  std::vector<float> mvrMaxScint;         // outer radii of a ring (.at(0) -> ring 1, .at(4) -> ring 5)
  std::vector<TGeoMatrix*> mvSectorTrans; // transformations of sectors (.at(0) -> sector 1)

  int mGeometryType; // same meaning as initType in constructor

  ClassDefNV(Geometry, 1);
};
} // namespace v0
} // namespace o2
#endif
