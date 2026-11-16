// Exceptions.hpp  --  UNIT IV : custom exception hierarchy derived from std::exception
#pragma once
#include <exception>
#include <sstream>
#include <string>

namespace DisasterCore {

// Thrown when a node cannot operate (dead battery, hardware fault ...)
class SensorFailureException : public std::exception {
    std::string msg_;
public:
    SensorFailureException(int sensorId, const std::string& reason)
        : msg_("SensorFailureException: node #" + std::to_string(sensorId) +
               " failed - " + reason) {}
    const char* what() const noexcept override { return msg_.c_str(); }
};

// Thrown when a received telemetry packet is corrupted in transit
class TelemetryChecksumMismatch : public std::exception {
    std::string msg_;
public:
    TelemetryChecksumMismatch(int sensorId, unsigned expected, unsigned received) {
        std::ostringstream os;
        os << "TelemetryChecksumMismatch: node #" << sensorId << " expected 0x"
           << std::hex << expected << " but received 0x" << received
           << " (packet discarded)";
        msg_ = os.str();
    }
    const char* what() const noexcept override { return msg_.c_str(); }
};

// Thrown when a reading exceeds the physical measuring range of the instrument
class ThresholdOverflowException : public std::exception {
    std::string msg_;
public:
    ThresholdOverflowException(int sensorId, double value, double limit) {
        std::ostringstream os;
        os << "ThresholdOverflowException: node #" << sensorId << " reading " << value
           << " exceeds instrument limit " << limit;
        msg_ = os.str();
    }
    const char* what() const noexcept override { return msg_.c_str(); }
};

} // namespace DisasterCore
