#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>


#include "Tle.h"
#include "SGP4.h"

class Sgp4System {
public:
    bool loadFromTleFile(const std::string& path);

    size_t count() const { return m_names.size(); }
    const std::string& name(size_t i) const { return m_names[i]; }

    void positionsAt(float simTimeSec, float earthRadiusRender, std::vector<glm::vec3>& outPos) const;
    glm::vec3 sample(size_t idx, float simTimeSec, float earthRadiusRender) const;

    double periodSeconds(size_t idx) const;


private:
    std::vector<std::string> m_names;

    struct SatImpl {
        libsgp4::Tle  tle;
        libsgp4::SGP4 sgp4;
        SatImpl(const libsgp4::Tle& t) : tle(t), sgp4(t) {}
    };

    std::vector<SatImpl> m_sats;
};
