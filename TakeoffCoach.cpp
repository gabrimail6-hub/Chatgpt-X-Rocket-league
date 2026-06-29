#include "TakeoffCoach.h"

#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "IMGUI/imgui.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

BAKKESMOD_PLUGIN(
    TakeoffCoach,
    "Takeoff Coach",
    "2.0.0",
    PLUGINTYPE_FREEPLAY
)

namespace
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float UU_PER_METER = 100.0f;
    constexpr float BALL_GRAVITY = -650.0f;

    constexpr float JUMP_IMPULSE = 292.0f;
    constexpr float SECOND_JUMP_IMPULSE = 292.0f;
    constexpr float SECOND_JUMP_DELAY = 0.16f;
    constexpr float BOOST_ACCELERATION = 992.0f;

    constexpr float CAR_Z = 17.0f;
    constexpr float CONTACT_ALLOWANCE = 155.0f;
}

void TakeoffCoach::onLoad()
{
    registerCvarsAndCommands();

    gameWrapper->HookEventWithCaller(
        "Function TAGame.Car_TA.SetVehicleInput",
        [this](CarWrapper caller, void* params, std::string eventName)
        {
            handleVehicleInput(caller, params, eventName);
        });

    gameWrapper->RegisterDrawable(
        [this](CanvasWrapper canvas)
        {
            renderHud(canvas);
        });

    cvarManager->log("Takeoff Coach 2 loaded. Open F2 > Plugins > Takeoff Coach.");
}

void TakeoffCoach::onUnload()
{
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void TakeoffCoach::registerCvarsAndCommands()
{
    cvarManager->registerCvar("tc_car_speed", "1250",
        "Initial car speed", true, true, 0.0f, true, 2200.0f);

    cvarManager->registerCvar("tc_ball_forward", "850",
        "Ball forward speed", true, true, 0.0f, true, 2000.0f);

    cvarManager->registerCvar("tc_ball_up", "720",
        "Ball upward speed", true, true, 0.0f, true, 1600.0f);

    cvarManager->registerCvar("tc_ball_height", "260",
        "Initial ball height", true, true, 100.0f, true, 1100.0f);

    cvarManager->registerCvar("tc_start_distance", "3600",
        "Car distance behind the ball", true, true, 1800.0f, true, 6000.0f);

    cvarManager->registerCvar("tc_trigger_ratio", "0.88",
        "How late the JUMP cue appears", true, true, 0.65f, true, 0.98f);

    cvarManager->registerCvar("tc_jump_window", "0.12",
        "Reach-ratio width of the ideal jump window", true, true, 0.04f, true, 0.25f);

    cvarManager->registerCvar("tc_min_contact_time", "0.55",
        "Minimum modeled aerial duration", true, true, 0.35f, true, 1.2f);

    cvarManager->registerCvar("tc_max_contact_time", "1.85",
        "Maximum modeled aerial duration", true, true, 0.8f, true, 2.8f);

    cvarManager->registerCvar("tc_horizontal_boost_efficiency", "0.68",
        "Modeled horizontal boost efficiency", true, true, 0.25f, true, 1.0f);

    cvarManager->registerCvar("tc_vertical_boost_efficiency", "0.72",
        "Modeled vertical boost efficiency", true, true, 0.25f, true, 1.0f);

    cvarManager->registerCvar("tc_random_angle", "45",
        "Maximum setup rotation in degrees", true, true, 0.0f, true, 180.0f);

    cvarManager->registerCvar("tc_random_speed", "0.20",
        "Random speed variation", true, true, 0.0f, true, 0.5f);

    cvarManager->registerCvar("tc_random_lateral", "450",
        "Random lateral ball variation", true, true, 0.0f, true, 1200.0f);

    cvarManager->registerCvar("tc_auto_reset", "1",
        "Automatically start another drill", true, true, 0.0f, true, 1.0f);

    cvarManager->registerCvar("tc_reset_delay", "2.5",
        "Seconds before automatic reset", true, true, 0.6f, true, 8.0f);

    cvarManager->registerCvar("tc_show_hud", "1",
        "Show coaching HUD", true, true, 0.0f, true, 1.0f);

    constexpr unsigned char permission = PERMISSION_FREEPLAY;

    cvarManager->registerNotifier(
        "tc_start",
        [this](std::vector<std::string>) { startAttempt(false); },
        "Start fixed Takeoff Coach drill",
        permission);

    cvarManager->registerNotifier(
        "tc_random",
        [this](std::vector<std::string>) { startAttempt(true); },
        "Start randomized Takeoff Coach drill",
        permission);

    cvarManager->registerNotifier(
        "tc_reset",
        [this](std::vector<std::string>) { resetAttempt(); },
        "Reset Takeoff Coach drill",
        permission);
}

void TakeoffCoach::startAttempt(bool randomize)
{
    if (!gameWrapper->IsInFreeplay())
    {
        attempt_.active = false;
        attempt_.headline = "FREEPLAY REQUIRED";
        attempt_.detail = "Enter Freeplay, then press Start.";
        return;
    }

    auto server = gameWrapper->GetCurrentGameState();
    auto car = gameWrapper->GetLocalCar();
    auto ball = server.GetBall();

    if (car.IsNull() || ball.IsNull())
    {
        attempt_.active = false;
        attempt_.headline = "SETUP FAILED";
        attempt_.detail = "Could not access the local car or ball.";
        return;
    }

    float angle = 0.0f;
    float speedScale = 1.0f;
    float lateral = 0.0f;

    if (randomize)
    {
        const float maxAngle = cvarFloat("tc_random_angle") * PI / 180.0f;
        const float speedVariation = cvarFloat("tc_random_speed");
        const float lateralVariation = cvarFloat("tc_random_lateral");

        std::uniform_real_distribution<float> angleDistribution(-maxAngle, maxAngle);
        std::uniform_real_distribution<float> speedDistribution(
            1.0f - speedVariation,
            1.0f + speedVariation);
        std::uniform_real_distribution<float> lateralDistribution(
            -lateralVariation,
            lateralVariation);

        angle = angleDistribution(rng_);
        speedScale = speedDistribution(rng_);
        lateral = lateralDistribution(rng_);
    }

    const float carSpeed = cvarFloat("tc_car_speed") * speedScale;
    const float ballForward = cvarFloat("tc_ball_forward") * speedScale;
    const float ballUp = cvarFloat("tc_ball_up") * speedScale;
    const float ballHeight = cvarFloat("tc_ball_height");
    const float startDistance = cvarFloat("tc_start_distance");

    Vector forward = rotate2D(Vector{1.0f, 0.0f, 0.0f}, angle);
    Vector side{-forward.Y, forward.X, 0.0f};

    const Vector carPosition = forward * (-startDistance) + side * (-0.25f * lateral);
    const Vector ballPosition = forward * 250.0f + side * lateral
        + Vector{0.0f, 0.0f, ballHeight};

    const Vector carVelocity = forward * carSpeed;
    const Vector ballVelocity = forward * ballForward
        + side * (0.25f * lateral)
        + Vector{0.0f, 0.0f, ballUp};

    car.SetLocation(Vector{carPosition.X, carPosition.Y, CAR_Z});
    car.SetVelocity(carVelocity);
    car.SetAngularVelocity(Vector{0.0f, 0.0f, 0.0f}, false);

    const int yaw = static_cast<int>(std::atan2(forward.Y, forward.X) * 32768.0f / PI);
    car.SetRotation(Rotator{0, yaw, 0});

    ball.SetLocation(ballPosition);
    ball.SetVelocity(ballVelocity);
    ball.SetAngularVelocity(Vector{0.0f, 0.0f, 0.0f}, false);

    attempt_ = AttemptState{};
    attempt_.active = true;
    attempt_.randomized = randomize;
    attempt_.cue = Cue::Wait;
    attempt_.headline = "READ THE BALL";
    attempt_.detail = "Keep the car lined up. The cue updates from the live trajectory.";
    attempt_.startTime = std::chrono::steady_clock::now();
}

void TakeoffCoach::resetAttempt()
{
    startAttempt(attempt_.randomized);
}

void TakeoffCoach::handleVehicleInput(
    CarWrapper caller,
    void* params,
    std::string)
{
    if (!attempt_.active || params == nullptr || !gameWrapper->IsInFreeplay())
        return;

    auto localCar = gameWrapper->GetLocalCar();
    if (localCar.IsNull() || caller.memory_address != localCar.memory_address)
        return;

    updateRecommendation(caller);

    auto* input = static_cast<ControllerInput*>(params);
    const bool jumpPressed = input->Jump != 0;

    if (jumpPressed && !attempt_.previousJump)
        gradeJump(caller);

    attempt_.previousJump = jumpPressed;
}

void TakeoffCoach::updateRecommendation(CarWrapper car)
{
    auto server = gameWrapper->GetCurrentGameState();
    auto ball = server.GetBall();
    if (ball.IsNull())
        return;

    const Vector carPosition = car.GetLocation();
    const Vector carVelocity = car.GetVelocity();
    const Vector ballPosition = ball.GetLocation();
    const Vector ballVelocity = ball.GetVelocity();

    const float minTime = cvarFloat("tc_min_contact_time");
    const float maxTime = std::max(minTime + 0.1f, cvarFloat("tc_max_contact_time"));
    const float horizontalEfficiency = cvarFloat("tc_horizontal_boost_efficiency");
    const float verticalEfficiency = cvarFloat("tc_vertical_boost_efficiency");

    float bestRatio = 999.0f;
    float bestTime = minTime;
    Vector bestPoint = ballPosition;
    Vector bestDirection{1.0f, 0.0f, 0.0f};

    for (float t = minTime; t <= maxTime; t += 0.025f)
    {
        Vector predictedBall{
            ballPosition.X + ballVelocity.X * t,
            ballPosition.Y + ballVelocity.Y * t,
            ballPosition.Z + ballVelocity.Z * t + 0.5f * BALL_GRAVITY * t * t
        };

        if (predictedBall.Z < 110.0f)
            continue;

        const Vector delta = predictedBall - carPosition;
        const float horizontalNeed = length2D(delta);
        const float verticalNeed = std::max(0.0f, delta.Z - CONTACT_ALLOWANCE);

        const float planarSpeed = length2D(carVelocity);

        const float horizontalReach =
            planarSpeed * t
            + 0.5f * BOOST_ACCELERATION * horizontalEfficiency * t * t
            + CONTACT_ALLOWANCE;

        const float secondJumpTime = std::max(0.0f, t - SECOND_JUMP_DELAY);
        const float verticalReach =
            JUMP_IMPULSE * t
            + SECOND_JUMP_IMPULSE * secondJumpTime
            + 0.5f * (BOOST_ACCELERATION * verticalEfficiency + BALL_GRAVITY) * t * t
            + CONTACT_ALLOWANCE;

        if (horizontalReach <= 1.0f || verticalReach <= 1.0f)
            continue;

        const float horizontalRatio = horizontalNeed / horizontalReach;
        const float verticalRatio = verticalNeed / verticalReach;
        const float ratio = std::max(horizontalRatio, verticalRatio);

        if (ratio < bestRatio)
        {
            bestRatio = ratio;
            bestTime = t;
            bestPoint = predictedBall;
            bestDirection = normalized2D(delta);
        }
    }

    attempt_.reachRatio = bestRatio;
    attempt_.targetTime = bestTime;
    attempt_.targetPoint = bestPoint;
    attempt_.targetDirection = bestDirection;

    Vector travelDirection = normalized2D(carVelocity);
    attempt_.directionErrorDeg = angleDeg2D(travelDirection, bestDirection);

    const float trigger = cvarFloat("tc_trigger_ratio");
    const float window = cvarFloat("tc_jump_window");
    const float directionError = attempt_.directionErrorDeg;

    std::ostringstream detail;
    detail << "Contact in " << oneDecimal(bestTime) << " s"
           << " | aim error " << oneDecimal(directionError) << " deg"
           << " | reach " << oneDecimal(bestRatio * 100.0f) << "%";

    if (bestRatio > 1.0f + window)
    {
        attempt_.cue = Cue::Late;
        attempt_.headline = "TOO LATE";
        detail << " | the modeled fast-aerial envelope can no longer reach it";
    }
    else if (directionError > 13.0f)
    {
        attempt_.cue = Cue::Wait;
        attempt_.headline = "ALIGN";
        detail << " | turn toward the predicted contact point";
    }
    else if (bestRatio >= trigger)
    {
        attempt_.cue = Cue::Jump;
        attempt_.headline = "JUMP NOW";
        detail << " | commit to the fast aerial";
        attempt_.lastReachableSeconds = bestTime;
    }
    else
    {
        attempt_.cue = Cue::Wait;
        attempt_.headline = "WAIT";
        detail << " | stay grounded and keep reading the trajectory";
    }

    attempt_.detail = detail.str();
}

void TakeoffCoach::gradeJump(CarWrapper car)
{
    updateRecommendation(car);

    const float trigger = cvarFloat("tc_trigger_ratio");
    const float window = cvarFloat("tc_jump_window");

    std::ostringstream result;

    if (attempt_.directionErrorDeg > 15.0f)
    {
        result << "BAD DIRECTION";
    }
    else if (attempt_.reachRatio < trigger - window)
    {
        result << "EARLY JUMP";
    }
    else if (attempt_.reachRatio > 1.0f + window)
    {
        result << "LATE JUMP";
    }
    else if (attempt_.reachRatio >= trigger && attempt_.reachRatio <= 1.0f)
    {
        result << "EXCELLENT TAKEOFF";
    }
    else
    {
        result << "GOOD TAKEOFF";
    }

    result << " | reach " << oneDecimal(attempt_.reachRatio * 100.0f) << "%"
           << " | aim " << oneDecimal(attempt_.directionErrorDeg) << " deg"
           << " | predicted contact " << oneDecimal(attempt_.targetTime) << " s";

    attempt_.headline = result.str();
    attempt_.detail =
        "The ideal result is the latest reachable jump with a small direction error.";
    attempt_.active = false;
    attempt_.cue = Cue::Inactive;

    cvarManager->log(attempt_.headline);

    if (cvarBool("tc_auto_reset"))
    {
        const float delay = cvarFloat("tc_reset_delay");
        const bool randomize = attempt_.randomized;

        gameWrapper->SetTimeout(
            [this, randomize](GameWrapper*)
            {
                startAttempt(randomize);
            },
            delay);
    }
}

void TakeoffCoach::renderHud(CanvasWrapper canvas)
{
    if (!cvarBool("tc_show_hud") || !gameWrapper->IsInFreeplay())
        return;

    canvas.SetPosition(Vector2{40, 105});
    canvas.DrawString("TAKEOFF COACH 2", 1.7f, 1.7f, true);

    canvas.SetPosition(Vector2{40, 137});
    canvas.DrawString(attempt_.headline, 1.45f, 1.45f, true);

    canvas.SetPosition(Vector2{40, 166});
    canvas.DrawString(attempt_.detail, 1.0f, 1.0f, false);
}

void TakeoffCoach::RenderSettings()
{
    ImGui::TextWrapped(
        "Trains the latest viable fast-aerial takeoff. "
        "The cue continuously recomputes a predicted contact from the live ball position and velocity.");

    ImGui::Separator();

    if (ImGui::Button("Start fixed drill"))
        cvarManager->executeCommand("tc_start");

    ImGui::SameLine();

    if (ImGui::Button("Start randomized drill"))
        cvarManager->executeCommand("tc_random");

    ImGui::SameLine();

    if (ImGui::Button("Reset"))
        cvarManager->executeCommand("tc_reset");

    ImGui::Separator();
    ImGui::Text("Setup");

    float carSpeed = cvarFloat("tc_car_speed");
    if (ImGui::SliderFloat("Initial car speed", &carSpeed, 0.0f, 2200.0f, "%.0f uu/s"))
        setCvar("tc_car_speed", carSpeed);

    float ballForward = cvarFloat("tc_ball_forward");
    if (ImGui::SliderFloat("Ball forward speed", &ballForward, 0.0f, 2000.0f, "%.0f uu/s"))
        setCvar("tc_ball_forward", ballForward);

    float ballUp = cvarFloat("tc_ball_up");
    if (ImGui::SliderFloat("Ball upward speed", &ballUp, 0.0f, 1600.0f, "%.0f uu/s"))
        setCvar("tc_ball_up", ballUp);

    float ballHeight = cvarFloat("tc_ball_height");
    if (ImGui::SliderFloat("Initial ball height", &ballHeight, 100.0f, 1100.0f, "%.0f uu"))
        setCvar("tc_ball_height", ballHeight);

    float startDistance = cvarFloat("tc_start_distance");
    if (ImGui::SliderFloat("Approach distance", &startDistance, 1800.0f, 6000.0f, "%.0f uu"))
        setCvar("tc_start_distance", startDistance);

    ImGui::Separator();
    ImGui::Text("Timing model");

    float trigger = cvarFloat("tc_trigger_ratio");
    if (ImGui::SliderFloat("How late to wait", &trigger, 0.65f, 0.98f, "%.2f"))
        setCvar("tc_trigger_ratio", trigger);

    float window = cvarFloat("tc_jump_window");
    if (ImGui::SliderFloat("Jump-window tolerance", &window, 0.04f, 0.25f, "%.2f"))
        setCvar("tc_jump_window", window);

    float minContact = cvarFloat("tc_min_contact_time");
    if (ImGui::SliderFloat("Minimum aerial time", &minContact, 0.35f, 1.2f, "%.2f s"))
        setCvar("tc_min_contact_time", minContact);

    float maxContact = cvarFloat("tc_max_contact_time");
    if (ImGui::SliderFloat("Maximum aerial time", &maxContact, 0.8f, 2.8f, "%.2f s"))
        setCvar("tc_max_contact_time", maxContact);

    ImGui::TextWrapped(
        "Higher 'How late to wait' values demand a later, tighter takeoff. "
        "Lower it if the cue regularly becomes impossible before you can react.");

    ImGui::Separator();
    ImGui::Text("Randomization");

    float randomAngle = cvarFloat("tc_random_angle");
    if (ImGui::SliderFloat("Angle range", &randomAngle, 0.0f, 180.0f, "%.0f deg"))
        setCvar("tc_random_angle", randomAngle);

    float randomSpeed = cvarFloat("tc_random_speed");
    if (ImGui::SliderFloat("Speed variation", &randomSpeed, 0.0f, 0.5f, "%.2f"))
        setCvar("tc_random_speed", randomSpeed);

    float randomLateral = cvarFloat("tc_random_lateral");
    if (ImGui::SliderFloat("Lateral variation", &randomLateral, 0.0f, 1200.0f, "%.0f uu"))
        setCvar("tc_random_lateral", randomLateral);

    ImGui::Separator();

    bool autoReset = cvarBool("tc_auto_reset");
    if (ImGui::Checkbox("Automatically start the next drill", &autoReset))
        setCvar("tc_auto_reset", autoReset);

    bool showHud = cvarBool("tc_show_hud");
    if (ImGui::Checkbox("Show coaching HUD", &showHud))
        setCvar("tc_show_hud", showHud);

    float resetDelay = cvarFloat("tc_reset_delay");
    if (ImGui::SliderFloat("Reset delay", &resetDelay, 0.6f, 8.0f, "%.1f s"))
        setCvar("tc_reset_delay", resetDelay);
}

std::string TakeoffCoach::GetPluginName()
{
    return "Takeoff Coach";
}

void TakeoffCoach::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

float TakeoffCoach::cvarFloat(const std::string& name) const
{
    return cvarManager->getCvar(name).getFloatValue();
}

bool TakeoffCoach::cvarBool(const std::string& name) const
{
    return cvarManager->getCvar(name).getBoolValue();
}

void TakeoffCoach::setCvar(const std::string& name, float value)
{
    cvarManager->getCvar(name).setValue(value);
}

void TakeoffCoach::setCvar(const std::string& name, bool value)
{
    cvarManager->getCvar(name).setValue(value ? 1 : 0);
}

float TakeoffCoach::clamp(float value, float low, float high)
{
    return std::max(low, std::min(value, high));
}

float TakeoffCoach::dot2D(const Vector& a, const Vector& b)
{
    return a.X * b.X + a.Y * b.Y;
}

float TakeoffCoach::length2D(const Vector& v)
{
    return std::sqrt(v.X * v.X + v.Y * v.Y);
}

Vector TakeoffCoach::normalized2D(const Vector& v)
{
    const float length = length2D(v);
    if (length < 0.001f)
        return Vector{1.0f, 0.0f, 0.0f};

    return Vector{v.X / length, v.Y / length, 0.0f};
}

Vector TakeoffCoach::rotate2D(const Vector& v, float radians)
{
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    return Vector{
        cosine * v.X - sine * v.Y,
        sine * v.X + cosine * v.Y,
        v.Z
    };
}

float TakeoffCoach::angleDeg2D(const Vector& a, const Vector& b)
{
    const Vector normalizedA = normalized2D(a);
    const Vector normalizedB = normalized2D(b);
    const float cosine = clamp(dot2D(normalizedA, normalizedB), -1.0f, 1.0f);
    return std::acos(cosine) * 180.0f / PI;
}

std::string TakeoffCoach::oneDecimal(float value)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(1) << value;
    return output.str();
}

