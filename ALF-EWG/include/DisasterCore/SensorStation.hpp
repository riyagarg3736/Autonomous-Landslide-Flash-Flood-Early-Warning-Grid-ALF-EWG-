// SensorStation.hpp  --  UNIT I (friend class) + UNIT II (abstraction, const
//                        correctness, static members, operator overloading)
#pragma once
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "Hazard.hpp"
#include "TelemetryBuffer.hpp"

namespace DisasterCore::Sensors {

class SensorStation;

// ---------------------------------------------------------------------------
// ZoneVector : result of  station + station  (regional zone aggregate)
// ---------------------------------------------------------------------------
class ZoneVector {
    std::string      zone_;
    std::vector<int> ids_;
    double           totalHazard_ = 0.0;
public:
    ZoneVector() = default;
    explicit ZoneVector(const SensorStation& s);                 // defined below

    ZoneVector operator+(const ZoneVector& o) const {            // zone + zone
        ZoneVector r(*this);
        if (r.zone_.empty())                               r.zone_ = o.zone_;
        else if (!o.zone_.empty() && r.zone_ != o.zone_)   r.zone_ += "+" + o.zone_;
        r.ids_.insert(r.ids_.end(), o.ids_.begin(), o.ids_.end());
        r.totalHazard_ += o.totalHazard_;
        return r;
    }
    ZoneVector operator+(const SensorStation& s) const;          // zone + station

    std::size_t        size() const        { return ids_.size(); }
    double             meanHazard() const  { return ids_.empty() ? 0.0 : totalHazard_ / ids_.size(); }
    const std::string& zone() const        { return zone_; }

    friend std::ostream& operator<<(std::ostream& os, const ZoneVector& z) {
        os << "Zone[" << z.zone_ << "] stations={";
        for (std::size_t i = 0; i < z.ids_.size(); ++i) os << (i ? "," : "") << z.ids_[i];
        return os << "} mean hazard=" << std::fixed << std::setprecision(1)
                  << z.meanHazard() << std::defaultfloat << std::setprecision(6);
    }
};

// ---------------------------------------------------------------------------
// SensorStation
// ---------------------------------------------------------------------------
class SensorStation {
    int          id_;
    std::string  name_;
    std::string  zone_;
    GeoCoord     pos_;
    double       rainfall_;     // mm/hr   } raw private metrics: no getters,
    double       slope_;        // degrees } only the friend TelemetryAuditor
    double       saturation_;   // %       } and the stream operators see them
    double       vibration_;    // Hz      }
    TelemetryBuffer history_;   // containership of a dynamic buffer

    // ---- static data members (shared by the whole network) ------------------
    inline static int activeStations_   = 0;
    inline static int globalAlertLevel_ = 0;

    friend class TelemetryAuditor;                              // Unit I : friend class
    friend std::ostream& operator<<(std::ostream&, const SensorStation&);
    friend std::istream& operator>>(std::istream&, SensorStation&);

public:
    SensorStation(int id, std::string name, std::string zone, GeoCoord pos,
                  double rain, double slope, double sat, double vib)
        : id_(id), name_(std::move(name)), zone_(std::move(zone)), pos_(pos),
          rainfall_(rain), slope_(slope), saturation_(sat), vibration_(vib),
          history_(8) { ++activeStations_; }

    SensorStation() : SensorStation(0, "unnamed", "unassigned", GeoCoord{}, 0, 0, 0, 0) {}

    SensorStation(const SensorStation& o)
        : id_(o.id_), name_(o.name_), zone_(o.zone_), pos_(o.pos_),
          rainfall_(o.rainfall_), slope_(o.slope_), saturation_(o.saturation_),
          vibration_(o.vibration_), history_(o.history_) { ++activeStations_; }

    SensorStation& operator=(const SensorStation&) = default;

    ~SensorStation() { --activeStations_; }

    // ---- const-correct public interface ------------------------------------
    int                id()   const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& zone() const { return zone_; }
    double hazardIndex() const {
        return computeHazardIndex(rainfall_, slope_, saturation_, vibration_);
    }
    void recordSample(double v)        { history_.push(v); }
    double meanSample() const          { return history_.mean(); }

    // Call by reference / return by reference on the geocoordinate
    void relocate(double dLat, double dLon) { shiftCoordinate(pos_, dLat, dLon); }
    const GeoCoord& position() const        { return pos_; }

    // ---- static member functions -------------------------------------------
    static int  activeCount()                 { return activeStations_; }
    static int  globalAlertLevel()            { return globalAlertLevel_; }
    static void raiseGlobalAlert(int level)   { if (level > globalAlertLevel_) globalAlertLevel_ = level; }
    static void resetGlobalAlert()            { globalAlertLevel_ = 0; }

    // ---- operator overloading ----------------------------------------------
    bool operator>(const SensorStation& o) const { return hazardIndex() > o.hazardIndex(); }
    ZoneVector operator+(const SensorStation& o) const;       // defined below
};

// ---------------------------------------------------------------------------
// ZoneVector / SensorStation operator + definitions
// ---------------------------------------------------------------------------
inline ZoneVector::ZoneVector(const SensorStation& s)
    : zone_(s.zone()), ids_{s.id()}, totalHazard_(s.hazardIndex()) {}

inline ZoneVector ZoneVector::operator+(const SensorStation& s) const {
    return *this + ZoneVector(s);
}
inline ZoneVector SensorStation::operator+(const SensorStation& o) const {
    return ZoneVector(*this) + ZoneVector(o);
}

// ---------------------------------------------------------------------------
// Stream insertion / extraction
//   * file streams  -> compact machine-readable record (round-trips via >>)
//   * other streams -> human-readable console line
// ---------------------------------------------------------------------------
inline std::ostream& operator<<(std::ostream& os, const SensorStation& s) {
    if (dynamic_cast<std::ofstream*>(&os)) {
        os << s.id_ << ' ' << s.name_ << ' ' << s.zone_ << ' ' << s.pos_.lat << ' '
           << s.pos_.lon << ' ' << s.rainfall_ << ' ' << s.slope_ << ' '
           << s.saturation_ << ' ' << s.vibration_ << '\n';
    } else {
        os << "Station #" << s.id_ << " " << std::left << std::setw(16) << s.name_
           << std::setw(12) << s.zone_ << std::right << " (" << s.pos_.lat << ", "
           << s.pos_.lon << ")  hazard=" << std::fixed << std::setprecision(1)
           << s.hazardIndex() << std::defaultfloat << std::setprecision(6);
    }
    return os;
}

inline std::istream& operator>>(std::istream& is, SensorStation& s) {
    int id; std::string name, zone; double lat, lon, r, sl, sa, v;
    if (is >> id >> name >> zone >> lat >> lon >> r >> sl >> sa >> v) {
        s.id_ = id; s.name_ = name; s.zone_ = zone; s.pos_ = GeoCoord{lat, lon};
        s.rainfall_ = r; s.slope_ = sl; s.saturation_ = sa; s.vibration_ = v;
    }
    return is;
}

// ---------------------------------------------------------------------------
// TelemetryAuditor : friend class that reads raw private metrics (no getters)
// ---------------------------------------------------------------------------
class TelemetryAuditor {
public:
    void audit(const SensorStation& s) const {
        std::cout << "  [AUDIT] station #" << s.id_ << " raw metrics -> rain=" << s.rainfall_
                  << " mm/hr, slope=" << s.slope_ << " deg, saturation=" << s.saturation_
                  << " %, vibration=" << s.vibration_ << " Hz, history=" << s.history_ << '\n';
        if (s.saturation_ > 85.0)
            std::cout << "  [AUDIT] WARNING: soil saturation critical at " << s.name_ << '\n';
    }
};

} // namespace DisasterCore::Sensors
