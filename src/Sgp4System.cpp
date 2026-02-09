#include "Sgp4System.h"
#include "TleLoader.h"

#include <algorithm>
#include <chrono>
#include <iostream>    
#include <exception>     

#include "Eci.h"
#include "DateTime.h"
#include "Vector.h"
#include "DecayedException.h"     
#include "SatelliteException.h"   

static constexpr double EARTH_RADIUS_KM = 6378.137;

static libsgp4::DateTime nowUtcDateTime()
{
    using namespace std::chrono;

    auto now = system_clock::now();
    time_t tt = system_clock::to_time_t(now);

    tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif

    return libsgp4::DateTime(
        utc.tm_year + 1900,
        utc.tm_mon + 1,
        utc.tm_mday,
        utc.tm_hour,
        utc.tm_min,
        (double)utc.tm_sec
    );
}

bool Sgp4System::loadFromTleFile(const std::string& path)
{
    auto tles = loadTleFile3Line(path);
    if (tles.empty()) return false;

    m_names.clear();
    m_sats.clear();
    m_names.reserve(tles.size());
    m_sats.reserve(tles.size());

    for (const auto& t : tles) {
        const std::string name = t.name.empty() ? std::string("SAT") : t.name;

        libsgp4::Tle tle(name, t.l1, t.l2);

        m_names.push_back(name);
        m_sats.emplace_back(tle);
    }
    return true;
}

glm::vec3 Sgp4System::sample(size_t idx, float simTimeSec, float earthRadiusRender) const
{
    if (idx >= m_sats.size()) return glm::vec3(0);

    static libsgp4::DateTime startUtc = nowUtcDateTime();

    try {
        libsgp4::DateTime t = startUtc.AddSeconds((double)simTimeSec);

        libsgp4::Eci eci = m_sats[idx].sgp4.FindPosition(t);
        libsgp4::Vector posKm = eci.Position();

        const float scale = earthRadiusRender / (float)EARTH_RADIUS_KM;
        return glm::vec3(
            (float)posKm.x * scale,
            (float)posKm.y * scale,
            (float)posKm.z * scale
        );
    }
    catch (const libsgp4::DecayedException&) {
    }
    catch (const libsgp4::SatelliteException&) {
    }
    catch (const std::exception& e) {

        static int printed = 0;
        if (printed < 10) {
            std::cerr << "[SGP4] exception idx=" << idx
                      << " name=" << (idx < m_names.size() ? m_names[idx] : std::string("?"))
                      << " simTimeSec=" << simTimeSec
                      << " what=" << e.what() << "\n";
            printed++;
        }
    }
    catch (...) {
        static int printed = 0;
        if (printed < 10) {
            std::cerr << "[SGP4] unknown exception idx=" << idx
                      << " simTimeSec=" << simTimeSec << "\n";
            printed++;
        }
    }

    return glm::vec3(0);
}

void Sgp4System::positionsAt(float simTimeSec, float earthRadiusRender, std::vector<glm::vec3>& outPos) const
{
    outPos.resize(m_sats.size());
    for (size_t i = 0; i < m_sats.size(); ++i) {
        outPos[i] = sample(i, simTimeSec, earthRadiusRender);
    }
}

double Sgp4System::periodSeconds(size_t idx) const
{
    if (idx >= m_sats.size()) return 0.0;
    double mm = m_sats[idx].tle.MeanMotion();
    return (mm > 1e-9) ? (86400.0 / mm) : 0.0;
}

