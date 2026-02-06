#pragma once
#include <vector>
#include <glm/glm.hpp>

struct SatOrbit {
    float radius;     
    float inclination; 
    float omega;       
    float phase;       
};

class OrbitSim {
public:
    void initRandom(size_t count, float earthRadius);

    void update(float tSeconds, std::vector<glm::vec3>& outPositions) const;

    glm::vec3 sample(size_t i, float tSeconds) const;

    size_t count() const { return m_orbits.size(); }

private:
    float m_earthRadius = 1.0f;
    std::vector<SatOrbit> m_orbits;
};
