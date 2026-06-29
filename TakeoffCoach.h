#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/CanvasWrapper.h"

#include <chrono>
#include <cstdint>
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
    enum class Objective : int
    {
        Fast = 0,
        Control = 1,
        RandomCall = 2,
        Score = 3
    };

    enum class Phase
    {
        Idle,
        Reading,
        Airborne,
        Feedback
    };

    struct Scenario
    {
        Vector carPosition{};
        Vector carVelocity{};
        Rotator carRotation{};
        Vector ballPosition{};
        Vector ballVelocity{};
        Objective objective = Objective::Fast;
        int goalSign = 1;
    };

    struct Solution
    {
        bool valid = false;
        float idealJumpAbsolute = 0.0f;
        float jumpDelay = 0.0f;
        float contactDelay = 0.0f;
        float confidence = 0.0f;
        float alignmentErrorDeg = 0.0f;
        Vector contactPoint{};
        Vector requiredDirection{};
    };

    struct Attempt
    {
        uint64_t generation = 0;
        Phase phase = Phase::Idle;
        Objective objective = Objective::Fast;
        Scenario scenario{};
        Solution solution{};
        Solution jumpSolution{};

        bool previousJump = false;
        bool touched = false;
        bool scored = false;

        float startedAt = 0.0f;
        float jumpAt = 0.0f;
        float feedbackUntil = 0.0f;
        float lastSolveAt = -100.0f;
        float closestDistance = 99999.0f;
        float timingErrorMs = 0.0f;
        float alignmentErrorDeg = 0.0f;
        float previousBallSpeed = 0.0f;

        std::string headline = "Open settings and start a drill.";
        std::string detail;
        std::string correction;
    };

    Attempt attempt_;
    std::mt19937 rng_{std::random_device{}()};

    void registerCvars();
    void requestNewScenario();
    void startScenarioNow(uint64_t generation);
    bool generateScenario(Scenario& scenario);
    bool applyScenario(Scenario scenario);
    bool isScenarioSafe(Scenario scenario) const;

    void onVehicleInput(CarWrapper car, void* params, std::string eventName);
    void updateAttempt(CarWrapper car, float now);
    void updateReading(CarWrapper car, BallWrapper ball, float now);
    void updateAirborne(CarWrapper car, BallWrapper ball, float now);
    void finishAttempt(CarWrapper car, BallWrapper ball, bool touched, bool scored, float now);

    Solution solve(CarWrapper car, BallWrapper ball, float now) const;
    Vector predictBallPosition(Vector position, Vector velocity, float t) const;

    void renderHud(CanvasWrapper canvas);
    void drawGauge(
        CanvasWrapper& canvas,
        Vector2 origin,
        const std::string& label,
        const std::string& value,
        float normalized,
        bool centered);

    void renderDrillTab();
    void renderSetupTab();
    void renderFeedbackTab();
    bool rangeControl(
        const char* label,
        const std::string& minName,
        const std::string& maxName,
        float low,
        float high,
        const char* format);

    float nowSeconds() const;
    float getFloat(const std::string& name) const;
    int getInt(const std::string& name) const;
    bool getBool(const std::string& name) const;
    void setValue(const std::string& name, float value);
    void setValue(const std::string& name, int value);
    void setValue(const std::string& name, bool value);

    float sample(const std::string& minName, const std::string& maxName);
    Objective chooseObjective();
    static std::string objectiveName(Objective objective);

    static float clamp(float value, float low, float high);
    static float length(const Vector& value);
    static float length2D(const Vector& value);
    static Vector normalized2D(const Vector& value);
    static Vector rotate2D(const Vector& value, float radians);
    static float signedAngleDeg2D(const Vector& from, const Vector& to);
    static Vector forwardFromRotator(Rotator rotation);
    static std::string format0(float value);
    static std::string format1(float value);
};

