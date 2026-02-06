#pragma once
#include <string>
#include <vector>

struct TleTriplet {
    std::string name; 
    std::string l1;
    std::string l2;
};

std::vector<TleTriplet> loadTleFile3Line(const std::string& path);
