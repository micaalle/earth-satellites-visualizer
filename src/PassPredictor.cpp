#include "PassPredictor.h"
#include "Sgp4System.h"

#include <cmath>
#include <algorithm>
#include <ctime>

double PassPredictor::deg2rad(double d) { return d * 3.14159265358979323846 / 180.0; }
double PassPredictor::rad2deg(double r) { return r * 180.0 / 3.14159265358979323846; }

glm::vec3 PassPredictor::rotateY(const glm::vec3& v, float a) {
    float c = std::cos(a);
    float s = std::sin(a);
    return glm::vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

double PassPredictor::julianDayUTC(const std::tm& utc, double fracSeconds) {
    int Y = utc.tm_year + 1900;
    int M = utc.tm_mon + 1;
    int D = utc.tm_mday;

    double hour = (double)utc.tm_hour + (double)utc.tm_min / 60.0 + ((double)utc.tm_sec + fracSeconds) / 3600.0;

    if (M <= 2) { Y -= 1; M += 12; }
    int A = Y / 100;
    int B = 2 - A + (A / 4);

    double JD = std::floor(365.25 * (Y + 4716)) +
                std::floor(30.6001 * (M + 1)) +
                (double)D + (double)B - 1524.5 + hour / 24.0;
    return JD;
}

double PassPredictor::gmstRadians_FromUTC(const std::chrono::system_clock::time_point& tpUtc) {
    using namespace std::chrono;
    const auto tt = system_clock::to_time_t(tpUtc);

    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif

    const auto base = system_clock::from_time_t(tt);
    const double frac = duration<double>(tpUtc - base).count();

    const double JD = julianDayUTC(utc, frac);
    const double T  = (JD - 2451545.0) / 36525.0;

    double gmstDeg =
        280.46061837 +
        360.98564736629 * (JD - 2451545.0) +
        0.000387933 * T * T -
        (T * T * T) / 38710000.0;

    gmstDeg = std::fmod(gmstDeg, 360.0);
    if (gmstDeg < 0) gmstDeg += 360.0;

    return deg2rad(gmstDeg);
}

glm::vec3 PassPredictor::stationEcefRender(const GroundStation& st, float earthRadiusRender, float earthRadiusKm) {
    const double lat = deg2rad(st.latDeg);
    const double lon = deg2rad(st.lonDeg);

    const float altUnits = (float)(st.altKm / earthRadiusKm) * earthRadiusRender;
    const float r = earthRadiusRender + altUnits;

    const float cl = (float)std::cos(lat);
    const float sl = (float)std::sin(lat);
    const float co = (float)std::cos(lon);
    const float so = (float)std::sin(lon);

    return glm::vec3(r * cl * co, r * sl, r * cl * so);
}

double PassPredictor::elevationRad_EcefRho(const glm::vec3& rhoEcef, double latRad, double lonRad) {
    const double slat = std::sin(latRad), clat = std::cos(latRad);
    const double slon = std::sin(lonRad), clon = std::cos(lonRad);

    glm::dvec3 east(-slon, 0.0,  clon);
    glm::dvec3 north(-slat * clon, clat, -slat * slon);
    glm::dvec3 up( clat * clon, slat,  clat * slon);

    glm::dvec3 rho((double)rhoEcef.x, (double)rhoEcef.y, (double)rhoEcef.z);

    const double E = east.x * rho.x + east.y * rho.y + east.z * rho.z;
    const double N = north.x * rho.x + north.y * rho.y + north.z * rho.z;
    const double U = up.x * rho.x + up.y * rho.y + up.z * rho.z;

    const double horiz = std::sqrt(E*E + N*N);
    return std::atan2(U, horiz);
}

double PassPredictor::elevationRad(
    const glm::vec3& satEciRender,
    const GroundStation& st,
    float earthRadiusRender,
    float earthRadiusKm,
    float theta,
    double* outRangeKm
) {
    const double lat = deg2rad(st.latDeg);
    const double lon = deg2rad(st.lonDeg);

    const glm::vec3 stEcef = stationEcefRender(st, earthRadiusRender, earthRadiusKm);
    const glm::vec3 satEcef = rotateY(satEciRender, -theta);

    const glm::vec3 rho = satEcef - stEcef;

    if (outRangeKm) {
        const double kmPerUnit = (double)earthRadiusKm / (double)earthRadiusRender;
        *outRangeKm = glm::length(rho) * kmPerUnit;
    }
    return elevationRad_EcefRho(rho, lat, lon);
}

double PassPredictor::thetaAt(
    const std::chrono::system_clock::time_point& startUtcTP,
    double tSec,
    bool rotateEarthGMST,
    float earthLonOffsetDeg
) const {
    if (!rotateEarthGMST) return 0.0;

    auto tp =
        startUtcTP +
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::duration<double>(tSec));

    double th = gmstRadians_FromUTC(tp) + deg2rad((double)earthLonOffsetDeg);
    return th;
}

double PassPredictor::refineCrossingSec(
    const Sgp4System& sys,
    int satIndex,
    float earthRadiusRender,
    float earthRadiusKm,
    const std::chrono::system_clock::time_point& startUtcTP,
    double a,
    double b,
    double maskRad,
    const GroundStation& st,
    bool rotateEarthGMST,
    float earthLonOffsetDeg
) const {

    double fa = 0.0, fb = 0.0;
    {
        float thA = (float)thetaAt(startUtcTP, a, rotateEarthGMST, earthLonOffsetDeg);
        float thB = (float)thetaAt(startUtcTP, b, rotateEarthGMST, earthLonOffsetDeg);
        const glm::vec3 pA = sys.sample((size_t)satIndex, (float)a, earthRadiusRender);
        const glm::vec3 pB = sys.sample((size_t)satIndex, (float)b, earthRadiusRender);
        fa = elevationRad(pA, st, earthRadiusRender, earthRadiusKm, thA, nullptr) - maskRad;
        fb = elevationRad(pB, st, earthRadiusRender, earthRadiusKm, thB, nullptr) - maskRad;
    }

    if (fa * fb > 0.0) return 0.5 * (a + b);

    for (int it = 0; it < 25; ++it) {
        double m = 0.5 * (a + b);
        float thM = (float)thetaAt(startUtcTP, m, rotateEarthGMST, earthLonOffsetDeg);
        const glm::vec3 pM = sys.sample((size_t)satIndex, (float)m, earthRadiusRender);
        double fm = elevationRad(pM, st, earthRadiusRender, earthRadiusKm, thM, nullptr) - maskRad;

        if (fa * fm <= 0.0) { b = m; fb = fm; }
        else { a = m; fa = fm; }
    }
    return 0.5 * (a + b);
}

void PassPredictor::predictSelectedSat(
    const Sgp4System& sys,
    int satIndex,
    float earthRadiusRender,
    float earthRadiusKm,
    const std::chrono::system_clock::time_point& startUtcTP,
    double tStartSec,
    double horizonSec,
    double stepSec,
    const GroundStation& st,
    bool rotateEarthGMST,
    float earthLonOffsetDeg,
    std::vector<PassEvent>& outPasses
) const {
    outPasses.clear();
    if (satIndex < 0) return;

    const double maskRad = deg2rad(st.maskDeg);
    const double tEnd = tStartSec + horizonSec;
    stepSec = std::max(1.0, stepSec);

    auto elevAt = [&](double t, double* rangeKm) -> double {
        float th = (float)thetaAt(startUtcTP, t, rotateEarthGMST, earthLonOffsetDeg);
        glm::vec3 p = sys.sample((size_t)satIndex, (float)t, earthRadiusRender);
        return elevationRad(p, st, earthRadiusRender, earthRadiusKm, th, rangeKm);
    };

    double tPrev = tStartSec;
    double rangePrev = 0.0;
    double ePrev = elevAt(tPrev, &rangePrev);
    bool inPass = (ePrev > maskRad);

    PassEvent cur{};
    cur.satIndex = satIndex;
    cur.maxElDeg = rad2deg(ePrev);
    cur.tMaxSec = tPrev;
    cur.rangeAtMaxKm = rangePrev;

    if (inPass) {
        cur.aosSec = tPrev; 
    }

    for (double t = tStartSec + stepSec; t <= tEnd + 1e-6; t += stepSec) {
        double rangeNow = 0.0;
        double eNow = elevAt(t, &rangeNow);
        bool nowIn = (eNow > maskRad);

        if (nowIn) {
            double eDeg = rad2deg(eNow);
            if (eDeg > cur.maxElDeg) {
                cur.maxElDeg = eDeg;
                cur.tMaxSec = t;
                cur.rangeAtMaxKm = rangeNow;
            }
        }

        if (!inPass && nowIn) {
            double aos = refineCrossingSec(
                sys, satIndex, earthRadiusRender, earthRadiusKm, startUtcTP,
                tPrev, t, maskRad, st, rotateEarthGMST, earthLonOffsetDeg
            );
            cur = PassEvent{};
            cur.satIndex = satIndex;
            cur.aosSec = aos;

            double rA = 0.0;
            double eA = elevAt(aos, &rA);
            cur.maxElDeg = rad2deg(eA);
            cur.tMaxSec = aos;
            cur.rangeAtMaxKm = rA;
            inPass = true;
        }

        if (inPass && !nowIn) {
            double los = refineCrossingSec(
                sys, satIndex, earthRadiusRender, earthRadiusKm, startUtcTP,
                tPrev, t, maskRad, st, rotateEarthGMST, earthLonOffsetDeg
            );
            cur.losSec = los;
            outPasses.push_back(cur);
            inPass = false;
        }

        tPrev = t;
        ePrev = eNow;
    }

    if (inPass) {
        cur.losSec = tEnd;
        outPasses.push_back(cur);
    }
}
