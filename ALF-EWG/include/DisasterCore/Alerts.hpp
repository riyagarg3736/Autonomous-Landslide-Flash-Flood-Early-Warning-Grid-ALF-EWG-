// Alerts.hpp  --  namespace DisasterCore::Alerts
//   UNIT II : static members (EarlyWarningSiren)
//   UNIT IV : binary fstream persistence with transactional rollback (seekp/seekg),
//             custom functors, custom stream manipulators
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace DisasterCore::Alerts {

// ---------------------------------------------------------------------------
// Alert levels
// ---------------------------------------------------------------------------
enum class AlertLevel { Normal = 0, Watch = 1, Warning = 2, Evacuate = 3 };

inline AlertLevel levelFromHazard(double h) {
    if (h >= 75.0) return AlertLevel::Evacuate;
    if (h >= 55.0) return AlertLevel::Warning;
    if (h >= 35.0) return AlertLevel::Watch;
    return AlertLevel::Normal;
}
inline const char* toString(AlertLevel l) {
    switch (l) {
        case AlertLevel::Normal:   return "NORMAL";
        case AlertLevel::Watch:    return "WATCH";
        case AlertLevel::Warning:  return "WARNING";
        case AlertLevel::Evacuate: return "EVACUATE";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// EarlyWarningSiren : static counter of sirens sounding network-wide
// ---------------------------------------------------------------------------
class EarlyWarningSiren {
    int         id_;
    std::string region_;
    AlertLevel  level_  = AlertLevel::Normal;
    bool        active_ = false;
    inline static int sirensActive_ = 0;
public:
    EarlyWarningSiren(int id, std::string region) : id_(id), region_(std::move(region)) {}
    EarlyWarningSiren(const EarlyWarningSiren&)            = delete;
    EarlyWarningSiren& operator=(const EarlyWarningSiren&) = delete;
    ~EarlyWarningSiren() { if (active_) --sirensActive_; }

    void set(AlertLevel l) {
        level_ = l;
        const bool shouldSound = static_cast<int>(l) >= static_cast<int>(AlertLevel::Warning);
        if (shouldSound && !active_)       { active_ = true;  ++sirensActive_; }
        else if (!shouldSound && active_)  { active_ = false; --sirensActive_; }
    }
    int                id() const     { return id_; }
    AlertLevel         level() const  { return level_; }
    bool               isActive() const { return active_; }
    const std::string& region() const { return region_; }
    static int         activeSirens() { return sirensActive_; }
};

// ---------------------------------------------------------------------------
// Evacuation routing : functors used with STL algorithms
// ---------------------------------------------------------------------------
struct EvacuationRoute {
    std::string village;
    double      hazard;
    int         population;
    double      distanceKm;
};

// Comparator functor: highest hazard first, then larger population, then shorter route
struct EvacuationPriority {
    bool operator()(const EvacuationRoute& a, const EvacuationRoute& b) const {
        if (a.hazard != b.hazard)         return a.hazard > b.hazard;
        if (a.population != b.population) return a.population > b.population;
        return a.distanceKm < b.distanceKm;
    }
};

// ---------------------------------------------------------------------------
// Custom stream manipulators for tabular dispatch output
// ---------------------------------------------------------------------------
inline std::ostream& tableRule(std::ostream& os) {                 // plain manipulator
    return os << std::string(58, '-') << '\n';
}
struct Col { int width; };                                         // parameterised manipulator
inline Col col(int w) { return Col{w}; }
inline std::ostream& operator<<(std::ostream& os, Col c) {
    return os << std::left << std::setw(c.width);
}
inline std::ostream& alertHeader(std::ostream& os) {
    os << tableRule << col(18) << "VILLAGE" << col(10) << "LEVEL" << col(9) << "HAZARD"
       << col(12) << "POPULATION" << col(8) << "DIST(km)" << '\n' << tableRule;
    return os;
}

// Stateful functor for std::for_each : prints one dispatch row, counts rows
struct DispatchPrinter {
    int printed = 0;
    void operator()(const EvacuationRoute& r) {
        std::cout << col(18) << r.village << col(10) << toString(levelFromHazard(r.hazard))
                  << col(9) << std::fixed << std::setprecision(1) << r.hazard
                  << col(12) << r.population << col(8) << r.distanceKm
                  << std::defaultfloat << std::setprecision(6) << '\n';
        ++printed;
    }
};

// ---------------------------------------------------------------------------
// IncidentRecord : fixed-size POD, safe to serialise in binary
// ---------------------------------------------------------------------------
struct IncidentRecord {
    std::int32_t stationId;
    std::int32_t level;
    double       hazard;
    std::int64_t cycle;
    char         zone[24];
    char         message[48];

    bool operator==(const IncidentRecord& o) const {
        return stationId == o.stationId && level == o.level && hazard == o.hazard &&
               cycle == o.cycle && std::strncmp(zone, o.zone, sizeof zone) == 0 &&
               std::strncmp(message, o.message, sizeof message) == 0;
    }
};

inline IncidentRecord makeRecord(int stationId, int level, double hazard, long cycle,
                                 const std::string& zone, const std::string& message) {
    IncidentRecord r{};
    r.stationId = stationId;
    r.level     = level;
    r.hazard    = hazard;
    r.cycle     = cycle;
    std::strncpy(r.zone, zone.c_str(), sizeof r.zone - 1);
    std::strncpy(r.message, message.c_str(), sizeof r.message - 1);
    return r;
}

// ---------------------------------------------------------------------------
// IncidentLog : binary std::fstream with transactional append.
//   begin  : remember the end-of-file offset (seekp / tellp)
//   write  : append the record, flush
//   verify : seekg back and compare the bytes that reached the file
//   commit : success
//   rollback (on any failure): truncate to the saved offset, restore positions
// ---------------------------------------------------------------------------
class IncidentLog {
    std::string  path_;
    std::fstream file_;

    void rollback(std::streampos start) {
        file_.clear();
        file_.flush();
        std::filesystem::resize_file(path_, static_cast<std::uintmax_t>(
                                                static_cast<std::streamoff>(start)));
        file_.clear();
        file_.seekp(start);
        file_.seekg(start);
    }
public:
    explicit IncidentLog(std::string path) : path_(std::move(path)) {
        { std::ofstream touch(path_, std::ios::binary | std::ios::app); }   // create if absent
        file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_) throw std::runtime_error("cannot open incident log: " + path_);
    }

    void append(const IncidentRecord& rec, bool simulateCrash = false) {
        file_.clear();
        file_.seekp(0, std::ios::end);
        const std::streampos start = file_.tellp();
        try {
            file_.write(reinterpret_cast<const char*>(&rec), sizeof rec);
            file_.flush();
            if (simulateCrash) throw std::runtime_error("simulated disk failure mid-transaction");

            IncidentRecord check{};
            file_.seekg(start);
            file_.read(reinterpret_cast<char*>(&check), sizeof check);
            if (!file_ || !(check == rec)) throw std::runtime_error("write verification failed");
            file_.seekp(0, std::ios::end);
        } catch (...) {
            rollback(start);
            throw;
        }
    }

    std::size_t count() {
        file_.clear();
        file_.seekg(0, std::ios::end);
        return static_cast<std::size_t>(static_cast<std::streamoff>(file_.tellg())) /
               sizeof(IncidentRecord);
    }

    std::vector<IncidentRecord> readAll() {
        std::vector<IncidentRecord> out;
        file_.clear();
        file_.seekg(0, std::ios::beg);
        IncidentRecord r{};
        while (file_.read(reinterpret_cast<char*>(&r), sizeof r)) out.push_back(r);
        file_.clear();
        file_.seekp(0, std::ios::end);
        return out;
    }
};

} // namespace DisasterCore::Alerts
