# Autonomous Landslide & Flash-Flood Early Warning Grid (ALF-EWG)

**Course:** Object-Oriented Programming Using C++ (CIC-211) – Project Assignment, Option 1
**Institute:** Maharaja Agrasen Institute of Technology, Dept. of CSE
**Author:** Riya Garg | Enrolment No. 21314802724 | Class 3C2
**Standard:** C++17, no external libraries

## Build & run
```bash
make          # g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude src/main.cpp -o alf_ewg
make clean
```
Compiles with zero warnings and runs clean under AddressSanitizer + UBSan.

## Project layout
```
include/DisasterCore/
  Exceptions.hpp      custom std::exception classes
  Hazard.hpp          namespace, inline fns, overloaded computeHazardIndex, reference semantics
  TelemetryBuffer.hpp dynamic buffer: parameterized/dynamic/deep-copy ctors, destructor
  SensorStation.hpp   SensorStation, ZoneVector, TelemetryAuditor (friend), operators
  Nodes.hpp           BaseSensorNode, diamond hierarchy, PowerSubsystem (containership)
  Alerts.hpp          sirens, functors, manipulators, binary IncidentLog with rollback
src/main.cpp          demo driver, one section per unit
```

## Requirement → where it is implemented
| Unit | Requirement | Location |
|---|---|---|
| I | Namespaces `DisasterCore::Sensors` / `::Alerts` | all headers |
| I | `inline` risk-index functions | `Hazard.hpp` |
| I | Overloaded `computeHazardIndex()` (Rainfall+Slope, Saturation+Vibration, 3-arg, 4-arg) | `Hazard.hpp` |
| I | Call by reference / return by reference | `shiftCoordinate()`, `accumulate()`, `TelemetryBuffer::at()` |
| I | Friend class | `TelemetryAuditor` in `SensorStation.hpp` |
| II | Data abstraction + const correctness | `SensorStation`, `EarlyWarningSiren`, `TelemetryBuffer` |
| II | Static members (station count, global alert level, active sirens) | `SensorStation`, `EarlyWarningSiren` |
| II | Parameterized, dynamic, deep-copy ctors + destructor | `TelemetryBuffer.hpp` |
| II | `operator+`, `operator>`, `operator<<`, `operator>>` | `SensorStation.hpp` |
| III | Abstract `BaseSensorNode` (pure virtual `readTelemetry`, `evaluateThreshold`) | `Nodes.hpp` |
| III | Diamond via virtual bases (`DisasterTelemetryDevice` → `SolarPoweredNode` / `WirelessLoRaTransceiver` → `AutonomousHybridProbe`) | `Nodes.hpp` |
| III | Containership: `PowerSubsystem` inside `BaseSensorNode` | `Nodes.hpp` |
| III | `std::vector<BaseSensorNode*>` dynamic dispatch in sweep cycles | `sweepGrid()` in `main.cpp` |
| IV | `SensorFailureException`, `TelemetryChecksumMismatch`, `ThresholdOverflowException` with `what()` | `Exceptions.hpp` |
| IV | Binary `std::fstream` log with rollback using `seekp/seekg/tellp` | `IncidentLog` in `Alerts.hpp` |
| IV | `std::vector`, `std::map`, `std::sort`, `std::for_each` | `main.cpp` |
| IV | Functors (`EvacuationPriority`, `DispatchPrinter`) | `Alerts.hpp` |
| IV | Custom manipulators (`tableRule`, `alertHeader`, `col(n)`) | `Alerts.hpp` |

## How the demo shows the tricky parts
* **Diamond:** prints `true` – the Solar, LoRa and Base paths all reach one shared `DisasterTelemetryDevice`.
* **Polymorphism:** one loop over `BaseSensorNode*` calls five different `readTelemetry()` implementations through the vptr.
* **Exceptions:** the sweep deliberately triggers a checksum mismatch (cycle 3), battery failure (cycles 5–6) and range overflow (cycle 6).
* **Rollback:** a simulated crash mid-write leaves the log at 5 records; the next valid write commits record 6.
* **Deep copy:** changing the copy of a `TelemetryBuffer` does not change the original.

The demo deletes `incident_log.bin` at start so output is reproducible; remove that line in `main()` to keep logs across runs.
