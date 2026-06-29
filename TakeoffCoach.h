#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/CanvasWrapper.h"

#include <chrono>
#include <random>
#include <string>

class TakeoffCoach final
    : public BakkesMod::Plugin::BakkesModPlugin
    , public BakkesMod::Plugin::PluginSettingsWindow
{
public:
    void onLoad() override;
    void onUnload() override;

    void RenderSettings() override;
    std::string GetPluginName() override;
    void SetImGuiContext(uintptr_t ctx) override;

private:
    enum class Cue
    {
        Inactive,
        Wait,
        Jump,
        Late
    };

    struct AttemptState
    {
        bool active = false;
        bool previousJump = false;
        bool randomized = false;
        Cue cue = Cue::Inactive;

        Vector targetPoint{};
        Vector targetDirection{};
        float targetTime = 0.0f;
        float reachRatio = 0.0f;
        float directionErrorDeg = 0.0f;
        float lastReachableSeconds = 0.0f;

        std::string headline = "Open Takeoff Coach settings";
        std::string detail = "Start a drill, then approach the moving ball.";
        std::chrono::steady_clock::time_point startTime{};
    };

    AttemptState attempt_;
    std::mt19937 rng_{std::random_device{}()};

    void registerCvarsAndCommands();
    void startAttempt(bool randomize);
    void resetAttempt();
    void handleVehicleInput(CarWrapper caller, void* params, std::string eventName);
    void updateRecommendation(CarWrapper car);
    void gradeJump(CarWrapper car);
    void renderHud(CanvasWrapper canvas);

    float cvarFloat(const std::string& name) const;
    bool cvarBool(const std::string& name) const;
    void setCvar(const std::string& name, float value);
    void setCvar(const std::string& name, bool value);

    static float clamp(float value, float low, float high);
    static float dot2D(const Vector& a, const Vector& b);
    static float length2D(const Vector& v);
    static Vector normalized2D(const Vector& v);
    static Vector rotate2D(const Vector& v, float radians);
    static float angleDeg2D(const Vector& a, const Vector& b);
    static std::string oneDecimal(float value);
};

