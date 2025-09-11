#include "SemiconductorDevice.h"
#include "GeometryBuilder.h"
#include "BoundaryMesh.h"

#include <TopoDS.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <TopExp_Explorer.hxx>
#include <Standard_Failure.hxx>
#include <iostream>
#include <vector>

static TopoDS_Solid firstSolidOrThrow(const TopoDS_Shape& shape, const char* ctx) {
    if (shape.IsNull()) {
        throw std::runtime_error(std::string("Null shape in ") + ctx);
    }
    if (shape.ShapeType() == TopAbs_SOLID) {
        return TopoDS::Solid(shape);
    }
    TopExp_Explorer exp(shape, TopAbs_SOLID);
    if (exp.More()) {
        return TopoDS::Solid(exp.Current());
    }
    throw std::runtime_error(std::string("No SOLID found after ") + ctx);
}

int main() {
    try {
        std::cout << "=== FinFET Extrusion Example ===" << std::endl;

        // Device dimensions (meters)
        const double Lx = 20e-6;          // device length (X: source->drain)
        const double Ly = 10e-6;          // device width (Y: along fin)
        const double substrateH = 2e-6;   // substrate thickness
        const double oxideH = 0.20e-6;    // blanket oxide thickness (>=200 nm for robustness)

        // Gate & fin parameters
        const double gateLength = 4e-6;   // gate dimension along X
        const double gateHeight = 0.30e-6;// gate vertical height
        const double gateCenterX = Lx * 0.5;  // centered gate
        const double gateX0 = gateCenterX - 0.5 * gateLength;
        const double gateX1 = gateCenterX + 0.5 * gateLength;

        const double finWidthX = 0.30e-6;     // fin width along X
        const double finLengthY = Ly * 0.6;   // fin length along Y
        const double finHeight = 0.40e-6;     // fin protrusion above oxide

        // Gate dielectric around fin (sidewall oxide) thickness
        const double toxSide = 0.12e-6;   // 120 nm around fin sidewalls (avoid OCC precision issues)

        // Source/Drain contact bar
        const double sdBarThick = 0.30e-6;     // vertical thickness above oxide

        // Create device container
        SemiconductorDevice device("FinFET_Extrusion");
        device.setCharacteristicLength(1.0e-6);

        // Materials
        auto matSi   = SemiconductorDevice::createStandardSilicon();
        auto matOx   = SemiconductorDevice::createStandardSiliconDioxide();
        auto matPoly = SemiconductorDevice::createStandardPolysilicon();
        auto matMetal= SemiconductorDevice::createStandardMetal();

        // 1) Substrate (silicon)
        TopoDS_Solid substrate = GeometryBuilder::createBox(
            gp_Pnt(0, 0, 0), Dimensions3D(Lx, Ly, substrateH)
        );
        device.addLayer(std::make_unique<DeviceLayer>(substrate, matSi, DeviceRegion::Substrate, "Substrate"));

        // 2) Blanket oxide on substrate
        std::cout << "[stage] Creating blanket oxide..." << std::endl;
        TopoDS_Solid blanketOx = GeometryBuilder::createBox(
            gp_Pnt(0, 0, substrateH), Dimensions3D(Lx, Ly, oxideH)
        );
        std::cout << "[ok] Blanket oxide created" << std::endl;

        // 3) Fin extrusion (silicon) rising through oxide and protruding above it
        const double finX0 = gateCenterX - 0.5 * finWidthX;
        const double finX1 = gateCenterX + 0.5 * finWidthX;
        const double finY0 = (Ly - finLengthY) * 0.5;
        const double finY1 = finY0 + finLengthY;
        TopoDS_Solid fin = GeometryBuilder::createBox(
            gp_Pnt(finX0, finY0, substrateH),
            Dimensions3D(finX1 - finX0, finY1 - finY0, oxideH + finHeight)
        );
        if (!GeometryBuilder::isValidShape(fin)) {
            throw std::runtime_error("Fin geometry invalid");
        }
        device.addLayer(std::make_unique<DeviceLayer>(fin, matSi, DeviceRegion::ActiveRegion, "Fin"));

        // Carve the fin slot out of the blanket oxide (use Z-extended cutter to avoid coplanar faces)
        std::cout << "[stage] Cutting fin slot in oxide..." << std::endl;
        const double cutEpsZ = 2e-9; // 2 nm epsilon to avoid tangential/coincident faces
        TopoDS_Solid finCutZ = GeometryBuilder::createBox(
            gp_Pnt(finX0, finY0, substrateH - cutEpsZ),
            Dimensions3D(finX1 - finX0, finY1 - finY0, oxideH + finHeight + 2.0 * cutEpsZ)
        );
        TopoDS_Shape oxWithSlotShape = GeometryBuilder::subtractShapes(blanketOx, finCutZ);
        TopoDS_Solid oxWithSlot = firstSolidOrThrow(oxWithSlotShape, "oxide - finCutZ");
        if (!GeometryBuilder::isValidShape(oxWithSlot)) {
            std::cout << "[warn] Oxide-with-slot initially invalid; attempting repair..." << std::endl;
            TopoDS_Shape repaired = GeometryBuilder::repairShape(oxWithSlot);
            TopoDS_Solid repairedSolid = firstSolidOrThrow(repaired, "repair oxide-with-slot");
            if (!GeometryBuilder::isValidShape(repairedSolid)) {
                throw std::runtime_error("Oxide-with-slot invalid");
            }
            oxWithSlot = repairedSolid;
        }
        device.addLayer(std::make_unique<DeviceLayer>(oxWithSlot, matOx, DeviceRegion::Insulator, "Oxide_Blanket"));
        std::cout << "[ok] Fin slot cut in oxide" << std::endl;

// 4) Sidewall oxide sleeve around fin within gate window
        const double sleeveX0 = finX0 - toxSide;
        const double sleeveX1 = finX1 + toxSide;
        const double sleeveY0 = std::max(0.0, finY0 - toxSide);
        const double sleeveY1 = std::min(Ly,   finY1 + toxSide);
        TopoDS_Solid sleeveOuter;
        bool sleeveBuilt = false;
        try {
            std::cout << "[debug] Sleeve extents X0=" << sleeveX0 << ", X1=" << sleeveX1
                      << ", Y0=" << sleeveY0 << ", Y1=" << sleeveY1 << std::endl;
            std::cout << "[debug] Creating sleeveOuter dims Lx=" << (sleeveX1 - sleeveX0)
                      << ", Ly=" << (sleeveY1 - sleeveY0) << ", Lz=" << (oxideH + finHeight) << std::endl;
            sleeveOuter = GeometryBuilder::createBox(
                gp_Pnt(sleeveX0, sleeveY0, substrateH),
                Dimensions3D((sleeveX1 - sleeveX0), (sleeveY1 - sleeveY0), (oxideH + finHeight))
            );
            std::cout << "[debug] sleeveOuter created" << std::endl;
            if (!GeometryBuilder::isValidShape(sleeveOuter)) {
                throw std::runtime_error("Sleeve outer invalid");
            }
            // Build sleeve as a hollow rectangular ring: sleeveOuter minus inner core (fin envelope)
            std::cout << "[stage] Creating gate sleeve (oxide around fin)..." << std::endl;
            const double z0 = substrateH;
            const double zH = oxideH + finHeight;
            const double eps = 2e-9; // small epsilon to avoid coplanar faces
            TopoDS_Solid innerCore = GeometryBuilder::createBox(
                gp_Pnt(finX0 + eps, finY0 + eps, z0 + eps),
                Dimensions3D((finX1 - finX0) - 2.0 * eps, (finY1 - finY0) - 2.0 * eps, zH - 2.0 * eps)
            );
            std::cout << "[debug] Subtracting inner core (fin envelope) to form sleeve ring..." << std::endl;
            TopoDS_Shape sleeveRingShape = GeometryBuilder::subtractShapes(sleeveOuter, innerCore);
            TopoDS_Solid sleeve = firstSolidOrThrow(sleeveRingShape, "sleeve ring (outer - innerCore)");
            if (!GeometryBuilder::isValidShape(sleeve)) {
                std::cout << "[warn] Sleeve initially invalid; attempting repair..." << std::endl;
                TopoDS_Shape repaired = GeometryBuilder::repairShape(sleeve);
                TopoDS_Solid repairedSolid = firstSolidOrThrow(repaired, "repair sleeve");
                if (!GeometryBuilder::isValidShape(repairedSolid)) {
                    throw std::runtime_error("Sleeve invalid");
                }
                sleeve = repairedSolid;
            }
            device.addLayer(std::make_unique<DeviceLayer>(sleeve, matOx, DeviceRegion::Insulator, "Gate_Sleeve_Oxide"));
            std::cout << "[ok] Gate sleeve created" << std::endl;
            sleeveBuilt = true;
        } catch (const Standard_Failure& e) {
            const char* msg = e.GetMessageString();
            std::cout << "[warn] OpenCASCADE failure during gate sleeve creation: " << (msg ? msg : "<no message>") << std::endl;
            std::cout << "[warn] Proceeding without explicit sleeve layer; gate will be hollowed using sleeveOuter." << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[warn] Gate sleeve construction failed: " << e.what() << std::endl;
            std::cout << "[warn] Proceeding without explicit sleeve layer; gate will be hollowed using sleeveOuter." << std::endl;
        }

        // 5) Gate that wraps around the sleeve (subtract sleeve outer to create a wrap)
        const double gateYMargin = 0.6e-6;
        const double gateY0 = std::max(0.0, sleeveY0 - gateYMargin);
        const double gateY1 = std::min(Ly, sleeveY1 + gateYMargin);
        TopoDS_Solid gateBox = GeometryBuilder::createBox(
            gp_Pnt(gateX0, gateY0, substrateH),
            Dimensions3D(gateX1 - gateX0, gateY1 - gateY0, oxideH + gateHeight)
        );
        if (!GeometryBuilder::isValidShape(gateBox)) {
            throw std::runtime_error("Gate box invalid");
        }
        // Hollow the gate so it wraps around the oxide sleeve
        std::cout << "[stage] Hollowing gate to wrap around sleeve..." << std::endl;
        TopoDS_Shape gateHollowShape = GeometryBuilder::subtractShapes(gateBox, sleeveOuter);
        TopoDS_Solid gateHollow = firstSolidOrThrow(gateHollowShape, "gateBox - sleeveOuter");
        if (!GeometryBuilder::isValidShape(gateHollow)) {
            throw std::runtime_error("Gate hollow invalid");
        }
        device.addLayer(std::make_unique<DeviceLayer>(gateHollow, matPoly, DeviceRegion::Gate, "Gate"));
        std::cout << "[ok] Gate created" << std::endl;

        // 6) Source-Drain bar above oxide spanning full device; split with gate footprint
        TopoDS_Solid sdBar = GeometryBuilder::createBox(
            gp_Pnt(0, 0, substrateH + oxideH), Dimensions3D(Lx, Ly, sdBarThick)
        );
        // Cutting prism equals the gate footprint across full Y
        TopoDS_Solid sdCut = GeometryBuilder::createBox(
            gp_Pnt(gateX0, 0, substrateH + oxideH), Dimensions3D(gateX1 - gateX0, Ly, sdBarThick + 0.02e-6)
        );
        TopoDS_Shape sdSplitShape = GeometryBuilder::subtractShapes(sdBar, sdCut); // demo only
        (void)sdSplitShape; // keep the demo cut but do not depend on its topology
        // Build explicit rectangular pads for Source and Drain
        const double sdZ = substrateH + oxideH;
        const double srcLen = std::max(0.0, gateX0 - 0.0);
        const double drnLen = std::max(0.0, Lx - gateX1);
        TopoDS_Solid sourcePad = GeometryBuilder::createBox(
            gp_Pnt(0.0, 0.0, sdZ), Dimensions3D(srcLen, Ly, sdBarThick)
        );
        TopoDS_Solid drainPad = GeometryBuilder::createBox(
            gp_Pnt(gateX1, 0.0, sdZ), Dimensions3D(drnLen, Ly, sdBarThick)
        );
        device.addLayer(std::make_unique<DeviceLayer>(sourcePad, matMetal, DeviceRegion::Source, "Source_Region"));
        device.addLayer(std::make_unique<DeviceLayer>(drainPad,  matMetal, DeviceRegion::Drain,  "Drain_Region"));

        // Build, mesh, export
        device.buildDeviceGeometry();
        device.printDeviceInfo();

        // Per-layer meshes for region export
        if (auto l = device.getLayer("Substrate"))          l->generateBoundaryMesh(0.6e-6);
        if (auto l = device.getLayer("Oxide_Blanket"))      l->generateBoundaryMesh(0.2e-6);
        if (auto l = device.getLayer("Fin"))                l->generateBoundaryMesh(0.15e-6);
        if (auto l = device.getLayer("Gate_Sleeve_Oxide"))  l->generateBoundaryMesh(0.12e-6);
        if (auto l = device.getLayer("Gate"))               l->generateBoundaryMesh(0.2e-6);
        if (auto l = device.getLayer("Source_Region"))      l->generateBoundaryMesh(0.25e-6);
        if (auto l = device.getLayer("Drain_Region"))       l->generateBoundaryMesh(0.25e-6);

        // Optional global mesh
        device.generateGlobalBoundaryMesh(0.25e-6);

        // Exports
        device.exportGeometry("finfet_extrusion.step", "STEP");
        device.exportMeshWithRegions("finfet_extrusion_with_regions.vtk", "VTK");
        device.exportMesh("finfet_extrusion_global.vtk", "VTK");

        std::cout << "\nGenerated files:" << std::endl;
        std::cout << "  • finfet_extrusion.step" << std::endl;
        std::cout << "  • finfet_extrusion_with_regions.vtk" << std::endl;
        std::cout << "  • finfet_extrusion_global.vtk" << std::endl;
        std::cout << "\n=== FinFET Extrusion Example completed ===" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Error: Unknown OpenCASCADE exception" << std::endl;
        return 1;
    }
}

