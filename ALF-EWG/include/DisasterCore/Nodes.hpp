// Nodes.hpp  --  UNIT III : abstract class, virtual base classes (diamond),
//                containership, runtime polymorphism.   (+ UNIT IV exceptions)
#pragma once
#include <algorithm>
#include <sstream>
#include <string>

#include "Exceptions.hpp"
#include "Hazard.hpp"

namespace DisasterCore::Sensors {

// ---------------------------------------------------------------------------
// PowerSubsystem : independent class, CONTAINED (has-a) inside every node
// ---------------------------------------------------------------------------
class PowerSubsystem {
    double batteryPct_;
public:
    explicit PowerSubsystem(double battery = 100.0) : batteryPct_(battery) {}
    double level() const          { return batteryPct_; }
    void   drain(double pct)      { batteryPct_ = std::max(0.0, batteryPct_ - pct); }
    void   charge(double pct)     { batteryPct_ = std::min(100.0, batteryPct_ + pct); }
    bool   isLow() const          { return batteryPct_ < 15.0; }
};

// ---------------------------------------------------------------------------
// Packet + checksum (used to simulate corrupted LoRa frames)
// ---------------------------------------------------------------------------
struct Packet { double value; unsigned crc; };

inline unsigned checksum(double v) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(&v);
    unsigned s = 0;
    for (std::size_t i = 0; i < sizeof v; ++i) s = (s * 31u + p[i]) & 0xFFu;
    return s;
}
inline void verifyPacket(int nodeId, const Packet& p) {
    const unsigned expected = checksum(p.value);
    if (expected != p.crc) throw TelemetryChecksumMismatch(nodeId, expected, p.crc);
}

// ---------------------------------------------------------------------------
// Diamond:      DisasterTelemetryDevice            (root, virtual base)
//                 /                  \ (virtual)
//        SolarPoweredNode     WirelessLoRaTransceiver
//                 \                  /
//                AutonomousHybridProbe
// ---------------------------------------------------------------------------
class DisasterTelemetryDevice {
protected:
    int         deviceId_;
    std::string firmware_;
public:
    explicit DisasterTelemetryDevice(int id = 0, std::string fw = "fw-2.4.1")
        : deviceId_(id), firmware_(std::move(fw)) {}
    virtual ~DisasterTelemetryDevice() = default;
    int                deviceId() const { return deviceId_; }
    const std::string& firmware() const { return firmware_; }
};

class SolarPoweredNode : public virtual DisasterTelemetryDevice {
protected:
    double panelWatts_;
public:
    explicit SolarPoweredNode(int id, double panelWatts = 20.0)
        : DisasterTelemetryDevice(id), panelWatts_(panelWatts) {}
    // battery percent harvested in `sunHours`
    double harvest(double sunHours) const { return panelWatts_ * sunHours * 0.1; }
};

class WirelessLoRaTransceiver : public virtual DisasterTelemetryDevice {
protected:
    int spreadingFactor_;
public:
    explicit WirelessLoRaTransceiver(int id, int sf = 9)
        : DisasterTelemetryDevice(id), spreadingFactor_(sf) {}
    std::string transmit(double payload) const {
        std::ostringstream os;
        os << "LoRa SF" << spreadingFactor_ << " uplink from device #" << deviceId_
           << " payload=" << payload;
        return os.str();
    }
};

// ---------------------------------------------------------------------------
// BaseSensorNode : abstract base class
// ---------------------------------------------------------------------------
class BaseSensorNode : public virtual DisasterTelemetryDevice {
protected:
    std::string    name_;
    PowerSubsystem power_;          // containership (aggregation) - NOT inheritance
    double         lastReading_ = 0.0;
    int            tick_        = 0;

    void ensurePowered() const {
        if (power_.level() < 5.0)
            throw SensorFailureException(deviceId_, "battery depleted (" +
                                         std::to_string(static_cast<int>(power_.level())) + "%)");
    }
public:
    BaseSensorNode(int id, std::string name, double battery)
        : DisasterTelemetryDevice(id), name_(std::move(name)), power_(battery) {}
    ~BaseSensorNode() override = default;               // virtual via root

    virtual double      readTelemetry()      = 0;       // pure virtual
    virtual bool        evaluateThreshold()  = 0;       // pure virtual
    virtual std::string typeName() const     = 0;

    const std::string&    name() const        { return name_; }
    const PowerSubsystem& power() const       { return power_; }
    double                lastReading() const { return lastReading_; }
};

// ---------------------------------------------------------------------------
// Concrete sensors (each overrides the pure virtuals)
// ---------------------------------------------------------------------------
class SoilMoistureSensor : public BaseSensorNode {
public:
    SoilMoistureSensor(int id, std::string name, double battery = 14.0)
        : DisasterTelemetryDevice(id), BaseSensorNode(id, std::move(name), battery) {}
    double readTelemetry() override {
        ensurePowered();
        power_.drain(3.0);
        ++tick_;
        const double v = std::min(100.0, 55.0 + 8.0 * tick_);          // % saturation
        verifyPacket(deviceId_, Packet{v, checksum(v)});
        return lastReading_ = v;
    }
    bool evaluateThreshold() override { return lastReading_ > 85.0; }
    std::string typeName() const override { return "SoilMoisture"; }
};

class SeismicGeophone : public BaseSensorNode {
public:
    SeismicGeophone(int id, std::string name)
        : DisasterTelemetryDevice(id), BaseSensorNode(id, std::move(name), 90.0) {}
    double readTelemetry() override {
        ensurePowered();
        power_.drain(1.0);
        ++tick_;
        const double v = 2.0 + 2.5 * tick_;                            // Hz
        Packet p{v, checksum(v)};
        if (tick_ == 3) p.crc ^= 0xFFu;                                // simulated line noise
        verifyPacket(deviceId_, p);
        return lastReading_ = v;
    }
    bool evaluateThreshold() override { return lastReading_ > 12.0; }
    std::string typeName() const override { return "SeismicGeophone"; }
};

class AcousticTiltmeter : public BaseSensorNode {
public:
    AcousticTiltmeter(int id, std::string name)
        : DisasterTelemetryDevice(id), BaseSensorNode(id, std::move(name), 80.0) {}
    double readTelemetry() override {
        ensurePowered();
        power_.drain(1.0);
        ++tick_;
        const double v = 1.0 + 1.2 * tick_;                            // degrees
        verifyPacket(deviceId_, Packet{v, checksum(v)});
        return lastReading_ = v;
    }
    bool evaluateThreshold() override { return lastReading_ > 5.0; }
    std::string typeName() const override { return "AcousticTiltmeter"; }
};

class RiverSonarGauge : public BaseSensorNode {
    static constexpr double kRangeLimit = 16.0;                        // metres
public:
    RiverSonarGauge(int id, std::string name)
        : DisasterTelemetryDevice(id), BaseSensorNode(id, std::move(name), 75.0) {}
    double readTelemetry() override {
        ensurePowered();
        power_.drain(1.0);
        ++tick_;
        const double v = 3.0 + 2.5 * tick_;                            // river level (m)
        if (v > kRangeLimit) throw ThresholdOverflowException(deviceId_, v, kRangeLimit);
        verifyPacket(deviceId_, Packet{v, checksum(v)});
        return lastReading_ = v;
    }
    bool evaluateThreshold() override { return lastReading_ > 10.0; }
    std::string typeName() const override { return "RiverSonarGauge"; }
};

// ---------------------------------------------------------------------------
// Leaf of the diamond : the most-derived class initialises the virtual root
// ---------------------------------------------------------------------------
class AutonomousHybridProbe : public BaseSensorNode,
                              public SolarPoweredNode,
                              public WirelessLoRaTransceiver {
public:
    AutonomousHybridProbe(int id, std::string name)
        : DisasterTelemetryDevice(id, "fw-3.0.0-hybrid"),
          BaseSensorNode(id, std::move(name), 60.0),
          SolarPoweredNode(id, 25.0),
          WirelessLoRaTransceiver(id, 10) {}

    double readTelemetry() override {
        ensurePowered();
        ++tick_;
        power_.drain(2.0);
        power_.charge(harvest(4.0));                                   // 4 sun-hours
        const double sat  = 50.0 + 6.0 * tick_;
        const double vib  = 1.5 + 2.0 * tick_;
        const double tilt = 0.5 + 1.0 * tick_;
        return lastReading_ = computeHazardIndex(sat, vib, tilt);      // overload #3
    }
    bool evaluateThreshold() override { return lastReading_ > 60.0; }
    std::string typeName() const override { return "HybridProbe"; }
};

} // namespace DisasterCore::Sensors
