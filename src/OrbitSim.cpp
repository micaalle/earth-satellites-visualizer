#include "OrbitSim.h"
#include <random>
#include <cmath>

static float frand(std::mt19937& rng, float a, float b){
    std::uniform_real_distribution<float> dist(a,b);
    return dist(rng);
}

void OrbitSim::initRandom(size_t count, float earthRadius){
    m_earthRadius = earthRadius;
    m_orbits.resize(count);

    std::mt19937 rng(1337);

    for(size_t i=0;i<count;i++){
        float r = frand(rng, 1.15f, 3.0f) * m_earthRadius;
        float inc = frand(rng, 0.0f, 3.1415926f);
        float phase = frand(rng, 0.0f, 6.2831853f);
        float omega = frand(rng, 0.15f, 1.2f);

        m_orbits[i] = { r, inc, omega, phase };
    }
}

glm::vec3 OrbitSim::sample(size_t i, float tSeconds) const{
    const auto& o = m_orbits[i];
    float a = o.phase + o.omega * tSeconds;

    float x = o.radius * std::cos(a);
    float z = o.radius * std::sin(a);
    float y = 0.0f;

    float cy = std::cos(o.inclination);
    float sy = std::sin(o.inclination);

    float ry = y * cy - z * sy;
    float rz = y * sy + z * cy;

    return glm::vec3(x, ry, rz);
}

void OrbitSim::update(float tSeconds, std::vector<glm::vec3>& outPositions) const{
    outPositions.resize(m_orbits.size());
    for(size_t i=0;i<m_orbits.size();i++){
        outPositions[i] = sample(i, tSeconds);
    }
}
