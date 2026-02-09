#include "Conjunction.h"
#include <algorithm>
#include <cmath>

static double lengthKm(const glm::dvec3& v) {
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

static bool sampleKmAt(const Sgp4System& sys, size_t idx, double tSec, glm::dvec3& outKm) {
    return sys.sampleKm(idx, tSec, outKm);
}

static double distKm(const glm::dvec3& a, const glm::dvec3& b) {
    return lengthKm(a - b);
}

bool computeConjunctionsSelectedVsAll(
    const Sgp4System& sys,
    size_t targetIdx,
    double startSimSec,
    const ConjunctionParams& p,
    std::vector<ConjunctionHit>& outHits)
{
    outHits.clear();
    const size_t N = sys.count();
    if (N == 0 || targetIdx >= N) return false;

    const int maxCheck = std::max(1, std::min((int)N, p.maxSatsToCheck));
    const double t0 = startSimSec;
    const double t1 = startSimSec + std::max(1.0, p.horizonSec);
    const double dt = std::max(1.0, p.stepSec);

    const int steps = (int)std::ceil((t1 - t0) / dt) + 1;
    if (steps < 2) return false;

    std::vector<double> times;
    times.reserve((size_t)steps);
    std::vector<glm::dvec3> targetPos;
    targetPos.reserve((size_t)steps);

    for (int s = 0; s < steps; ++s) {
        const double t = t0 + (double)s * dt;
        glm::dvec3 pt;
        if (!sampleKmAt(sys, targetIdx, t, pt)) {
            return false;
        }
        times.push_back(t);
        targetPos.push_back(pt);
    }

    const double thresh = std::max(0.1, p.thresholdKm);
    const double thresh2 = thresh * thresh;

    for (int oi = 0; oi < maxCheck; ++oi) {
        const size_t otherIdx = (size_t)oi;
        if (otherIdx == targetIdx) continue;

        double bestD2 = 1e300;
        double bestT  = times[0];

        bool anyValid = false;

        for (int s = 0; s < steps; ++s) {
            glm::dvec3 po;
            if (!sampleKmAt(sys, otherIdx, times[s], po)) continue;
            anyValid = true;

            const glm::dvec3 d = po - targetPos[s];
            const double d2 = d.x*d.x + d.y*d.y + d.z*d.z;

            if (d2 < bestD2) {
                bestD2 = d2;
                bestT  = times[s];
            }
        }

        if (!anyValid) continue;
        if (bestD2 > thresh2) continue;

        double tca = bestT;
        double miss = std::sqrt(bestD2);

        if (p.refine) {
            const double halfWin = dt;      
            const double fineDt  = std::max(0.5, dt / 10.0);

            double bestFineD = miss;
            double bestFineT = bestT;

            for (double t = bestT - halfWin; t <= bestT + halfWin; t += fineDt) {
                glm::dvec3 po, pt;
                if (!sampleKmAt(sys, otherIdx, t, po)) continue;
                if (!sampleKmAt(sys, targetIdx, t, pt)) continue;

                const double d = distKm(po, pt);
                if (d < bestFineD) {
                    bestFineD = d;
                    bestFineT = t;
                }
            }

            tca = bestFineT;
            miss = bestFineD;
        }

        double relSpeed = 0.0;
        {
            const double eps = 1.0;
            glm::dvec3 pta0, pta1, pob0, pob1;
            if (sampleKmAt(sys, targetIdx, tca - eps, pta0) &&
                sampleKmAt(sys, targetIdx, tca + eps, pta1) &&
                sampleKmAt(sys, otherIdx,  tca - eps, pob0) &&
                sampleKmAt(sys, otherIdx,  tca + eps, pob1))
            {
                glm::dvec3 vT = (pta1 - pta0) / (2.0 * eps); 
                glm::dvec3 vO = (pob1 - pob0) / (2.0 * eps);
                relSpeed = lengthKm(vO - vT);
            }
        }

        ConjunctionHit hit;
        hit.otherIdx = (int)otherIdx;
        hit.tcaSec = tca;
        hit.missKm = miss;
        hit.relSpeedKmS = relSpeed;
        outHits.push_back(hit);
    }

    std::sort(outHits.begin(), outHits.end(), [](const ConjunctionHit& a, const ConjunctionHit& b){
        return a.missKm < b.missKm;
    });

    return true;
}
