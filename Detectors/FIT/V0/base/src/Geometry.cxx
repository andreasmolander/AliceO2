// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
#include <iomanip>
//#include <TVector3.h>
#include "V0Base/Geometry.h"

#include <TGeoManager.h>
#include <TGeoBBox.h>
#include <TGeoTube.h>
#include <TGeoCone.h>
#include <TGeoCompositeShape.h>
#include <TGeoMedium.h>
#include <TGeoVolume.h>
#include <TGeoMatrix.h>
#include <FairLogger.h>
#include <cmath>
#include <sstream>

ClassImp(o2::v0::Geometry);

using namespace o2::v0;

Geometry::Geometry(EGeoType initType)
{
  mGeometryType = initType;
  initializeGeometry();
  buildGeometry();
}

Geometry::Geometry(const Geometry& geom)
{
  this->mGeometryType = geom.mGeometryType;
}

const int Geometry::getCurrentCellId(TVirtualMC* fMC) {
  int ring = -1;
  int sector = -1;

  fMC->CurrentVolOffID(0, ring);
  fMC->CurrentVolOffID(1, sector);

  return sector + 8 * (ring - 1);
}

void Geometry::initializeGeometry()
{
  initializeVectors();
  initializeSensVols();
  if (mGeometryType == eFull) {
    initializeNonSensVols();
  }
  initializeLuts();
}

void Geometry::initializeVectors()
{
  initializeCellRadii();
  initializeSectorTransformations();
  initializeFiberRadii();
}

void Geometry::initializeCellRadii()
{
  LOG(INFO) << "FV0 Geometry::initializeCellRadii(): Initializing FV0 cell ring radii.";

  // Index of mRAvgRing is NOT linked directly to any ring number
  mRAvgRing.assign(sRingRadiiScint, sRingRadiiScint + sNumberOfRings + 1);

  // Set real scintillator radii (reduced by paint thickness and separation gap)
  for (uint16_t ir = 0; ir < mRAvgRing.size() - 1; ir++) {
    mRMinScint.push_back(mRAvgRing.at(ir) + sDrSeparationScint);
    mRMaxScint.push_back(mRAvgRing.at(ir + 1) - sDrSeparationScint);
    LOG(INFO) << "FV0 Geometry::initializeCellRadii(): Ring " << ir << " min: " << mRMinScint.at(ir) << ", max: " << mRMaxScint.at(ir);
  }
  // Now indices of mRMinScint and mRMaxScint correspond to the same ring
}

void Geometry::initializeSectorTransformations()
{
  LOG(INFO) << "FV0 Geometry::initializeSectorTransformations(): Initializing FV0 sector transformations.";

  for (uint16_t isector = 1; isector <= sBaseNumberOfSectors; isector++) {
    // isector = 1 corresponds to the first sector counter-clockwise from the y-axis + global azimuthal rotation
    // isector = 2 corresponds to the next sector in counter-clockwise direction

    // if the sector is rotated, this is the angle (plus an extra 45 for "b"-type cells)
    float phi = isector * 45;

    std::stringstream ssRotName, ssReflName, ssTotalTransName;
    ssRotName << "FV0_rotationSector" << isector;
    ssReflName << "FV0_reflectSector" << isector;
    ssTotalTransName << "FV0_totalTransformSector" << isector;

    TGeoRotation* rot = new TGeoRotation(ssRotName.str().c_str());            // rotation of the sector
    TGeoRotation* refl = new TGeoRotation(ssReflName.str().c_str());          // reflection of the sector
    TGeoHMatrix* totTrans = new TGeoHMatrix(ssTotalTransName.str().c_str());  // the combined transformation

    // The reference to "a" and "b" can be understood with the CAD drawings of the detector.
    switch (isector)
    {
    case 1: // "a"-mirror
      refl->ReflectX(true);
      break;
    case 2: // "b"-mirror
      refl->ReflectX(true);
      break;
    case 3: // "b"
      rot->SetAngles(phi + 45, 0, 0);
      break;
    case 4: // "a"
      rot->SetAngles(phi, 0, 0);
      break;
    case 5: // "a"-mirror
      refl->ReflectY(true);
      break;
    case 6: // "b"-mirror
      refl->ReflectY(true);
      break;
    case 7: // "b"
      rot->SetAngles(phi + 45, 0, 0);
      break;
    case 8: // "a"
      rot->SetAngles(phi, 0, 0);
      break;
    default:
      break;
    }

    // Rotate the sector with the global rotation angle
    rot->SetAngles(rot->GetPhiRotation() + sGlobalPhiRotation, 0, 0);
    
    // Combine the rotation and reflection
    *totTrans = *rot * *refl;
    totTrans->RegisterYourself();
    mSectorTrans.push_back(totTrans);
  }
}

void Geometry::initializeFiberRadii()
{
  mRMinFiber.push_back(0);
  mRMinFiber.push_back(sDrMinAluCone + sEpsilon);
  mRMinFiber.push_back(sDrMinAluFront + sEpsilon);

  mRMaxFiber.push_back(sDrMinAluCone);
  mRMaxFiber.push_back(sDrMinAluFront);
  mRMaxFiber.push_back(mRMaxScint.back());
}

void Geometry::initializeSensVols()
{
  initializeScintCells();
}

void Geometry::initializeNonSensVols()
{
  initializePlasticCells();
  initializeFibers();
  initializeMetalContainer();
}

void Geometry::initializeCells(std::string cellType, float zThickness, TGeoMedium* medium) {
  // Creating the two types of cells, "a" and "b", for each ring.
  // All sectors can be assembled with these cells.
  //
  // The reference to "a" and "b" can be understood with the CAD drawings of the detector.

  LOG(INFO) << "FV0 Geometry::initializeCells(): Initializing " << cellType << " cells.";
  
  float xHoleCut = 0.2;                               // width of extension of hole 1, 2 and 7 in the "a" cell
  float dxHole = sDrSeparationScint + xHoleCut;       // x-placement of holes 1, 2 and 7 in the "a" cell

  // Sector separation gap shape
  std::string secSepShapeName = "FV0_" + cellType + "SectorSeparation";
  new TGeoBBox(secSepShapeName.c_str(), mRMaxScint.back() + sEpsilon, sDxAluCover + sDrSeparationScint, zThickness / 2);

  // Sector separation gap rotations
  std::string secSepRot45Name = "FV0_" + cellType + "SecSepRot45";
  std::string secSepRot90Name = "FV0_" + cellType + "SecSepRot90";

  createAndRegisterRot(secSepRot45Name, 45, 0, 0);  
  createAndRegisterRot(secSepRot90Name, 90, 0, 0);

  // Hole shapes
  std::string holeSmallName = "FV0_" + cellType + "HoleSmall";
  std::string holeLargeName = "FV0_" + cellType + "HoleLarge";
  std::string holeSmallCutName = "FV0_" + cellType + "HoleSmallCut";
  std::string holeLargeCutName = "FV0_" + cellType + "HoleLargeCut";

  new TGeoTube(holeSmallName.c_str(), 0, sDrHoleSmall, zThickness / 2);  
  new TGeoTube(holeLargeName.c_str(), 0, sDrHoleLarge, zThickness / 2);
  new TGeoBBox(holeSmallCutName.c_str(), xHoleCut, sDrHoleSmall, zThickness / 2);
  new TGeoBBox(holeLargeCutName.c_str(), xHoleCut, sDrHoleSmall, zThickness / 2);
  
  for (int ir = 0; ir < sNumberOfRings; ir++) {

    // Radii without separation
    float rMin = mRAvgRing.at(ir);
    float rMax = mRAvgRing.at(ir + 1);
    float rMid = rMin + (rMax - rMin) / 2;
    
    // "a"-type cell
    // 
    // Initial placement:
    //
    // y
    // ^
    // |  * * * 
    // |  * a * *
    // |  * * * 
    // |  * *
    // |
    // 0--------------> x

    std::stringstream aCellName;
    aCellName << "FV0" << cellType << sCellName << "a" << ir + 1;

    LOG(INFO) << "FV0 Geometry::initializeCells(): Initializing cell " << aCellName.str();

    // Base shape
    std::string aCellShapeName = aCellName.str() + "Shape";

    // The cells in the innermost ring have a slightly shifted inner radius origin.
    if (ir == 0) {
      // The innermost "a"-type cell
      std::string a1CellShapeFullName = aCellShapeName + "Full";
      std::string a1CellShapeHoleCutName = aCellShapeName + "HoleCut";
      std::string a1CellShapeHoleCutTransName = a1CellShapeHoleCutName + "Trans";
      
      new TGeoTubeSeg(a1CellShapeFullName.c_str(), 0, mRMaxScint.at(ir), zThickness / 2 - sEpsilon, 45, 90);
      new TGeoTube(a1CellShapeHoleCutName.c_str(), 0, mRMinScint.at(ir), zThickness);

      createAndRegisterTrans(a1CellShapeHoleCutTransName, sRingInnerRadiusDx, 0, 0);

      std::string a1BoolFormula = a1CellShapeFullName + "-" + a1CellShapeHoleCutName + ":" + a1CellShapeHoleCutTransName;
      new TGeoCompositeShape(aCellShapeName.c_str(), a1BoolFormula.c_str());
    } else {
      // The rest of the "a"-type cells
      new TGeoTubeSeg(aCellShapeName.c_str(), mRMinScint.at(ir), mRMaxScint.at(ir), zThickness / 2, 45, 90);
    }

    // Translations for screw holes
    //
    // 1 = outer left
    // 2 = inner left
    // 3 = outer right
    // 4 = inner right
    // 5 = outer middle
    // 6 = inner middle
    // 7 = half-lenght left
    // 8 = half-length right
    //
    // holes 1, 2 and 7 are sligtly shifted along the rim of the cell

    std::string aHole1TransName = aCellName.str() + "Hole1Trans";
    std::string aHole2TransName = aCellName.str() + "Hole2Trans";
    std::string aHole3TransName = aCellName.str() + "Hole3Trans";
    std::string aHole4TransName = aCellName.str() + "Hole4Trans";
    std::string aHole5TransName = aCellName.str() + "Hole5Trans";
    std::string aHole6TransName = aCellName.str() + "Hole6Trans";
    std::string aHole7TransName = aCellName.str() + "Hole7Trans";
    std::string aHole8TransName = aCellName.str() + "Hole8Trans";
    std::string aHole1CutTransName = aCellName.str() + "Hole1CutTrans";
    std::string aHole2CutTransName = aCellName.str() + "Hole2CutTrans";
    std::string aHole7CutTransName = aCellName.str() + "Hole7CutTrans";

    createAndRegisterTrans(aHole1TransName, dxHole, cos(asin(dxHole/rMax)) * rMax, 0);
    createAndRegisterTrans(aHole2TransName, dxHole, cos(asin(dxHole/rMin)) * rMin, 0);
    createAndRegisterTrans(aHole3TransName, sin(45 * M_PI/180) * rMax, cos(45 * M_PI/180) * rMax, 0);
    createAndRegisterTrans(aHole4TransName, sin(45 * M_PI/180) * rMin, cos(45 * M_PI/180) * rMin, 0);
    createAndRegisterTrans(aHole5TransName, sin(22.5 * M_PI/180) * rMax, cos(22.5 * M_PI/180) * rMax, 0);
    createAndRegisterTrans(aHole6TransName, sin(22.5 * M_PI/180) * rMin, cos(22.5 * M_PI/180) * rMin, 0);
    createAndRegisterTrans(aHole7TransName, dxHole, cos(asin(dxHole/rMid)) * rMid, 0);
    createAndRegisterTrans(aHole8TransName, sin(45 * M_PI/180) * rMid, cos(45 * M_PI/180) * rMid, 0);
    createAndRegisterTrans(aHole1CutTransName, 0, cos(asin(dxHole/rMax)) * rMax, 0);
    createAndRegisterTrans(aHole2CutTransName, 0, cos(asin(dxHole/rMin)) * rMin, 0);
    createAndRegisterTrans(aHole7CutTransName, 0, cos(asin(dxHole/rMid)) * rMid, 0);

    // Composite shape
    std::string aBoolFormula = aCellShapeName;

    // sector separation
    aBoolFormula += "-" + secSepShapeName + ":" + secSepRot45Name;
    aBoolFormula += "-" + secSepShapeName + ":" + secSepRot90Name;

    // outer holes
    aBoolFormula += "-" + ((ir < 2) ? holeSmallName : holeLargeName) + ":" + aHole1TransName;
    aBoolFormula += "-" + ((ir < 2) ? holeSmallCutName : holeLargeCutName) + ":" + aHole1CutTransName;
    aBoolFormula += "-" + ((ir < 2) ? holeSmallName : holeLargeName) + ":" + aHole3TransName;

    // inner holes
    if (ir > 0)  {      
      std::string screwHoleName = (ir < 3) ? holeSmallName : holeLargeName;
      std::string screwHoleCutName = (ir < 3) ? holeSmallCutName : holeLargeCutName;

      aBoolFormula += "-" + screwHoleName + ":" + aHole2TransName;
      aBoolFormula += "-" + screwHoleCutName + ":" + aHole2CutTransName;
      aBoolFormula += "-" + screwHoleName + ":" + aHole4TransName;
    }

    // outer middle hole
    if (ir > 1) {
      aBoolFormula += "-" + holeLargeName + ":" + aHole5TransName;
    }

    // inner middle hole
    if (ir > 2) {
      aBoolFormula += "-" + holeLargeName + ":" + aHole6TransName;
    }

    // half-length holes
    if (ir == 4) {
      aBoolFormula += "-" + holeLargeName + ":" + aHole7TransName;
      aBoolFormula += "-" + holeLargeCutName + ":" + aHole7CutTransName;
      aBoolFormula += "-" + holeLargeName + ":" + aHole8TransName;
    }

    LOG(INFO) << "FV0 Geometry::initializeCells(): Composite shape formula: " << aBoolFormula;

    std::string aCellCSName = aCellName.str() + "CS";
    TGeoCompositeShape* aCellCs = new TGeoCompositeShape(aCellCSName.c_str(), aBoolFormula.c_str());

    // Cell volume
    TGeoVolume* aCell = new TGeoVolume(aCellName.str().c_str(), aCellCs, medium);
    mvSensitiveVolumeNames.push_back(aCell->GetName());

    // "b"-type cells
    // 
    // Initial placement:
    //
    // y
    // ^
    // |     
    // |        *
    // |      * * *
    // |    * * b * *
    // |      * * * *
    // |
    // 0--------------> x

    std::stringstream bCellName;
    bCellName << "FV0" << cellType << sCellName << "b" << ir + 1;

    LOG(INFO) << "FV0 Geometry::initializeCells(): Initializing cell " << bCellName.str();

    // Base shape
    std::string bCellShapeName = bCellName.str() + "Shape";

    // The cells in the innermost ring are slightly different than the rest
    if (ir == 0) {
      // The innermost "b"-type cell
      std::string b1CellShapeFullName = bCellShapeName + "Full";
      std::string b1CellShapeHoleCutName = bCellShapeName + "Cut";
      std::string b1CellShapeHoleCutTransName = b1CellShapeHoleCutName + "Trans";

      new TGeoTubeSeg(b1CellShapeFullName.c_str(), 0, mRMaxScint.at(ir), zThickness / 2 - sEpsilon, 0, 45);
      new TGeoTube(b1CellShapeHoleCutName.c_str(), 0, mRMinScint.at(ir), zThickness);
      
      createAndRegisterTrans(b1CellShapeHoleCutTransName, sRingInnerRadiusDx, 0, 0);

      std::string b1BoolFormula = b1CellShapeFullName + "-" + b1CellShapeHoleCutName + ":" + b1CellShapeHoleCutTransName;
      new TGeoCompositeShape(bCellShapeName.c_str(), b1BoolFormula.c_str());
    } else {
      // The rest of the "b"-type cells
      new TGeoTubeSeg(bCellShapeName.c_str(), mRMinScint.at(ir), mRMaxScint.at(ir), zThickness / 2, 0, 45);
    }

    // Translations for holes
    //
    // 1 = outer left
    // 2 = inner left
    // 3 = outer right
    // 4 = inner right
    // 5 = outer middle
    // 6 = inner middle
    // 7 = half-lenght left
    // 8 = half-length right

    std::string bHole1TransName = bCellName.str() + "Hole1Trans";
    std::string bHole2TransName = bCellName.str() + "Hole2Trans";
    std::string bHole3TransName = bCellName.str() + "Hole3Trans";
    std::string bHole4TransName = bCellName.str() + "Hole4Trans";
    std::string bHole5TransName = bCellName.str() + "Hole5Trans";
    std::string bHole6TransName = bCellName.str() + "Hole6Trans";
    std::string bHole7TransName = bCellName.str() + "Hole7Trans";
    std::string bHole8TransName = bCellName.str() + "Hole8Trans";

    createAndRegisterTrans(bHole1TransName, sin(45 * M_PI/180) * rMax, cos(45 * M_PI/180) * rMax, 0);
    createAndRegisterTrans(bHole2TransName, sin(45 * M_PI/180) * rMin, cos(45 * M_PI/180) * rMin, 0);
    createAndRegisterTrans(bHole3TransName, rMax, 0, 0);
    createAndRegisterTrans(bHole4TransName, rMin, 0, 0);
    createAndRegisterTrans(bHole5TransName, cos(22.5 * M_PI/180) * rMax, sin(22.5 * M_PI/180) * rMax, 0);
    createAndRegisterTrans(bHole6TransName, cos(22.5 * M_PI/180) * rMin, sin(22.5 * M_PI/180) * rMin, 0);
    createAndRegisterTrans(bHole7TransName, sin(45 * M_PI/180) * rMid, cos(45 * M_PI/180) * rMid, 0);
    createAndRegisterTrans(bHole8TransName, rMid, 0, 0);

    // Composite shape
    std::string bBoolFormula = bCellShapeName;

    // sector separation
    bBoolFormula += "-" + secSepShapeName;
    bBoolFormula += "-" + secSepShapeName + ":" + secSepRot45Name;

    // outer holes
    bBoolFormula += "-" + holeSmallName + ":" + bHole1TransName;
    bBoolFormula += "-" + holeSmallName + ":" + bHole3TransName;

    // inner holes
    if (ir > 0) {
      std::string holeName = (ir < 3) ? holeSmallName : holeLargeName;

      bBoolFormula += "-" + holeName + ":" + bHole2TransName;
      bBoolFormula += "-" + holeName + ":" + bHole4TransName;
    }

    // outer middle hole
    if (ir > 1) {
      bBoolFormula += "-" + holeLargeName + ":" + bHole5TransName;
    }

    // inner middle hole
    if (ir > 2) {
      bBoolFormula += "-" + holeLargeName + ":" + bHole6TransName;
    }

    // half-lenght holes
    if (ir == 4) {
      bBoolFormula += "-" + holeLargeName + ":" + bHole7TransName;
      bBoolFormula += "-" + holeLargeName + ":" + bHole8TransName;
    }

    std::string bCellCSName = bCellName.str() + "CS";
    TGeoCompositeShape* bCellCs = new TGeoCompositeShape(bCellCSName.c_str(), bBoolFormula.c_str());

    // Cell volume
    TGeoVolume* bCell = new TGeoVolume(bCellName.str().c_str(), bCellCs, medium);
    mvSensitiveVolumeNames.push_back(bCell->GetName());
  }
}

void Geometry::initializeScintCells()
{
  TGeoMedium* medium = gGeoManager->GetMedium("V0_Scintillator$");
  initializeCells(sScintName, sDzScint, medium);
}

void Geometry::initializePlasticCells()
{
  TGeoMedium* medium = gGeoManager->GetMedium("V0_Plastic$");
  initializeCells(sPlastName, sDzPlast, medium);
}

void Geometry::initializeFibers()
{
  LOG(INFO) << "FVO Geometry::initializeFibers(): Initializing fibers";

  int numberOfFiberVols = mRMinFiber.size();
  float dzFibers = sDzAlu - sDzAluBack - sDzAluFront - sDzScint - sDzPlast - 2 * sEpsilon;  // depth of the fiber volumes

  TGeoMedium* medFiberInner = gGeoManager->GetMedium("FiberInner$");
  TGeoMedium* medFiberMiddle = gGeoManager->GetMedium("FiberMiddle$");
  TGeoMedium* medFiberOuter = gGeoManager->GetMedium("FiberOuter$");
  TGeoMedium* medFiber[] = { medFiberInner, medFiberMiddle, medFiberOuter };

  std::string fiberName = "FV0_Fibers";   // No volume with this name

  std::string fiberSepCutName = fiberName + "SepCut";
  std::string fiberConeCutName = fiberName + "ConeCut";
  std::string fiberHoleCutName = fiberName + "HoleCut";

  std::string fiberTransName = fiberName + "Trans";
  std::string fiberConeCutTransName = fiberConeCutName + "Trans";
  std::string fiberHoleCutTransName = fiberHoleCutName + "Trans";

  new TGeoBBox(fiberSepCutName.c_str(), sDxAluCover + sDrSeparationScint, mRMaxFiber.back() + sEpsilon, dzFibers / 2 + sEpsilon );
  new TGeoConeSeg(fiberConeCutName.c_str(), sDzAluCone / 2 + sEpsilon, 0, sDrMinAluCone + sXYThicknessAluCone + sEpsilon, 0, sDrMinAluFront + sEpsilon, -90, 90);
  new TGeoTube(fiberHoleCutName.c_str(), 0, mRMinScint.front(), dzFibers / 2 + sEpsilon);
  
  createAndRegisterTrans(fiberTransName, 0 , 0, sZFiber);
  createAndRegisterTrans(fiberConeCutTransName, 0, 0, sZCone);
  createAndRegisterTrans(fiberHoleCutTransName, sRingInnerRadiusDx, 0, sZFiber);

  for (int i = 0; i < numberOfFiberVols; i++) {
    std::stringstream fiberShapeName;
    fiberShapeName << fiberName << i + 1;

    LOG(INFO) << "FV0 Geometry::initializeFibers(): Initializing fiber volume " << fiberShapeName.str();

    new TGeoTubeSeg(fiberShapeName.str().c_str(), mRMinFiber.at(i), mRMaxFiber.at(i) - sEpsilon, dzFibers / 2, -90, 90);

    // Composite shape
    std::string boolFormula = "";
    boolFormula += fiberShapeName.str() + ":" + fiberTransName;
    boolFormula += "-" + fiberSepCutName + ":" + fiberTransName;
    boolFormula += "-" + fiberConeCutName + ":" + fiberConeCutTransName;

    if (i == 0) {
      // Cut out the hole in the innermost fiber volume
      boolFormula += "-" + fiberHoleCutName + ":" + fiberHoleCutTransName;
    }

    std::string fiberCSName = fiberShapeName.str() + "CS";
    TGeoCompositeShape* fiberCS = new TGeoCompositeShape(fiberCSName.c_str(), boolFormula.c_str());

    // Volume
    std::stringstream fiberName;
    fiberName << "FV0" << sFiberName << i + 1;
    if (!medFiber[i]) {
      LOG(WARN) << "FV0 Geometry::initializeFibers(): Medium for fiber volume " << fiberName.str() << " not found!";
    }
    new TGeoVolume(fiberName.str().c_str(), fiberCS, medFiber[i]);
  }
}

void Geometry::initializeMetalContainer()
{
  // The metal container is constructed starting from the backplate. The backplate is positioned first, relative to
  // the scintillator cells. The rest of the container parts are positioned relative to the backplate.

  // TODO: Make position variables consistent, some are now global coordinates, and some are relative to some other part of the container

  // Backplate
  std::string backPlateName = "FV0_BackPlate";                        // the full backplate
  std::string backPlateStandName = backPlateName + "Stand";           // the stand part of the backplate
  std::string backPlateHoleName = backPlateName + "Hole";             // the hole in the middle of the backplate
  std::string backPlateHoleCutName = backPlateHoleName + "Cut";       // extension of the hole
  std::string backPlateStandTransName = backPlateStandName + "Trans"; // shift of the backplate stand
  std::string backPlateHoleTransName = backPlateHoleName + "Trans";   // shift of the backplate inner radius

  new TGeoTubeSeg(backPlateName.c_str(), 0, sDrMaxAluBack, sDzAluBack / 2, -90, 90);
  new TGeoBBox(backPlateStandName.c_str(), sDxAluStand / 2, (sDrMaxAluBack + sDyAluStand) / 2, sDzAluBack / 2);
  new TGeoTubeSeg(backPlateHoleName.c_str(), 0, sDrAluHole, sDzAluBack / 2, -90, 90);
  new TGeoBBox(backPlateHoleCutName.c_str(), -sXAluHole, sDrAluHole, sDzAluBack);
  
  createAndRegisterTrans(backPlateStandTransName, sDxAluStand / 2, - (sDrMaxAluBack + sDyAluStand) / 2, 0);
  createAndRegisterTrans(backPlateHoleTransName, sXAluHole, 0, 0);

  // Backplate composite shape
  std::string backPlateBoolFormula = "";
  backPlateBoolFormula += backPlateName;
  backPlateBoolFormula += "+" + backPlateStandName + ":" + backPlateStandTransName;
  backPlateBoolFormula += "-" + backPlateHoleName + ":" + backPlateHoleTransName;
  backPlateBoolFormula += "-" + backPlateHoleCutName;

  std::string backPlateCSName = backPlateName + "CompositeShape";
  std::string backPlateCSTransName = backPlateCSName + "Trans";

  new TGeoCompositeShape(backPlateCSName.c_str(), backPlateBoolFormula.c_str());
  createAndRegisterTrans(backPlateCSTransName, 0, 0, sZAluBack);

  // Frontplate
  float zPosFrontPlate = sZAluFront;                                                    // the z-position o the frontplate
  float dyFrontPlateStand = sDyAluStand + (sDrMaxAluFront - sDrMinAluFront) / 2;        // the height of the total stand overlapping with the rest of the plate
  float yPosFrontPlateStand = -sDrMaxAluFront - sDyAluStand + dyFrontPlateStand / 2;    // the y-position of the total stand

  std::string frontPlateName = "FV0_FrontPlate";
  std::string frontPlateStandName = frontPlateName + "Stand";
  std::string frontPlateTransName = frontPlateName + "Trans";
  std::string frontPlateStandTransName = frontPlateStandName + "Trans";

  new TGeoTubeSeg(frontPlateName.c_str(), sDrMinAluFront, sDrMaxAluFront, sDzAluFront / 2 , -90, 90);
  new TGeoBBox(frontPlateStandName.c_str(), sDxAluStand / 2, dyFrontPlateStand / 2, sDzAluBack / 2);

  createAndRegisterTrans(frontPlateTransName, 0, 0, zPosFrontPlate);
  createAndRegisterTrans(frontPlateStandTransName, sDxAluStand / 2, yPosFrontPlateStand, 0);

  // Frontplate cone composite shape
  std::string frontPlateBoolFormula = "";
  frontPlateBoolFormula += frontPlateName;
  frontPlateBoolFormula += "+" + frontPlateStandName + ":" + frontPlateStandTransName;

  std::string frontPlateCSName = frontPlateName + "CompositeName";

  new TGeoCompositeShape(frontPlateCSName.c_str(), frontPlateBoolFormula.c_str());

  // Frontplate cone
  float thicknessFrontPlateCone = sXYThicknessAluCone;      // radial thickness of frontplate cone in the xy-plane
  float zPosCone = sDzAluFront / 2 - sDzAluCone / 2;       // z-position of the frontplate cone relative to the frontplate

  std::string frontPlateConeName = "FV0_FrontPlateCone";                            // no volume with this name
  std::string frontPlateConeShieldName = frontPlateConeName + "Shield";             // the "sides" of the cone
  std::string frontPlateConeShieldTransName = frontPlateConeShieldName + "Trans";

  new TGeoConeSeg(frontPlateConeShieldName.c_str(),
                  sDzAluCone / 2,
                  sDrMinAluCone,
                  sDrMinAluCone + thicknessFrontPlateCone,
                  sDrMinAluFront - thicknessFrontPlateCone,
                  sDrMinAluFront,
                  -90,
                  90);
  createAndRegisterTrans(frontPlateConeShieldTransName, 0, 0, zPosCone);

  // Frontplate cone "bottom"
  float zPosConePlate = sDzAluFront / 2 - sDzAluCone + thicknessFrontPlateCone / 2;   // z-position of the cone bottom relative to the frontplate
  std::string frontPlateConePlateName = frontPlateConeName + "Plate";                  // the bottom of the cone

  new TGeoTubeSeg(frontPlateConePlateName.c_str(), 0, sDrMinAluCone + thicknessFrontPlateCone,
                  thicknessFrontPlateCone / 2, -90, 90);  

  // Frontplate cone bottom composite shape
  std::string frontPlateConePlateCSBoolFormula;
  frontPlateConePlateCSBoolFormula += frontPlateConePlateName;
  frontPlateConePlateCSBoolFormula += "-" + backPlateHoleName + ":" + backPlateHoleTransName;

  std::string frontPlateConePlateCSName = frontPlateConePlateName + "CompositeShape";
  std::string frontPlateConePlateCSTransName = frontPlateConePlateCSName + "Trans";
  new TGeoCompositeShape(frontPlateConePlateCSName.c_str(), frontPlateConePlateCSBoolFormula.c_str());
  createAndRegisterTrans(frontPlateConePlateCSTransName, 0, 0, zPosConePlate);

  // Frontplate cone composite shape
  std::string frontPlateConeCSBoolFormula = "";
  frontPlateConeCSBoolFormula += frontPlateConeShieldName + ":" + frontPlateConeShieldTransName;
  frontPlateConeCSBoolFormula += "+" + frontPlateConePlateCSName + ":" + frontPlateConePlateCSTransName;

  std::string frontPlateConeCSName = frontPlateConeName + "CompositeShape";
  new TGeoCompositeShape(frontPlateConeCSName.c_str(), frontPlateConeCSBoolFormula.c_str());

  // Shields
  float dzShieldGap = 0.7;                      // z-distance between the shields and the front- and backplate outer edges (in z-direction)
  float dzShield = sDzAlu - 2 * dzShieldGap;    // depth of the shields
  
  // Outer shield
  float zPosOuterShield = sZAluBack + abs(sZAluBack - zPosFrontPlate) / 2;   // z-position of the outer shield
  
  std::string outerShieldName = "FV0_OuterShield";
  std::string outerShieldTransName = outerShieldName + "Trans";
  
  new TGeoTubeSeg(outerShieldName.c_str(), sDrMinAluOuterShield, sDrMaxAluOuterShield, dzShield / 2, -90, 90);
  createAndRegisterTrans(outerShieldTransName, 0, 0, zPosOuterShield);

  // Inner shield
  float dzInnerShield = sDzAlu - sDzAluCone - dzShieldGap;                              // depth of the inner shield
  float zPosInnerShield = sZAluBack - sDzAluBack / 2 + dzShieldGap + dzInnerShield / 2; // z-position of the inner shield relative to the backplate
  
  std::string innerShieldName = "FV0_InnerShield";
  std::string innerShieldCutName = innerShieldName + "Cut";
  
  new TGeoTubeSeg(innerShieldName.c_str(), sDrMinAluInnerShield, sDrMaxAluInnerShield, dzInnerShield / 2, -90, 90);
  new TGeoBBox(innerShieldCutName.c_str(), fabs(sXAluHole), sDrMaxAluInnerShield, dzInnerShield / 2);

  // Inner shield composite shape
  std::string innerShieldCSBoolFormula;
  innerShieldCSBoolFormula = innerShieldName;
  innerShieldCSBoolFormula += "-" + innerShieldCutName;

  std::string innerShieldCSName = innerShieldName + "CS";
  std::string innerShieldCSTransName = innerShieldCSName + "Trans";
  new TGeoCompositeShape(innerShieldCSName.c_str(), innerShieldCSBoolFormula.c_str());
  createAndRegisterTrans(innerShieldCSTransName, sXAluHole, 0, zPosInnerShield);

  // Cover
  float dzCover = sDzAlu;                             // Depth of the covers
  float zPosCoverConeCut = zPosFrontPlate + zPosCone; // Set the cone cut relative to the frontplate so that the exact position of the aluminium cone part can be used.
  
  std::string coverName = "FV0_Cover";
  std::string coverConeCutName = coverName + "ConeCut";
  std::string coverHoleCutName = coverName + "HoleCut";
 
  new TGeoBBox(coverName.c_str(), sDxAluCover / 2, sDrMaxAluOuterShield, dzCover / 2);
  new TGeoConeSeg(coverConeCutName.c_str(),
                  sDzAluCone / 2,
                  0,
                  sDrMinAluCone + thicknessFrontPlateCone,
                  0,
                  sDrMinAluFront,
                  -90,
                  90);
  new TGeoTubeSeg(coverHoleCutName.c_str(), 0, sDrMinAluInnerShield, dzCover / 2, 0, 360);  

  std::string coverTransName = coverName + "Trans";
  std::string coverConeCutTransName = coverConeCutName + "Trans";
  std::string coverHoleCutTransName = coverHoleCutName + "Trans";

  createAndRegisterTrans(coverTransName, sDxAluCover / 2, 0, zPosOuterShield);
  createAndRegisterTrans(coverConeCutTransName, 0, 0, zPosCoverConeCut);
  createAndRegisterTrans(coverHoleCutTransName.c_str(), sXAluHole, 0, zPosOuterShield);

  // Cover composite shape
  std::string coverCSBoolFormula = "";
  coverCSBoolFormula += coverName + ":" + coverTransName;
  coverCSBoolFormula += "-" + coverConeCutName + ":" + coverConeCutTransName;
  coverCSBoolFormula += "-" + coverHoleCutName + ":" + coverHoleCutTransName;

  std::string coverCSName = coverName + "CS";
  new TGeoCompositeShape(coverCSName.c_str(), coverCSBoolFormula.c_str());

  // Composite shape
  std::string boolFormula = "";
  boolFormula += backPlateCSName + ":" + backPlateCSTransName;
  boolFormula += "+" + frontPlateCSName + ":" + frontPlateTransName;
  boolFormula += "+" + frontPlateConeCSName + ":" + frontPlateTransName;
  boolFormula += "+" + outerShieldName + ":" + outerShieldTransName;
  boolFormula += "+" + innerShieldCSName + ":" + innerShieldCSTransName;
  boolFormula += "+" + coverCSName;

  std::string aluContCSName = "FV0_AluContCS";
  TGeoCompositeShape* aluContCS = new TGeoCompositeShape(aluContCSName.c_str(), boolFormula.c_str());

  // Volume
  std::string aluContName = "FV0" + sContainerName;
  TGeoMedium* kMed = gGeoManager->GetMedium("V0_Aluminium$");
  new TGeoVolume(aluContName.c_str(), aluContCS, kMed);
}

void Geometry::initializeLuts()
{
  // TODO: initialize sth
}

void Geometry::buildGeometry()
{
  TGeoVolume* vALIC = gGeoManager->GetVolume("cave");
  if (!vALIC) {
    LOG(FATAL) << "Could not find the top volume";
  }

  // Top volume of FIT V0 detector
  TGeoVolumeAssembly* vFV0 = new TGeoVolumeAssembly("FV0");
  LOG(INFO) << "FV0 Geometry::buildGeometry()::Volume name = " << vFV0->GetName();

  assembleSensVols(vFV0);
  if (mGeometryType == eFull) {
    assembleNonSensVols(vFV0);
  }

  TGeoTranslation* globalShift = new TGeoTranslation(sXGlobal, sYGlobal, sZGlobal);

  vALIC->AddNode(vFV0, 1, globalShift);
}

void Geometry::assembleSensVols(TGeoVolumeAssembly* vFV0)
{
  assembleScintSectors(vFV0);
}

void Geometry::assembleNonSensVols(TGeoVolumeAssembly* vFV0)
{
  assemblePlasticSectors(vFV0);
  assembleFibers(vFV0);
  assembleMetalContainer(vFV0);
}

void Geometry::assembleScintSectors(TGeoVolumeAssembly* vFV0)
{
  TGeoVolumeAssembly* sectors = buildSectorAssembly(sScintName);
  vFV0->AddNode(sectors, 1);
}

void Geometry::assemblePlasticSectors(TGeoVolumeAssembly* vFV0)
{
  TGeoVolumeAssembly* sectors = buildSectorAssembly(sPlastName);
  vFV0->AddNode(sectors, 1, new TGeoTranslation(0, 0, sZPlast));
}

void Geometry::assembleFibers(TGeoVolumeAssembly* vFV0)
{
  TGeoVolumeAssembly* fibers = new TGeoVolumeAssembly("FV0FIBERS");

  for (int i = 0; i < mRMinFiber.size(); i++) {
    std::stringstream ssFiberName;
    ssFiberName << "FV0" << sFiberName << i + 1;
    TGeoVolume* fiber = gGeoManager->GetVolume(ssFiberName.str().c_str());
    fibers->AddNode(fiber, 1);
  }

  TGeoRotation* reflection = new TGeoRotation();
  reflection->ReflectX(true);

  vFV0->AddNode(fibers, 1);
  vFV0->AddNode(fibers, 2, reflection);
}

void Geometry::assembleMetalContainer(TGeoVolumeAssembly* volV0)
{
  std::string containerName = "FV0" + sContainerName;
  TGeoVolume* container = gGeoManager->GetVolume(containerName.c_str());
  if (!container) {
    LOG(WARNING) << "FV0: Couldn't find volume " << containerName;
  } else {
    LOG(DEBUG) << "FVO Geometry::assembleMetalContainer(): adding container volume " << containerName;
    TGeoRotation* reflection = new TGeoRotation();
    reflection->ReflectX(true);
    volV0->AddNode(container, 1);
    volV0->AddNode(container, 2, reflection);
  }
}

TGeoVolumeAssembly* Geometry::buildSectorAssembly(std::string cellName)
{
  std::string assemblyName = "FV0" + cellName;
  TGeoVolumeAssembly* assembly = new TGeoVolumeAssembly(assemblyName.c_str());
  TGeoVolumeAssembly* left = new TGeoVolumeAssembly((assemblyName + "LEFT").c_str());
  TGeoVolumeAssembly* right = new TGeoVolumeAssembly((assemblyName + "RIGHT").c_str());

  for (int iSector = 0; iSector < ceil(mSectorTrans.size() / 2); iSector++) {
    TGeoVolumeAssembly* sector = buildSector(cellName, iSector);
    left->AddNode(sector, iSector + 1, mSectorTrans.at(iSector));
  }

  for (int iSector = ceil(mSectorTrans.size() / 2); iSector < mSectorTrans.size(); iSector++) {
    TGeoVolumeAssembly* sector = buildSector(cellName, iSector);
    right->AddNode(sector, iSector + 1, mSectorTrans.at(iSector));
  }

  assembly->AddNode(left, 1);
  assembly->AddNode(right, 1);
  
  return assembly;
}

TGeoVolumeAssembly* Geometry::buildSector(std::string cellType, int iSector)
{
  std::stringstream ssSectorName;
  ssSectorName << "FV0" << cellType << sSectorName << iSector + 1;

  LOG(DEBUG) << "FV0 Geometry::buildSector(): building sector " << ssSectorName.str();

  TGeoVolumeAssembly* sector = new TGeoVolumeAssembly(ssSectorName.str().c_str());
  
  for (int i = 0; i < sNumberOfRings; i++) {
    std::stringstream ssCellName;
    ssCellName << "FV0" << cellType << sCellName << sCellTypes[iSector] << i + 1;

    TGeoVolume* cell = gGeoManager->GetVolume(ssCellName.str().c_str());

    if (!cell) {
      LOG(WARNING) << "FV0: Couldn't find cell volume " << ssCellName.str();
    } else {
      LOG(DEBUG) << "FV0 Geometry::buildSector(): adding cell volume " << ssCellName.str();
      sector->AddNode(cell, 1);
    }
  }

  return sector;
}

TGeoTranslation* Geometry::createAndRegisterTrans(std::string name, double dx, double dy, double dz)
{
  TGeoTranslation* trans = new TGeoTranslation(name.c_str(), dx, dy, dz);
  trans->RegisterYourself();
  return trans;
}

TGeoRotation* Geometry::createAndRegisterRot(std::string name, double phi, double theta, double psi)
{
  TGeoRotation* rot = new TGeoRotation(name.c_str(), phi, theta, psi);
  rot->RegisterYourself();
  return rot;
}