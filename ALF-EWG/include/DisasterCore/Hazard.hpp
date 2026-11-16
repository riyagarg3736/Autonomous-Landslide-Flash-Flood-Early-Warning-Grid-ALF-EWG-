// Hazard.hpp  --  UNIT I : namespaces, inline functions, function overloading,
//                          call by reference, return by reference
#pragma once
#include <algorithm>

namespace DisasterCore::Sensors {

// ---- Geo-coordinate ---------------------------------------------------------
struct GeoCoord {
    double lat = 0.0;
    double lon = 0.0;
};

// ---- Strong types so that two-parameter overloads are unambiguous ----------
struct Rainfall   { double mmPerHr; };
struct Slope      { double degrees; };
struct Saturation { double percent; };
struct Vibration  { double hertz; };

// ---- Inline helpers for real-time calculation ------------------------------
inline double normalize(double v, double lo, double hi) {
    return std::clamp((v - lo) / (hi - lo), 0.0, 1.0);
}

// ---- Overloaded computeHazardIndex() ---------------------------------------
// 1) rainfall rate + slope angle  (landslide trigger)
inline double computeHazardIndex(Rainfall r, Slope s) {
    return 100.0 * (0.6 * normalize(r.mmPerHr, 0.0, 120.0) +
                    0.4 * normalize(s.degrees, 0.0, 60.0));
}
// 2) soil saturation + vibration frequency  (ground instability)
inline double computeHazardIndex(Saturation s, Vibration v) {
    return 100.0 * (0.65 * normalize(s.percent, 0.0, 100.0) +
                    0.35 * normalize(v.hertz, 0.0, 20.0));
}
// 3) saturation + vibration + tilt  (used by the hybrid probe)
inline double computeHazardIndex(double saturationPct, double vibrationHz, double tiltDeg) {
    return 100.0 * (0.5 * normalize(saturationPct, 0.0, 100.0) +
                    0.3 * normalize(vibrationHz, 0.0, 20.0) +
                    0.2 * normalize(tiltDeg, 0.0, 10.0));
}
// 4) full composite index for a SensorStation
inline double computeHazardIndex(double rainMmHr, double slopeDeg,
                                 double saturationPct, double vibrationHz) {
    return 100.0 * (0.35 * normalize(rainMmHr, 0.0, 120.0) +
                    0.20 * normalize(slopeDeg, 0.0, 60.0) +
                    0.30 * normalize(saturationPct, 0.0, 100.0) +
                    0.15 * normalize(vibrationHz, 0.0, 20.0));
}

// ---- Call by reference : in-place geocoordinate transformation --------------
inline void shiftCoordinate(GeoCoord& c, double dLat, double dLon) {
    c.lat += dLat;
    c.lon += dLon;
}

// ---- Return by reference : telemetry state accumulation ---------------------
// Returns a reference to the accumulator itself, so calls can be chained and
// the result can even be used as an lvalue:  accumulate(accumulate(t,1),2) += 3;
inline double& accumulate(double& total, double sample) {
    total += sample;
    return total;
}

} // namespace DisasterCore::Sensors
