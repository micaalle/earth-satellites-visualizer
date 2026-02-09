#pragma once
#include <vector>
#include <string>
#include <chrono>
#include <glm/glm.hpp>

class Sgp4System;

struct GroundStation {
    std::string name = "Station";
    double latDeg = 40.7128;    
    double lonDeg = -74.0060;   
    double altKm  = 0.0;
    double maskDeg = 10.0;       
};

struct PassEvent {
    int satIndex = -1;

    double aosSec = 0.0;   
    double losSec = 0.0;
    double tMaxSec = 0.0;

    double maxElDeg = 0.0;
    double rangeAtMaxKm = 0.0;
};

class PassPredictor {
public:
    void predictSelectedSat(
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
    ) const;

    static glm::vec3 stationEcefRender(
        const GroundStation& st,
        float earthRadiusRender,
        float earthRadiusKm
    );

    static double elevationRad(
        const glm::vec3& satEciRender,
        const GroundStation& st,
        float earthRadiusRender,
        float earthRadiusKm,
        float theta,               
        double* outRangeKm = nullptr
    );

private:
    static double deg2rad(double d);
    static double rad2deg(double r);

    static double julianDayUTC(const std::tm& utc, double fracSeconds);
    static double gmstRadians_FromUTC(const std::chrono::system_clock::time_point& tpUtc);

    static glm::vec3 rotateY(const glm::vec3& v, float a);

    static double elevationRad_EcefRho(
        const glm::vec3& rhoEcef,
        double latRad,
        double lonRad
    );

    double thetaAt(
        const std::chrono::system_clock::time_point& startUtcTP,
        double tSec,
        bool rotateEarthGMST,
        float earthLonOffsetDeg
    ) const;

    double refineCrossingSec(
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
    ) const;
};
