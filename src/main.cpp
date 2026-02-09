#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <ctime>
#include <cstdio> 

#include "Shader.h"
#include "Camera.h"
#include "OrbitLine.h"
#include "GltfModel.h"

#include "TleLoader.h"
#include "Sgp4System.h"

#include "Conjunction.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

static Camera gCam;
static bool gFirstMouse = true;
static double gLastX = 0.0, gLastY = 0.0;
static bool gMouseCaptured = true;

static int gWinW = 1920;
static int gWinH = 1080;

static int gSSA_HitsForSat = -1;

static bool gPaused = false;
static float gTimeScale = 1.0f;
static float gSimTime = 0.0f;

static int gSelectedSat = 0;

static bool pressed(GLFWwindow *w, int key)
{
    static bool prev[512] = {};
    bool cur = glfwGetKey(w, key) == GLFW_PRESS;
    bool was = prev[key];
    prev[key] = cur;
    return cur && !was;
}
static bool pressedMouse(GLFWwindow *w, int button)
{
    static bool prev[16] = {};
    bool cur = glfwGetMouseButton(w, button) == GLFW_PRESS;
    bool was = prev[button];
    prev[button] = cur;
    return cur && !was;
}

static void framebuffer_size_callback(GLFWwindow *, int w, int h)
{
    gWinW = (w > 1) ? w : 1;
    gWinH = (h > 1) ? h : 1;
    glViewport(0, 0, gWinW, gWinH);
}
static void mouse_callback(GLFWwindow *, double xpos, double ypos)
{
    if (!gMouseCaptured)
        return;

    if (gFirstMouse)
    {
        gLastX = xpos;
        gLastY = ypos;
        gFirstMouse = false;
    }
    float dx = (float)(xpos - gLastX);
    float dy = (float)(ypos - gLastY);
    gLastX = xpos;
    gLastY = ypos;

    gCam.processMouseDelta(dx, dy);
}
static void scroll_callback(GLFWwindow *, double, double yoff)
{
    gCam.fov -= (float)yoff * 2.0f;
    gCam.fov = std::clamp(gCam.fov, 20.0f, 80.0f);
}
static void processInput(GLFWwindow *w, float dt)
{
    if (glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(w, true);

    static bool lastTab = false;
    bool tab = glfwGetKey(w, GLFW_KEY_TAB) == GLFW_PRESS;
    if (tab && !lastTab)
    {
        gMouseCaptured = !gMouseCaptured;
        gFirstMouse = true;
        glfwSetInputMode(w, GLFW_CURSOR, gMouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
    lastTab = tab;

    float spd = gCam.speed * dt;
    if (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        spd *= 3.0f;

    glm::vec3 f = gCam.front();
    glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0, 1, 0)));

    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS)
        gCam.pos += f * spd;
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS)
        gCam.pos -= f * spd;
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS)
        gCam.pos -= r * spd;
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS)
        gCam.pos += r * spd;
    if (glfwGetKey(w, GLFW_KEY_Q) == GLFW_PRESS)
        gCam.pos.y -= spd;
    if (glfwGetKey(w, GLFW_KEY_E) == GLFW_PRESS)
        gCam.pos.y += spd;
}

static std::string pathJoin(const std::string &a, const std::string &b)
{
    if (a.empty())
        return b;
    char c = a.back();
    if (c == '/' || c == '\\')
        return a + b;
    return a + "/" + b;
}

static double deg2rad(double d) { return d * 3.14159265358979323846 / 180.0; }
static double rad2deg(double r) { return r * 180.0 / 3.14159265358979323846; }

static double julianDayUTC(const std::tm &utc, double fracSeconds)
{
    int Y = utc.tm_year + 1900;
    int M = utc.tm_mon + 1;
    int D = utc.tm_mday;

    double hour = (double)utc.tm_hour + (double)utc.tm_min / 60.0 + ((double)utc.tm_sec + fracSeconds) / 3600.0;

    if (M <= 2)
    {
        Y -= 1;
        M += 12;
    }
    int A = Y / 100;
    int B = 2 - A + (A / 4);

    double JD = std::floor(365.25 * (Y + 4716)) + std::floor(30.6001 * (M + 1)) + (double)D + (double)B - 1524.5 + hour / 24.0;
    return JD;
}

static glm::vec3 sunDirECI_FromUTC(const std::chrono::system_clock::time_point &tpUtc)
{
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
    const double T = (JD - 2451545.0) / 36525.0;

    double L0 = std::fmod(280.46646 + T * (36000.76983 + 0.0003032 * T), 360.0);
    if (L0 < 0)
        L0 += 360.0;

    double M = std::fmod(357.52911 + T * (35999.05029 - 0.0001537 * T), 360.0);
    if (M < 0)
        M += 360.0;

    const double Mr = deg2rad(M);

    const double C =
        (1.914602 - T * (0.004817 + 0.000014 * T)) * std::sin(Mr) +
        (0.019993 - 0.000101 * T) * std::sin(2.0 * Mr) +
        0.000289 * std::sin(3.0 * Mr);

    const double trueLong = L0 + C;

    const double omega = 125.04 - 1934.136 * T;
    const double lambda = trueLong - 0.00569 - 0.00478 * std::sin(deg2rad(omega));

    const double eps0 =
        23.0 + (26.0 + (21.448 - T * (46.815 + T * (0.00059 - T * 0.001813))) / 60.0) / 60.0;
    const double eps = eps0 + 0.00256 * std::cos(deg2rad(omega));

    const double lam = deg2rad(lambda);
    const double epr = deg2rad(eps);

    const double alpha = std::atan2(std::cos(epr) * std::sin(lam), std::cos(lam));
    const double delta = std::asin(std::sin(epr) * std::sin(lam));

    glm::dvec3 s;
    s.x = std::cos(delta) * std::cos(alpha);
    s.y = std::cos(delta) * std::sin(alpha);
    s.z = std::sin(delta);

    return glm::normalize(glm::vec3((float)s.x, (float)s.y, (float)s.z));
}

static double gmstRadians_FromUTC(const std::chrono::system_clock::time_point &tpUtc)
{
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
    const double T = (JD - 2451545.0) / 36525.0;

    double gmstDeg =
        280.46061837 +
        360.98564736629 * (JD - 2451545.0) +
        0.000387933 * T * T -
        (T * T * T) / 38710000.0;

    gmstDeg = std::fmod(gmstDeg, 360.0);
    if (gmstDeg < 0)
        gmstDeg += 360.0;

    return deg2rad(gmstDeg);
}

static glm::vec3 rotateY(const glm::vec3 &v, float a)
{
    float c = std::cos(a);
    float s = std::sin(a);
    return glm::vec3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

static float satBrightnessShadow(const glm::vec3 &p, const glm::vec3 &sunDir, float earthRadius)
{
    float dp = glm::dot(p, sunDir);
    if (dp > 0.0f)
        return 1.0f;

    float p2 = glm::dot(p, p);
    float dist2 = p2 - dp * dp;
    dist2 = std::max(dist2, 0.0f);
    float d = std::sqrt(dist2);

    float R = earthRadius;
    float shadowMin = 0.25f;

    if (d < R)
        return shadowMin;

    float edge = R * 1.10f;
    float t = (d - R) / (edge - R);
    t = std::clamp(t, 0.0f, 1.0f);
    return shadowMin + (1.0f - shadowMin) * t;
}

struct SatVertex
{
    glm::vec3 pos;
    float bright;
};

static float gSSA_HorizonHrs = 2.0f;
static float gSSA_StepSec = 20.0f;
static float gSSA_ThresholdKm = 25.0f;
static int gSSA_MaxSats = 12000;
static bool gSSA_Refine = true;
static std::vector<ConjunctionHit> gSSA_Hits;
static int gSSA_SelectedHit = -1;
static float gSSA_LastRunMs = 0.0f;

static bool gSSA_ShowConjLine = true;
static float gSSA_ConjAlpha = 0.85f;

static void clearSSA(OrbitLine &conjLine, std::vector<glm::vec3> &conjPts)
{
    gSSA_Hits.clear();
    gSSA_SelectedHit = -1;
    gSSA_HitsForSat = -1;

    conjPts.clear();
    conjLine.update(conjPts);
}

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

    GLFWwindow *window = glfwCreateWindow(gWinW, gWinH, "Earth + Satellites (SSA)", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to init GLAD\n";
        return 1;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_FRAMEBUFFER_SRGB);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::string shaderDir = "shaders";
    std::string assetDir = "assets";

    Shader earthSh(pathJoin(shaderDir, "earth.vert"), pathJoin(shaderDir, "earth.frag"));
    Shader satSh(pathJoin(shaderDir, "sats.vert"), pathJoin(shaderDir, "sats.frag"));
    Shader orbitSh(pathJoin(shaderDir, "orbit.vert"), pathJoin(shaderDir, "orbit.frag"));

    const float earthRadius = 1.0f;
    const float EARTH_RADIUS_KM = 6378.137f;

    GltfModel earthGltf;
    bool hasEarthGltf = earthGltf.loadFromFile(pathJoin(assetDir, "Earth_1_12756.glb"), true);
    if (!hasEarthGltf)
    {
        std::cerr << "Failed to load GLB: " << pathJoin(assetDir, "Earth_1_12756.glb") << "\n";
    }
    float earthScale = hasEarthGltf ? (earthRadius / earthGltf.boundsRadius()) : 1.0f;

    const std::string tlePath = "data/tles.txt";
    Sgp4System sgp4sys;
    bool loaded = sgp4sys.loadFromTleFile(tlePath);
    if (!loaded)
        std::cerr << "Failed to load TLE file: " << tlePath << "\n";
    size_t satCount = loaded ? sgp4sys.count() : 0;

    std::vector<glm::vec3> satPos(satCount);
    std::vector<SatVertex> satData(satCount);

    GLuint satVAO = 0, satVBO = 0;
    glGenVertexArrays(1, &satVAO);
    glGenBuffers(1, &satVBO);

    glBindVertexArray(satVAO);
    glBindBuffer(GL_ARRAY_BUFFER, satVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(satData.size() * sizeof(SatVertex)),
                 satData.empty() ? nullptr : satData.data(),
                 GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SatVertex), (void *)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(SatVertex), (void *)offsetof(SatVertex, bright));
    glBindVertexArray(0);

    GLuint hiVAO = 0, hiVBO = 0;
    glGenVertexArrays(1, &hiVAO);
    glGenBuffers(1, &hiVBO);

    glBindVertexArray(hiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hiVBO);
    SatVertex initHi{{0, 0, 0}, 1.0f};
    glBufferData(GL_ARRAY_BUFFER, sizeof(SatVertex), &initHi, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SatVertex), (void *)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(SatVertex), (void *)offsetof(SatVertex, bright));
    glBindVertexArray(0);

    OrbitLine orbitLine;
    orbitLine.init();
    std::vector<glm::vec3> orbitPts;

    OrbitLine groundLine;
    groundLine.init();
    std::vector<glm::vec3> groundPts;

    OrbitLine nadirLine;
    nadirLine.init();
    std::vector<glm::vec3> nadirPts;
    nadirPts.reserve(2);

    OrbitLine conjLine;
    conjLine.init();
    std::vector<glm::vec3> conjPts;
    conjPts.reserve(2);

    float lastTime = (float)glfwGetTime();
    const auto startUtcTP = std::chrono::system_clock::now();

    bool useRealSun = true;
    bool rotateEarthGMST = true;
    float earthLonOffsetDeg = 0.0f;

    bool autoOrbitWindow = true;
    bool showHiddenOrbit = true;
    float orbitWindowSec = 240.0f;

    bool showGroundTrack = true;
    float groundDurationSec = 5400.0f;
    int groundSamples = 512;
    bool showBacksideTrack = true;

    bool showNadirLine = true;
    float nadirAlpha = 0.85f;

    float basePointSize = 3.0f;
    float highlightPointSize = 16.0f;
    int drawLimit = (int)std::min<size_t>(satCount, 30000);

    float pickRadiusPx = 10.0f;

    float selAltKm = 0.0f;
    float selLatDeg = 0.0f;
    float selLonDeg = 0.0f;
    bool selInShadow = false;

    while (!glfwWindowShouldClose(window))
    {
        float now = (float)glfwGetTime();
        float dt = now - lastTime;
        lastTime = now;

        processInput(window, dt);

        if (pressed(window, GLFW_KEY_SPACE))
            gPaused = !gPaused;
        if (pressed(window, GLFW_KEY_LEFT_BRACKET))
            gTimeScale = std::max(0.125f, gTimeScale * 0.5f);
        if (pressed(window, GLFW_KEY_RIGHT_BRACKET))
            gTimeScale = std::min(4096.0f, gTimeScale * 2.0f);
        if (pressed(window, GLFW_KEY_R))
            gSimTime = 0.0f;

        if (satCount > 0)
        {
            int before = gSelectedSat;
            if (pressed(window, GLFW_KEY_N))
                gSelectedSat = (gSelectedSat + 1) % (int)satCount;
            if (pressed(window, GLFW_KEY_P))
                gSelectedSat = (gSelectedSat - 1 + (int)satCount) % (int)satCount;

            if (gSelectedSat != before)
                clearSSA(conjLine, conjPts);
        }

        if (!gPaused)
            gSimTime += dt * gTimeScale;

        auto simUtcTP =
            startUtcTP +
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::duration<double>(gSimTime));

        glm::vec3 sunDir = glm::normalize(glm::vec3(1.0f, 0.2f, 0.6f));
        if (useRealSun)
            sunDir = sunDirECI_FromUTC(simUtcTP);

        float theta = 0.0f;
        if (useRealSun && rotateEarthGMST)
            theta = (float)gmstRadians_FromUTC(simUtcTP) + glm::radians(earthLonOffsetDeg);

        glm::mat4 proj = glm::perspective(glm::radians(gCam.fov), (float)gWinW / (float)gWinH, 0.01f, 200.0f);
        glm::mat4 view = gCam.view();
        glm::mat4 VP = proj * view;

        // sat update
        if (loaded && satCount > 0)
        {
            gSelectedSat = std::clamp(gSelectedSat, 0, (int)satCount - 1);

            if (autoOrbitWindow)
            {
                double T = sgp4sys.periodSeconds((size_t)gSelectedSat);
                if (T > 0.0)
                {
                    double w = std::clamp(T * 1.02, 60.0, 172800.0);
                    orbitWindowSec = (float)w;
                }
            }

            sgp4sys.positionsAt(gSimTime, earthRadius, satPos);

            const size_t drawN = (size_t)std::clamp(drawLimit, 0, (int)satCount);
            for (size_t i = 0; i < drawN; i++)
            {
                float b = satBrightnessShadow(satPos[i], sunDir, earthRadius);
                satData[i] = {satPos[i], b};
            }

            glBindBuffer(GL_ARRAY_BUFFER, satVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(drawN * sizeof(SatVertex)), satData.data());

            ImGuiIO &io = ImGui::GetIO();
            if (pressedMouse(window, GLFW_MOUSE_BUTTON_LEFT) && !io.WantCaptureMouse)
            {
                double mx = 0.0, my = 0.0;
                glfwGetCursorPos(window, &mx, &my);

                float mNdcX = (2.0f * (float)mx / (float)gWinW) - 1.0f;
                float mNdcY = 1.0f - (2.0f * (float)my / (float)gWinH);

                float threshNdc = (2.0f * pickRadiusPx) / (float)gWinW;
                float thresh2 = threshNdc * threshNdc;

                int bestId = -1;
                float bestD2 = 1e30f;

                for (size_t i = 0; i < drawN; i++)
                {
                    glm::vec4 clip = VP * glm::vec4(satPos[i], 1.0f);
                    if (clip.w <= 0.0f)
                        continue;

                    glm::vec2 ndc = glm::vec2(clip.x, clip.y) / clip.w;
                    float dx = ndc.x - mNdcX;
                    float dy = ndc.y - mNdcY;
                    float d2 = dx * dx + dy * dy;

                    if (d2 < thresh2 && d2 < bestD2)
                    {
                        bestD2 = d2;
                        bestId = (int)i;
                    }
                }

                if (bestId >= 0 && bestId != gSelectedSat)
                {
                    gSelectedSat = bestId;
                    clearSSA(conjLine, conjPts);
                }
            }

            glm::vec3 selPos = satPos[(size_t)gSelectedSat];

            SatVertex hi{selPos, 1.0f};
            glBindBuffer(GL_ARRAY_BUFFER, hiVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(SatVertex), &hi);

            float r = glm::length(selPos);
            selAltKm = (r - earthRadius) * EARTH_RADIUS_KM;

            glm::vec3 selEcef = rotateY(selPos, -theta);
            float rr = glm::length(selEcef);
            if (rr > 1e-6f)
            {
                glm::vec3 u = selEcef / rr;
                selLatDeg = (float)rad2deg(std::asin(std::clamp((double)u.y, -1.0, 1.0)));
                selLonDeg = (float)rad2deg(std::atan2((double)u.z, (double)u.x));
            }
            else
            {
                selLatDeg = selLonDeg = 0.0f;
            }

            float sb = satBrightnessShadow(selPos, sunDir, earthRadius);
            selInShadow = (sb < 0.6f);

            orbitPts.clear();
            const int orbitSamples = 512;
            orbitPts.reserve(orbitSamples);
            for (int i = 0; i < orbitSamples; i++)
            {
                float t = gSimTime + (orbitWindowSec * (float)i / (float)(orbitSamples - 1));
                orbitPts.push_back(sgp4sys.sample((size_t)gSelectedSat, t, earthRadius));
            }
            if (!orbitPts.empty())
                orbitPts.back() = orbitPts.front();
            orbitLine.update(orbitPts);

            // ground track adjust later i dont really like this maybe disable it on start:/
            if (showGroundTrack)
            {
                groundPts.clear();
                int N = std::max(32, groundSamples);
                groundPts.reserve((size_t)N);

                float t0 = std::max(0.0f, gSimTime - groundDurationSec);
                float t1 = gSimTime;

                for (int i = 0; i < N; i++)
                {
                    float t = t0 + (t1 - t0) * (float)i / (float)(N - 1);

                    auto tp =
                        startUtcTP +
                        std::chrono::duration_cast<std::chrono::system_clock::duration>(
                            std::chrono::duration<double>(t));

                    float th = 0.0f;
                    if (useRealSun && rotateEarthGMST)
                        th = (float)gmstRadians_FromUTC(tp) + glm::radians(earthLonOffsetDeg);

                    glm::vec3 pEci = sgp4sys.sample((size_t)gSelectedSat, t, earthRadius);
                    glm::vec3 pEcef = rotateY(pEci, -th);

                    float len = glm::length(pEcef);
                    if (len < 1e-6f)
                        continue;

                    glm::vec3 s = (pEcef / len) * (earthRadius * 1.002f);
                    glm::vec3 sWorld = rotateY(s, th);
                    groundPts.push_back(sWorld);
                }
                groundLine.update(groundPts);
            }

            if (showNadirLine)
            {
                glm::vec3 surf = glm::normalize(selPos) * (earthRadius * 1.002f);
                nadirPts.clear();
                nadirPts.push_back(selPos);
                nadirPts.push_back(surf);
                nadirLine.update(nadirPts);
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Controls");
        ImGui::Checkbox("Paused", &gPaused);
        ImGui::SliderFloat("Time scale", &gTimeScale, 0.125f, 512.0f, "%.3gx", ImGuiSliderFlags_Logarithmic);
        ImGui::Text("Sim time: %.1f s", gSimTime);

        ImGui::Separator();
        ImGui::Checkbox("Real sun from UTC", &useRealSun);
        ImGui::Checkbox("Rotate Earth (GMST)", &rotateEarthGMST);
        ImGui::SliderFloat("Earth lon offset (deg)", &earthLonOffsetDeg, -180.0f, 180.0f);

        ImGui::Separator();
        ImGui::Text("TLE: %s", tlePath.c_str());
        ImGui::Text("Loaded: %s | sats: %d", loaded ? "yes" : "no", (int)satCount);

        ImGui::Separator();
        {
            int before = gSelectedSat;
            ImGui::SliderInt("Selected", &gSelectedSat, 0, (satCount > 0) ? ((int)satCount - 1) : 0);
            if (gSelectedSat != before)
                clearSSA(conjLine, conjPts);
        }

        if (loaded && satCount > 0)
        {
            ImGui::Text("Name: %s", sgp4sys.name((size_t)gSelectedSat).c_str());
            double T = sgp4sys.periodSeconds((size_t)gSelectedSat);
            if (T > 0.0)
                ImGui::Text("Period: %.1f min", T / 60.0);
            ImGui::Text("Alt: %.0f km", selAltKm);
            ImGui::Text("Lat/Lon: %.2f, %.2f", selLatDeg, selLonDeg);
            ImGui::Text("Shadow: %s", selInShadow ? "YES" : "no");
        }

        ImGui::Separator();
        ImGui::SliderInt("Draw limit", &drawLimit, 1, (satCount > 0) ? (int)satCount : 1);
        ImGui::SliderFloat("Pick radius (px)", &pickRadiusPx, 3.0f, 30.0f, "%.0f");

        ImGui::Separator();
        ImGui::Checkbox("Auto orbit window (1 period)", &autoOrbitWindow);
        ImGui::Checkbox("Show hidden orbit (dim)", &showHiddenOrbit);
        ImGui::SliderFloat("Orbit window (sec)", &orbitWindowSec, 10.0f, 172800.0f, "%.0f", ImGuiSliderFlags_Logarithmic);

        ImGui::Separator();
        ImGui::Checkbox("Ground track", &showGroundTrack);
        ImGui::SliderFloat("Track duration (sec)", &groundDurationSec, 300.0f, 43200.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderInt("Track samples", &groundSamples, 64, 2048);
        ImGui::Checkbox("Show backside track (dim)", &showBacksideTrack);

        ImGui::Separator();
        ImGui::Checkbox("Nadir line (sat -> surface)", &showNadirLine);
        ImGui::SliderFloat("Nadir alpha", &nadirAlpha, 0.05f, 1.0f, "%.2f");

        ImGui::Separator();
        ImGui::SliderFloat("Base dot size", &basePointSize, 1.0f, 8.0f, "%.1f");
        ImGui::SliderFloat("Highlight size", &highlightPointSize, 6.0f, 40.0f, "%.1f");

        ImGui::Separator();
        if (ImGui::CollapsingHeader("SSA - Conjunctions (Selected vs All)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderFloat("Horizon (hours)", &gSSA_HorizonHrs, 0.25f, 24.0f, "%.2f");
            ImGui::SliderFloat("Step (sec)", &gSSA_StepSec, 1.0f, 120.0f, "%.0f");
            ImGui::SliderFloat("Threshold (km)", &gSSA_ThresholdKm, 1.0f, 200.0f, "%.1f");
            ImGui::Checkbox("Refine TCA", &gSSA_Refine);

            int maxMax = (satCount > 0) ? (int)satCount : 1;
            gSSA_MaxSats = std::clamp(gSSA_MaxSats, 1, maxMax);
            ImGui::SliderInt("Max sats to check", &gSSA_MaxSats, 1, maxMax);

            ImGui::Checkbox("Show conjunction line", &gSSA_ShowConjLine);
            ImGui::SliderFloat("Conj line alpha", &gSSA_ConjAlpha, 0.05f, 1.0f, "%.2f");

            if (ImGui::Button("Run conjunction screening"))
            {
                if (loaded && satCount > 0)
                {
                    ConjunctionParams p;
                    p.horizonSec = (double)gSSA_HorizonHrs * 3600.0;
                    p.stepSec = (double)gSSA_StepSec;
                    p.thresholdKm = (double)gSSA_ThresholdKm;
                    p.maxSatsToCheck = gSSA_MaxSats;
                    p.refine = gSSA_Refine;

                    auto t0 = std::chrono::high_resolution_clock::now();
                    bool ok = computeConjunctionsSelectedVsAll(
                        sgp4sys,
                        (size_t)gSelectedSat,
                        (double)gSimTime,
                        p,
                        gSSA_Hits);
                    auto t1 = std::chrono::high_resolution_clock::now();
                    gSSA_LastRunMs = (float)std::chrono::duration<double, std::milli>(t1 - t0).count();

                    if (!ok || gSSA_Hits.empty())
                    {
                        gSSA_SelectedHit = -1;
                        gSSA_HitsForSat = -1;
                        conjPts.clear();
                        conjLine.update(conjPts);
                    }
                    else
                    {
                        gSSA_SelectedHit = 0;
                        gSSA_HitsForSat = gSelectedSat; 
                    }
                }
            }

            ImGui::SameLine();
            ImGui::Text("Last run: %.1f ms | hits: %d", gSSA_LastRunMs, (int)gSSA_Hits.size());

            if (!gSSA_Hits.empty())
            {
                ImGui::BeginChild("##hits", ImVec2(0, 160), true);

                const int showMax = std::min(200, (int)gSSA_Hits.size());
                for (int i = 0; i < showMax; ++i)
                {
                    const auto &h = gSSA_Hits[i];

                    const char *otherName =
                        (h.otherIdx >= 0 && (size_t)h.otherIdx < satCount)
                            ? sgp4sys.name((size_t)h.otherIdx).c_str()
                            : "?";

                    char label[256];
                    std::snprintf(label, sizeof(label),
                                  "%3d) miss %.2f km | TCA +%.1f min | rel %.2f km/s | %s",
                                  i,
                                  h.missKm,
                                  (h.tcaSec - (double)gSimTime) / 60.0,
                                  h.relSpeedKmS,
                                  otherName);

                    if (ImGui::Selectable(label, gSSA_SelectedHit == i))
                        gSSA_SelectedHit = i;
                }

                ImGui::EndChild();

                if (gSSA_SelectedHit >= 0 && gSSA_SelectedHit < (int)gSSA_Hits.size())
                {
                    const auto &h = gSSA_Hits[gSSA_SelectedHit];

                    if (ImGui::Button("Jump to TCA"))
                    {
                        gSimTime = (float)h.tcaSec;
                    }
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("TAB mouse capture | N/P cycle | SPACE pause");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::End();

        if (gSSA_ShowConjLine &&
            loaded && satCount > 0 &&
            gSSA_HitsForSat == gSelectedSat &&
            gSSA_SelectedHit >= 0 && gSSA_SelectedHit < (int)gSSA_Hits.size())
        {
            const auto &h = gSSA_Hits[gSSA_SelectedHit];
            if (h.otherIdx >= 0 && (size_t)h.otherIdx < satCount)
            {
                glm::vec3 a = sgp4sys.sample((size_t)gSelectedSat, (float)h.tcaSec, earthRadius);
                glm::vec3 b = sgp4sys.sample((size_t)h.otherIdx, (float)h.tcaSec, earthRadius);

                conjPts.clear();
                conjPts.push_back(a);
                conjPts.push_back(b);
                conjLine.update(conjPts);
            }
            else
            {
                conjPts.clear();
                conjLine.update(conjPts);
            }
        }
        else
        {
            conjPts.clear();
            conjLine.update(conjPts);
        }

        glClearColor(0.005f, 0.007f, 0.015f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 model = glm::mat4(1.0f);
        if (useRealSun && rotateEarthGMST)
            model = glm::rotate(glm::mat4(1.0f), theta, glm::vec3(0, 1, 0));
        model = model * glm::scale(glm::mat4(1.0f), glm::vec3(earthScale));

        if (hasEarthGltf)
        {
            earthSh.use();
            earthSh.setMat4("uModel", model);
            earthSh.setMat4("uView", view);
            earthSh.setMat4("uProj", proj);
            earthSh.setVec3("uSunDir", sunDir);
            earthSh.setVec3("uCamPos", gCam.pos);
            earthGltf.drawEarthStyle(earthSh, 0);
        }

        glDepthMask(GL_FALSE);
        glLineWidth(2.0f);

        orbitSh.use();
        orbitSh.setMat4("uView", view);
        orbitSh.setMat4("uProj", proj);

        glDepthFunc(GL_LESS);
        orbitSh.setFloat("uAlpha", 0.75f);
        orbitLine.draw();

        if (showHiddenOrbit)
        {
            glDepthFunc(GL_GREATER);
            orbitSh.setFloat("uAlpha", 0.18f);
            orbitLine.draw();
            glDepthFunc(GL_LESS);
        }

        if (showGroundTrack)
        {
            orbitSh.setFloat("uAlpha", 0.90f);
            groundLine.draw();

            if (showBacksideTrack)
            {
                glDepthFunc(GL_GREATER);
                orbitSh.setFloat("uAlpha", 0.20f);
                groundLine.draw();
                glDepthFunc(GL_LESS);
            }
        }

        if (showNadirLine)
        {
            glDepthFunc(GL_LESS);
            orbitSh.setFloat("uAlpha", nadirAlpha);
            nadirLine.draw();
        }

        // ssa conjuction line adjust this is tmp for now
        if (gSSA_ShowConjLine && !conjPts.empty() && gSSA_HitsForSat == gSelectedSat)
        {
            glDepthFunc(GL_LESS);
            orbitSh.setFloat("uAlpha", gSSA_ConjAlpha);
            conjLine.draw();
        }

        glDepthFunc(GL_LESS);

        if (loaded && satCount > 0)
        {
            const GLsizei drawN = (GLsizei)std::clamp(drawLimit, 0, (int)satCount);

            satSh.use();
            satSh.setMat4("uView", view);
            satSh.setMat4("uProj", proj);

            satSh.setInt("uIsHighlight", 0);
            satSh.setFloat("uBaseSize", basePointSize);
            satSh.setFloat("uHighlightSize", highlightPointSize);

            glBindVertexArray(satVAO);
            glDrawArrays(GL_POINTS, 0, drawN);
            glBindVertexArray(0);

            satSh.setInt("uIsHighlight", 1);
            glBindVertexArray(hiVAO);
            glDrawArrays(GL_POINTS, 0, 1);
            glBindVertexArray(0);
        }

        glDepthMask(GL_TRUE);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    earthGltf.destroy();

    glDeleteBuffers(1, &satVBO);
    glDeleteVertexArrays(1, &satVAO);

    glDeleteBuffers(1, &hiVBO);
    glDeleteVertexArrays(1, &hiVAO);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

