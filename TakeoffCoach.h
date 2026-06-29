#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/CanvasWrapper.h"

#include <chrono>
#include <random>
#include <memory>
#include <string>

class TakeoffCoach final : public BakkesMod::Plugin::BakkesModPlugin
{
public:
    void onLoad() override;
    void onUnload() override;

private:
    struct AttemptState
    {
        bool active = false;
        bool previousJump = false;
        bool braking = false;
        bool hasBraked = false;

        float brakeStartSeconds = 0.0f;
        float accumulatedBrakeSeconds = 0.0f;

        Vector carStart{};
        Vector takeoffPoint{};
        Vector approachDirection{};
        Vector ballStart{};
        Vector ballVelocity{};

        float targetSpeed = 1350.0f;
        std::string feedback = "Run tc_start in Freeplay";
        std::chrono::steady_clock::time_point startTime{};
    };

    AttemptState attempt_;
    std::mt19937 rng_{std::random_device{}()};

    void registerCommands();
    void startAttempt(bool randomize);
    void resetAttempt();
    void handleVehicleInput(CarWrapper caller, void* params, std::string eventName);
    void gradeTakeoff(CarWrapper car);
    void render(CanvasWrapper canvas);

    float elapsedSeconds() const;
    static float dot2D(const Vector& a, const Vector& b);
    static float length2D(const Vector& v);
    static Vector normalized2D(const Vector& v);
    static Vector rotate2D(const Vector& v, float radians);
    static std::string formatSigned(float value, int precision = 0);
};
