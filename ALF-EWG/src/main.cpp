// ============================================================================
//  Autonomous Landslide & Flash-Flood Early Warning Grid (ALF-EWG)
//  CIC-211  Object-Oriented Programming Using C++  --  Project Assignment
//  Build:  g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp -o alf_ewg
// ============================================================================
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "DisasterCore/Alerts.hpp"
#include "DisasterCore/Exceptions.hpp"
#include "DisasterCore/Nodes.hpp"
#include "DisasterCore/SensorStation.hpp"

using namespace DisasterCore;
using namespace DisasterCore::Sensors;
using namespace DisasterCore::Alerts;

namespace {

void banner(const std::string& title) {
    std::cout << "\n==============================================================\n"
              << "  " << title
              << "\n==============================================================\n";
}

// ---------------------------------------------------------------------------
// UNIT I
// ---------------------------------------------------------------------------
void demoUnitI() {
    banner("UNIT I : namespaces, inline, overloading, references, friend");

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "computeHazardIndex(Rainfall 95, Slope 48)          = "
              << computeHazardIndex(Rainfall{95.0}, Slope{48.0}) << '\n';
    std::cout << "computeHazardIndex(Saturation 91, Vibration 14)    = "
              << computeHazardIndex(Saturation{91.0}, Vibration{14.0}) << '\n';
    std::cout << "computeHazardIndex(sat 70, vib 9, tilt 4)          = "
              << computeHazardIndex(70.0, 9.0, 4.0) << '\n';
    std::cout << "computeHazardIndex(rain 60, slope 35, sat 78, vib 6) = "
              << computeHazardIndex(60.0, 35.0, 78.0, 6.0) << '\n';
    std::cout << std::defaultfloat << std::setprecision(6);

    GeoCoord c{30.65, 79.07};
    shiftCoordinate(c, 0.002, -0.001);                       // call by reference
    std::cout << "Coordinate after in-place shift: (" << c.lat << ", " << c.lon << ")\n";

    double total = 0.0;
    Sensors::accumulate(Sensors::accumulate(total, 12.5), 7.5) += 1.0;   // return by reference
    std::cout << "Accumulated telemetry (chained, return by reference): " << total << '\n';

    SensorStation demo(900, "Audit-Demo", "Test", GeoCoord{30.0, 79.0}, 50, 30, 90, 8);
    demo.recordSample(41.5);
    demo.recordSample(47.0);
    TelemetryAuditor{}.audit(demo);                          // friend class, no getters
}

// ---------------------------------------------------------------------------
// UNIT II
// ---------------------------------------------------------------------------
std::vector<SensorStation> demoUnitII() {
    banner("UNIT II : abstraction, static members, dynamic memory, operators");

    std::vector<SensorStation> stations;
    stations.reserve(6);
    stations.emplace_back(101, "Gaurikund-Ridge", "Rudraprayag", GeoCoord{30.65, 79.07}, 95, 48, 91, 14);
    stations.emplace_back(102, "Sonprayag-Bend",  "Rudraprayag", GeoCoord{30.62, 78.99}, 60, 35, 78, 6);
    stations.emplace_back(103, "Joshimath-North", "Chamoli",     GeoCoord{30.56, 79.56}, 70, 52, 84, 11);
    stations.emplace_back(104, "Tapovan-Gauge",   "Chamoli",     GeoCoord{30.49, 79.62}, 25, 28, 52, 3);
    stations.emplace_back(105, "Harsil-Slope",    "Uttarkashi",  GeoCoord{31.03, 78.75}, 15, 20, 40, 1.5);
    stations.emplace_back(106, "Gangotri-Pass",   "Uttarkashi",  GeoCoord{30.99, 78.94}, 40, 44, 66, 5);

    for (auto& s : stations) s.recordSample(s.hazardIndex());

    std::cout << "Active stations (static counter): " << SensorStation::activeCount() << '\n';
    for (const auto& s : stations) std::cout << s << '\n';          // operator<< (console)

    // --- deep copy of the dynamic buffer ------------------------------------
    std::cout << "\n-- TelemetryBuffer deep copy --\n";
    TelemetryBuffer a(4);
    a.push(1.5); a.push(2.5); a.push(3.5);
    TelemetryBuffer b = a;                       // deep copy constructor
    b.at(0) = 99.0;                              // return by reference modifies b only
    const double raw[] = {7.0, 8.0, 9.0};
    TelemetryBuffer c(raw, 3);                   // dynamic constructor from array
    std::cout << "original a = " << a << "\ncopy     b = " << b << "\nfrom raw c = " << c << '\n';

    // --- operator + : aggregate into regional zone vectors -------------------
    std::cout << "\n-- operator+ : regional zone vectors --\n";
    ZoneVector pair = stations[0] + stations[1];
    pair = pair + stations[2];
    std::cout << pair << '\n';
    std::map<std::string, ZoneVector> zones;
    for (const auto& s : stations) zones[s.zone()] = zones[s.zone()] + s;
    for (const auto& [name, z] : zones) std::cout << z << '\n';

    // --- operator > ---------------------------------------------------------
    std::cout << "\n-- operator> --\n";
    std::cout << stations[0].name() << (stations[0] > stations[3] ? " outranks " : " does not outrank ")
              << stations[3].name() << '\n';

    // --- operator<< / >> on file streams -------------------------------------
    {
        std::ofstream out("stations.txt");
        for (const auto& s : stations) out << s;
    }
    std::vector<SensorStation> loaded;
    {
        std::ifstream in("stations.txt");
        SensorStation tmp;
        while (in >> tmp) loaded.push_back(tmp);             // operator>>
    }
    std::cout << "\nRestored " << loaded.size() << " stations from stations.txt (operator>>):\n";
    for (const auto& s : loaded) std::cout << "  " << s << '\n';

    return stations;
}

// ---------------------------------------------------------------------------
// UNIT III + IV : polymorphic sweep, diamond, exceptions, persistence
// ---------------------------------------------------------------------------
void sweepGrid(IncidentLog& log) {
    banner("UNIT III : diamond problem, containership, runtime polymorphism");

    std::vector<BaseSensorNode*> grid;
    grid.push_back(new SoilMoistureSensor(201, "Soil-Gaurikund"));
    grid.push_back(new SeismicGeophone(202, "Geophone-Joshimath"));
    grid.push_back(new AcousticTiltmeter(203, "Tilt-Sonprayag"));
    grid.push_back(new RiverSonarGauge(204, "Sonar-Mandakini"));
    auto* probe = new AutonomousHybridProbe(205, "Hybrid-Kedar");
    grid.push_back(probe);

    // Diamond: one shared DisasterTelemetryDevice sub-object
    const DisasterTelemetryDevice* viaSolar = static_cast<const SolarPoweredNode*>(probe);
    const DisasterTelemetryDevice* viaLora  = static_cast<const WirelessLoRaTransceiver*>(probe);
    const DisasterTelemetryDevice* viaBase  = static_cast<const BaseSensorNode*>(probe);
    std::cout << std::boolalpha
              << "Single shared virtual base via Solar/LoRa/Base paths: "
              << (viaSolar == viaLora && viaLora == viaBase) << '\n'
              << "probe->deviceId() (unambiguous): " << probe->deviceId()
              << ", firmware " << probe->firmware() << '\n' << std::noboolalpha;

    banner("UNIT III/IV : sensor sweep via BaseSensorNode* (dynamic dispatch + exceptions)");
    SensorStation::resetGlobalAlert();
    std::map<int, bool> logged;

    for (int cycle = 1; cycle <= 6; ++cycle) {
        std::cout << "\n-- sweep cycle " << cycle << " --\n";
        for (BaseSensorNode* node : grid) {
            std::cout << "  " << col(20) << node->typeName() << col(20) << node->name();
            try {
                const double v = node->readTelemetry();               // virtual call via vptr
                const bool alarm = node->evaluateThreshold();         // virtual call via vptr
                std::cout << col(10) << std::fixed << std::setprecision(1) << v
                          << (alarm ? "ALARM" : "ok") << std::defaultfloat << std::setprecision(6) << '\n';
                if (alarm && !logged[node->deviceId()]) {
                    logged[node->deviceId()] = true;
                    SensorStation::raiseGlobalAlert(static_cast<int>(AlertLevel::Warning));
                    log.append(makeRecord(node->deviceId(), static_cast<int>(AlertLevel::Warning),
                                          v, cycle, "Himalayan-Grid",
                                          node->typeName() + " threshold exceeded"));
                }
            } catch (const SensorFailureException& e) {
                std::cout << "\n      ! " << e.what() << '\n';
            } catch (const TelemetryChecksumMismatch& e) {
                std::cout << "\n      ! " << e.what() << '\n';
            } catch (const ThresholdOverflowException& e) {
                std::cout << "\n      ! " << e.what() << '\n';
            } catch (const std::exception& e) {
                std::cout << "\n      ! unexpected: " << e.what() << '\n';
            }
        }
    }

    std::cout << "\nGlobal emergency alert level (static): " << SensorStation::globalAlertLevel() << '\n';
    for (BaseSensorNode* n : grid)
        if (auto* h = dynamic_cast<AutonomousHybridProbe*>(n))
            std::cout << h->transmit(h->lastReading()) << "  (battery "
                      << h->power().level() << "%)\n";

    for (BaseSensorNode* n : grid) delete n;                 // virtual destructors
    std::cout << "All " << grid.size() << " nodes released through base pointers.\n";
}

// ---------------------------------------------------------------------------
// UNIT IV : STL, functors, manipulators, binary persistence
// ---------------------------------------------------------------------------
void demoUnitIV(const std::vector<SensorStation>& stations, IncidentLog& log) {
    banner("UNIT IV : STL, functors, manipulators, binary log with rollback");

    std::vector<SensorStation> ranked = stations;
    std::sort(ranked.begin(), ranked.end(),
              [](const SensorStation& x, const SensorStation& y) { return x > y; });
    std::cout << "Stations ranked by hazard (operator> + std::sort):\n";
    for (const auto& s : ranked) std::cout << "  " << s << '\n';

    // evacuation routing with a custom functor
    const int    pop[]  = {1200, 540, 2300, 310, 150, 480};
    const double dist[] = {4.5, 7.2, 3.1, 12.0, 15.5, 9.8};
    std::vector<EvacuationRoute> routes;
    for (std::size_t i = 0; i < stations.size(); ++i)
        routes.push_back({stations[i].name(), stations[i].hazardIndex(), pop[i], dist[i]});
    std::sort(routes.begin(), routes.end(), EvacuationPriority{});

    std::cout << "\nPriority evacuation dispatch:\n" << alertHeader;
    DispatchPrinter printer = std::for_each(routes.begin(), routes.end(), DispatchPrinter{});
    std::cout << tableRule << printer.printed << " routes dispatched\n";

    // sirens (static counter)
    std::map<std::string, EarlyWarningSiren> sirens;
    int nextId = 1;
    for (const auto& s : stations) {
        auto it = sirens.try_emplace(s.zone(), nextId, s.zone());
        if (it.second) ++nextId;
        const AlertLevel lvl = levelFromHazard(s.hazardIndex());
        if (static_cast<int>(lvl) > static_cast<int>(it.first->second.level()))
            it.first->second.set(lvl);
    }
    std::cout << "\nRegional sirens:\n";
    for (const auto& [zone, siren] : sirens)
        std::cout << "  " << col(14) << zone << toString(siren.level())
                  << (siren.isActive() ? "  [SIREN ON]" : "") << '\n';
    std::cout << "Sirens sounding network-wide (static): " << EarlyWarningSiren::activeSirens() << '\n';

    // transactional binary logging
    std::cout << "\n-- binary incident log --\n";
    std::cout << "Records on disk before test: " << log.count() << '\n';
    try {
        log.append(makeRecord(999, 3, 99.0, 7, "Test-Zone", "this write will be rolled back"), true);
    } catch (const std::exception& e) {
        std::cout << "Transaction failed: " << e.what() << '\n';
    }
    std::cout << "Records on disk after rollback: " << log.count() << " (unchanged)\n";

    log.append(makeRecord(301, 2, 61.4, 7, "Rudraprayag", "manual field report"));
    std::cout << "Committed one more record -> " << log.count() << " on disk\n\n";

    std::cout << "Persisted incidents:\n" << tableRule
              << col(9) << "STATION" << col(10) << "LEVEL" << col(8) << "CYCLE"
              << col(9) << "HAZARD" << "MESSAGE\n" << tableRule;
    for (const auto& r : log.readAll())
        std::cout << col(9) << r.stationId << col(10) << toString(static_cast<AlertLevel>(r.level))
                  << col(8) << r.cycle << col(9) << std::fixed << std::setprecision(1) << r.hazard
                  << std::defaultfloat << std::setprecision(6) << r.message << '\n';
}

} // namespace

int main() {
    std::cout << "ALF-EWG : Autonomous Landslide & Flash-Flood Early Warning Grid\n";
    std::filesystem::remove("incident_log.bin");             // fresh log for each demo run
    try {
        IncidentLog log("incident_log.bin");
        demoUnitI();
        std::vector<SensorStation> stations = demoUnitII();
        sweepGrid(log);
        demoUnitIV(stations, log);
        std::cout << "\nStations alive at end of run (static counter): "
                  << SensorStation::activeCount() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
