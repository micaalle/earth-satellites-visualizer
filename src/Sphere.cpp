#include "Sphere.h"
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

Mesh makeSphere(float r, int slices, int stacks){
    std::vector<Vertex> v;
    std::vector<uint32_t> idx;

    v.reserve((slices+1)*(stacks+1));

    for(int y=0; y<=stacks; ++y){
        float vty = (float)y / (float)stacks;
        float phi = vty * 3.14159265359f;

        for(int x=0; x<=slices; ++x){
            float vtx = (float)x / (float)slices;
            float theta = vtx * 2.0f * 3.14159265359f;

            float sx = sin(phi) * cos(theta);
            float sy = cos(phi);
            float sz = sin(phi) * sin(theta);

            Vertex vert{};
            vert.px = r * sx; vert.py = r * sy; vert.pz = r * sz;
            vert.nx = sx;     vert.ny = sy;     vert.nz = sz;

            vert.u = vtx;
            vert.v = 1.0f - vty;

            v.push_back(vert);
        }
    }

    auto row = slices + 1;
    for(int y=0; y<stacks; ++y){
        for(int x=0; x<slices; ++x){
            uint32_t i0 = (uint32_t)(y*row + x);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + row;
            uint32_t i3 = i2 + 1;

            idx.push_back(i0); idx.push_back(i2); idx.push_back(i1);
            idx.push_back(i1); idx.push_back(i2); idx.push_back(i3);
        }
    }

    Mesh m;
    m.upload(v, idx);
    return m;
}
