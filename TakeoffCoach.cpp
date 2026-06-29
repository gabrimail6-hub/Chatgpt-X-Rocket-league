#include "TakeoffCoach.h"

#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

BAKKESMOD_PLUGIN(
    TakeoffCoach,
    "Takeoff Coach",
    "0.1.0",
    PLUGINTYPE_FREEPLAY
)

namespace
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float UU_PER_METER = 100.0f;

    // Baseline drill geometry. The whole setup is rotated when random mode is used.
    const Vector BASE_CAR_START{0.0f, -3300.0f, 17.0f};
    const Vector BASE_TAKEOFF{0.0f, -950.0f, 17.0f};
    const Vector BASE_BALL_START{0.0f, 150.0f, 760.0f};
    const Vector BASE_BALL_VELOCITY{0.0f, 520.0f, 650.0f};

    constexpr float EARLY_LATE_GOOD = 120.0f;
    constexpr float EARLY_LATE_OK = 260.0f;
    constexpr float LATERAL_GOOD = 90.0f;
    constexpr float SPEED_GOOD = 140.0f;
    constexpr float SPEED_OK = 300.0f;
}

void TakeoffCoach::onLoad()
{
    registerCommands();

    gameWrapper->HookEventWithCaller<CarWrapper>(
        "Function TAGame.Car_TA.SetVehicleInput",
        [this](CarWrapper caller, void* params, std::string eventName)
        {
            handleVehicleInput(caller, params, eventName);
        });

    gameWrapper->RegisterDrawable(
        [this](CanvasWrapper canvas)
        {
            render(canvas);
        });

    cvarManager->log("Takeoff Coach loaded. Enter Freeplay and run: tc_start");
}

void TakeoffCoach::onUnload()
{
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void TakeoffCoach::registerCommands()
{
    constexpr unsigned char permission = PERMISSION_FREEPLAY;

    cvarManager->registerCvar(
        "tc_target_speed",
        "1350",
        "Target ground speed at takeoff in uu/s",
        true, true, 400.0f, true, 2300.0f);

    cvarManager->registerCvar(
        "tc_auto_reset",
        "1",
        "Automatically reset after grading",
        true, true, 0.0f, true, 1.0f);

    cvarManager->registerCvar(
        "tc_reset_delay",
        "2.2",
        "Seconds before automatic reset",
        true, true, 0.4f, true, 8.0f);

    cvarManager->registerNotifier(
        "tc_start",
        [this](std::vector<std::string>)
        {
            startAttempt(false);
        },
        "Start the fixed Takeoff Coach drill",
        permission);

    cvarManager->registerNotifier(
        "tc_random",
        [this](std::vector<std::string>)
        {
            startAttempt(true);
        },
        "Start a rotated and slightly varied drill",
        permission);

    cvarManager->registerNotifier(
        "tc_reset",
        [this](std::vector<std::string>)
        {
            resetAttempt();
        },
        "Reset the current attempt",
        permission);
}

void TakeoffCoach::startAttempt(bool randomize)
{
    if (!gameWrapper->IsInFreeplay())
    {
        attempt_.feedback = "Takeoff Coach only works in Freeplay";
        cvarManager->log(attempt_.feedback);
        return;
    }

    auto server = gameWrapper->GetCurrentGameState();
    auto car = gameWrapper->GetLocalCar();
    auto ball = server.GetBall();

    if (car.IsNull() || ball.IsNull())
    {
        attempt_.feedback = "Could not obtain the local car or ball";
        cvarManager->log(attempt_.feedback);
        return;
    }

    float angle = 0.0f;
    float distanceOffset = 0.0f;
    float lateralOffset = 0.0f;
    float heightOffset = 0.0f;
    float speedScale = 1.0f;

    if (randomize)
    {
        std::uniform_real_distribution<float> angleDist(-0.75f, 0.75f);
        std::uniform_real_distribution<float> distanceDist(-350.0f, 350.0f);
        std::uniform_real_distribution<float> lateralDist(-180.0f, 180.0f);
        std::uniform_real_distribution<float> heightDist(-100.0f, 180.0f);
        std::uniform_real_distribution<float> speedDist(0.88f, 1.12f);

        angle = angleDist(rng_);
        distanceOffset = distanceDist(rng_);
        lateralOffset = lateralDist(rng_);
        heightOffset = heightDist(rng_);
        speedScale = speedDist(rng_);
    }

    Vector carStart = BASE_CAR_START;
    carStart.Y -= distanceOffset;
    carStart.X += lateralOffset;

    Vector takeoff = BASE_TAKEOFF;
    Vector ballStart = BASE_BALL_START;
    ballStart.Z += heightOffset;

    carStart = rotate2D(carStart, angle);
    takeoff = rotate2D(takeoff, angle);
    ballStart = rotate2D(ballStart, angle);
    Vector ballVelocity = rotate2D(BASE_BALL_VELOCITY, angle);
    ballVelocity.X *= speedScale;
    ballVelocity.Y *= speedScale;
    ballVelocity.Z *= speedScale;

    const Vector approach = normalized2D(Vector{
        takeoff.X - carStart.X,
        takeoff.Y - carStart.Y,
        0.0f
    });

    car.SetLocation(carStart);
    car.SetVelocity(Vector{0.0f, 0.0f, 0.0f});
    car.SetAngularVelocity(Vector{0.0f, 0.0f, 0.0f}, false);

    // Unreal/Rocket League yaw uses 65536 units for one full turn.
    const float yawRadians = std::atan2(approach.Y, approach.X);
    const int yawUnits = static_cast<int>(yawRadians * 32768.0f / PI);
    car.SetRotation(Rotator{0, yawUnits, 0});

    ball.SetLocation(ballStart);
    ball.SetVelocity(ballVelocity);
    ball.SetAngularVelocity(Vector{0.0f, 0.0f, 0.0f}, false);

    attempt_ = AttemptState{};
    attempt_.active = true;
    attempt_.carStart = carStart;
    attempt_.takeoffPoint = takeoff;
    attempt_.approachDirection = approach;
    attempt_.ballStart = ballStart;
    attempt_.ballVelocity = ballVelocity;
    attempt_.targetSpeed =
        cvarManager->getCvar("tc_target_speed").getFloatValue();
    attempt_.feedback = "Approach the marker, control speed, then jump";
    attempt_.startTime = std::chrono::steady_clock::now();
}

void TakeoffCoach::resetAttempt()
{
    startAttempt(false);
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

    auto* input = static_cast<ControllerInput*>(params);
    const float now = elapsedSeconds();

    const bool currentlyBraking = input->Throttle < -0.10f;
    if (currentlyBraking && !attempt_.braking)
    {
        attempt_.braking = true;
        attempt_.hasBraked = true;
        attempt_.brakeStartSeconds = now;
    }
    else if (!currentlyBraking && attempt_.braking)
    {
        attempt_.braking = false;
        attempt_.accumulatedBrakeSeconds += now - attempt_.brakeStartSeconds;
    }

    const bool jumpPressed = input->Jump != 0;
    if (jumpPressed && !attempt_.previousJump)
    {
        if (attempt_.braking)
        {
            attempt_.accumulatedBrakeSeconds += now - attempt_.brakeStartSeconds;
            attempt_.braking = false;
        }
        gradeTakeoff(caller);
    }

    attempt_.previousJump = jumpPressed;
}

void TakeoffCoach::gradeTakeoff(CarWrapper car)
{
    const Vector location = car.GetLocation();
    const Vector velocity = car.GetVelocity();

    const Vector error{
        location.X - attempt_.takeoffPoint.X,
        location.Y - attempt_.takeoffPoint.Y,
        0.0f
    };

    // Positive means the player jumped beyond the target point (late).
    const float alongError = dot2D(error, attempt_.approachDirection);
    const Vector lateralAxis{
        -attempt_.approachDirection.Y,
        attempt_.approachDirection.X,
        0.0f
    };
    const float lateralError = dot2D(error, lateralAxis);
    const float forwardSpeed = dot2D(velocity, attempt_.approachDirection);
    const float speedError = forwardSpeed - attempt_.targetSpeed;

    std::ostringstream message;
    bool majorProblem = false;

    if (alongError < -EARLY_LATE_OK)
    {
        message << "TOO EARLY";
        majorProblem = true;
    }
    else if (alongError > EARLY_LATE_OK)
    {
        message << "TOO LATE";
        majorProblem = true;
    }
    else if (speedError > SPEED_OK)
    {
        message << "TOO FAST";
        majorProblem = true;
    }
    else if (speedError < -SPEED_OK)
    {
        message << "OVER-BRAKED / TOO SLOW";
        majorProblem = true;
    }
    else if (std::abs(lateralError) > 180.0f)
    {
        message << "BAD TAKEOFF LINE";
        majorProblem = true;
    }
    else
    {
        message << "GOOD TAKEOFF";
    }

    message << " | position " << formatSigned(alongError / UU_PER_METER, 1)
            << " m (+: late)"
            << " | speed " << std::fixed << std::setprecision(0)
            << forwardSpeed << " / " << attempt_.targetSpeed
            << " | lateral " << formatSigned(lateralError / UU_PER_METER, 1)
            << " m"
            << " | brake " << std::setprecision(2)
            << attempt_.accumulatedBrakeSeconds << " s";

    if (!attempt_.hasBraked && speedError > SPEED_GOOD)
        message << " | try releasing throttle or braking sooner";
    else if (attempt_.accumulatedBrakeSeconds > 0.65f && speedError < -SPEED_GOOD)
        message << " | shorten the braking phase";
    else if (alongError < -EARLY_LATE_GOOD)
        message << " | hold the ground approach longer";
    else if (alongError > EARLY_LATE_GOOD)
        message << " | initiate the jump sooner";

    attempt_.feedback = message.str();
    attempt_.active = false;
    cvarManager->log(attempt_.feedback);

    const bool autoReset =
        cvarManager->getCvar("tc_auto_reset").getBoolValue();

    if (autoReset)
    {
        const float delay =
            cvarManager->getCvar("tc_reset_delay").getFloatValue();

        gameWrapper->SetTimeout(
            [this](GameWrapper*)
            {
                startAttempt(true);
            },
            delay);
    }
}

void TakeoffCoach::render(CanvasWrapper canvas)
{
    if (!gameWrapper->IsInFreeplay())
        return;

    canvas.SetPosition(Vector2{40, 110});
    canvas.DrawString(
        "TAKEOFF COACH",
        2.0f,
        2.0f,
        true);

    canvas.SetPosition(Vector2{40, 145});
    canvas.DrawString(
        attempt_.feedback,
        1.35f,
        1.35f,
        false);

    if (attempt_.active)
    {
        auto car = gameWrapper->GetLocalCar();
        if (!car.IsNull())
        {
            const Vector location = car.GetLocation();
            const Vector relative{
                location.X - attempt_.takeoffPoint.X,
                location.Y - attempt_.takeoffPoint.Y,
                0.0f
            };
            const float along =
                dot2D(relative, attempt_.approachDirection) / UU_PER_METER;
            const float speed =
                dot2D(car.GetVelocity(), attempt_.approachDirection);

            std::ostringstream live;
            live << "Target zone: " << formatSigned(along, 1)
                 << " m (0 = jump)"
                 << "   Speed: " << std::fixed << std::setprecision(0)
                 << speed << " / " << attempt_.targetSpeed;

            canvas.SetPosition(Vector2{40, 172});
            canvas.DrawString(live.str(), 1.1f, 1.1f, false);
        }
    }
}

float TakeoffCoach::elapsedSeconds() const
{
    if (attempt_.startTime.time_since_epoch().count() == 0)
        return 0.0f;

    const auto duration =
        std::chrono::steady_clock::now() - attempt_.startTime;
    return std::chrono::duration<float>(duration).count();
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

std::string TakeoffCoach::formatSigned(float value, int precision)
{
    std::ostringstream output;
    output << std::showpos << std::fixed << std::setprecision(precision)
           << value;
    return output.str();
}
