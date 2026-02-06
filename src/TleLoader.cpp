#include "TleLoader.h"
#include <fstream>
#include <sstream>

static bool startsWith(const std::string& s, char c) {
    return !s.empty() && s[0] == c;
}

std::vector<TleTriplet> loadTleFile3Line(const std::string& path)
{
    std::ifstream f(path);
    std::vector<TleTriplet> out;
    if (!f.is_open()) return out;

    std::string line;
    std::string pendingName;
    std::string l1;

    while (std::getline(f, line)) {

        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (startsWith(line, '1')) {
            l1 = line;
        } else if (startsWith(line, '2')) {
            if (!l1.empty()) {
                TleTriplet t;
                t.name = pendingName;
                t.l1 = l1;
                t.l2 = line;
                out.push_back(std::move(t));
            }
            pendingName.clear();
            l1.clear();
        } else {
            pendingName = line;
        }
    }

    return out;
}
