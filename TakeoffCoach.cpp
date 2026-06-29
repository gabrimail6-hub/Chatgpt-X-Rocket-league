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
    "3.2.0",
    PLUGINTYPE_FREEPLAY
)

namespace
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float DEG_TO_RAD = PI / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / PI;

    constexpr float GRAVITY_Z = -650.0f;
    constexpr float BOOST_ACCEL = 991.7f;
    constexpr float FIRST_JUMP = 292.0f;
    constexpr float SECOND_JUMP = 292.0f;

    constexpr float SAFE_X = 3500.0f;
    constexpr float SAFE_Y = 4500.0f;
    constexpr float SAFE_FLOOR = 105.0f;
    constexpr float SAFE_CEILING = 1800.0f;

    void setColor(CanvasWrapper& canvas, int r, int g, int b, int a)
    {
        canvas.SetColor(
            static_cast<char>(r),
            static_cast<char>(g),
            static_cast<char>(b),
            static_cast<char>(a));
    }
}

void TakeoffCoach::onLoad()
{
    registerCvars();

    gameWrapper->HookEventWithCaller<CarWrapper>(
        "Function TAGame.Car_TA.SetVehicleInput",
        [this](CarWrapper car, void* params, std::string eventName)
        {
            onVehicleInput(car, params, eventName);
        });

    gameWrapper->RegisterDrawable(
        [this](CanvasWrapper canvas)
        {
            renderHud(canvas);
        });

    cvarManager->log("Takeoff Coach 3.1 loaded.");
}

void TakeoffCoach::onUnload()
{
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void TakeoffCoach::registerCvars()
{
    auto reg = [this](const std::string& name, const std::string& value, float low, float high)
    {
        cvarManager->registerCvar(name, value, "", true, true, low, true, high);
    };

    reg("tc_objective", "2", 0.0f, 3.0f);
    reg("tc_auto_reset", "1", 0.0f, 1.0f);
    reg("tc_feedback_seconds", "2.5", 0.5f, 8.0f);

    reg("tc_distance_min", "2600", 1200.0f, 6000.0f);
    reg("tc_distance_max", "4300", 1200.0f, 6000.0f);
    reg("tc_lateral_min", "-900", -2200.0f, 2200.0f);
    reg("tc_lateral_max", "900", -2200.0f, 2200.0f);
    reg("tc_ball_height_min", "220", 120.0f, 1400.0f);
    reg("tc_ball_height_max", "700", 120.0f, 1400.0f);
    reg("tc_setup_rotation_min", "-140", -180.0f, 180.0f);
    reg("tc_setup_rotation_max", "140", -180.0f, 180.0f);
    reg("tc_car_facing_min", "-15", -90.0f, 90.0f);
    reg("tc_car_facing_max", "15", -90.0f, 90.0f);

    reg("tc_car_speed_min", "700", 0.0f, 2300.0f);
    reg("tc_car_speed_max", "1800", 0.0f, 2300.0f);
    reg("tc_car_velocity_angle_min", "-10", -120.0f, 120.0f);
    reg("tc_car_velocity_angle_max", "10", -120.0f, 120.0f);

    reg("tc_ball_speed_min", "350", 0.0f, 2200.0f);
    reg("tc_ball_speed_max", "1500", 0.0f, 2200.0f);
    reg("tc_ball_vertical_min", "250", -500.0f, 1600.0f);
    reg("tc_ball_vertical_max", "1000", -500.0f, 1600.0f);
    reg("tc_ball_direction_min", "-180", -180.0f, 180.0f);
    reg("tc_ball_direction_max", "180", -180.0f, 180.0f);

    reg("tc_contact_min", "0.55", 0.25f, 1.5f);
    reg("tc_contact_max", "2.35", 0.8f, 3.5f);
    reg("tc_solver_hz", "20", 5.0f, 60.0f);
    reg("tc_horizontal_calibration", "0.76", 0.4f, 1.2f);
    reg("tc_vertical_calibration", "0.78", 0.4f, 1.2f);
    reg("tc_position_tolerance", "150", 80.0f, 300.0f);
    reg("tc_control_target", "700", 200.0f, 1600.0f);

    // Desired ball-centre height at contact. Standard Soccar crossbar height
    // is approximately 642.775 uu, so the default band surrounds it.
    reg("tc_target_height_min", "600", 120.0f, 1700.0f);
    reg("tc_target_height_max", "685", 120.0f, 1700.0f);
    reg("tc_height_yellow_margin", "180", 20.0f, 600.0f);
    reg("tc_height_display_margin", "500", 100.0f, 1200.0f);


    reg("tc_show_hud", "1", 0.0f, 1.0f);
    reg("tc_show_numbers", "1", 0.0f, 1.0f);
    reg("tc_show_timing", "1", 0.0f, 1.0f);
    reg("tc_show_alignment", "1", 0.0f, 1.0f);
    reg("tc_show_height", "1", 0.0f, 1.0f);
    reg("tc_hud_position", "0", 0.0f, 2.0f);
    reg("tc_hud_opacity", "0.88", 0.2f, 1.0f);
    reg("tc_indicator_text_scale", "1.05", 0.80f, 1.50f);

    reg("tc_timing_green_early_ms", "70", 10.0f, 250.0f);
    reg("tc_timing_green_late_ms", "55", 10.0f, 250.0f);
    reg("tc_timing_yellow_early_ms", "260", 40.0f, 800.0f);
    reg("tc_timing_yellow_late_ms", "190", 40.0f, 800.0f);
    reg("tc_timing_display_early_ms", "900", 150.0f, 2000.0f);
    reg("tc_timing_display_late_ms", "650", 150.0f, 2000.0f);

    reg("tc_green_alignment", "4", 1.0f, 15.0f);
    reg("tc_yellow_alignment", "11", 4.0f, 30.0f);
    reg("tc_alignment_display", "35", 10.0f, 90.0f);

    constexpr unsigned char permission = PERMISSION_FREEPLAY;

    cvarManager->registerNotifier(
        "tc_new",
        [this](std::vector<std::string>) { requestNewScenario(); },
        "Generate a new Takeoff Coach scenario",
        permission);

    cvarManager->registerNotifier(
        "tc_stop",
        [this](std::vector<std::string>)
        {
            ++attempt_.generation;
            attempt_ = Attempt{};
            attempt_.headline = "Drill stopped.";
        },
        "Stop Takeoff Coach",
        permission);
}

void TakeoffCoach::requestNewScenario()
{
    const uint64_t generation = ++attempt_.generation;

    gameWrapper->SetTimeout(
        [this, generation](GameWrapper*)
        {
            startScenarioNow(generation);
        },
        0.05f);
}

void TakeoffCoach::startScenarioNow(uint64_t generation)
{
    if (generation != attempt_.generation)
        return;

    if (!gameWrapper->IsInFreeplay())
    {
        attempt_.phase = Phase::Idle;
        attempt_.headline = "Enter Freeplay first.";
        return;
    }

    Scenario scenario;
    bool success = false;

    for (int i = 0; i < 50 && !success; ++i)
    {
        success = generateScenario(scenario);
    }

    if (!success || !applyScenario(scenario))
    {
        attempt_.phase = Phase::Idle;
        attempt_.headline = "Could not generate a safe scenario.";
        return;
    }

    attempt_.phase = Phase::Reading;
    attempt_.objective = scenario.objective;
    attempt_.scenario = scenario;
    attempt_.solution = Solution{};
    attempt_.jumpSolution = Solution{};
    attempt_.previousJump = false;
    attempt_.touched = false;
    attempt_.scored = false;
    attempt_.startedAt = nowSeconds();
    attempt_.jumpAt = 0.0f;
    attempt_.feedbackUntil = 0.0f;
    attempt_.lastSolveAt = -100.0f;
    attempt_.closestDistance = 99999.0f;
    attempt_.timingErrorMs = 0.0f;
    attempt_.alignmentErrorDeg = 0.0f;
    attempt_.headline = objectiveName(attempt_.objective) + " | READ";
    attempt_.detail.clear();
    attempt_.correction.clear();

    auto server = gameWrapper->GetCurrentGameState();
    if (!server.IsNull())
    {
        auto ball = server.GetBall();
        if (!ball.IsNull())
            attempt_.previousBallSpeed = length(ball.GetVelocity());
    }
}

bool TakeoffCoach::generateScenario(Scenario& scenario)
{
    const float setupAngle = sample("tc_setup_rotation_min", "tc_setup_rotation_max") * DEG_TO_RAD;
    const Vector forward = rotate2D(Vector{1.0f, 0.0f, 0.0f}, setupAngle);
    const Vector side{-forward.Y, forward.X, 0.0f};

    const float distance = sample("tc_distance_min", "tc_distance_max");
    const float lateral = sample("tc_lateral_min", "tc_lateral_max");
    const float ballHeight = sample("tc_ball_height_min", "tc_ball_height_max");

    const float facingOffset = sample("tc_car_facing_min", "tc_car_facing_max") * DEG_TO_RAD;
    const Vector facing = rotate2D(forward, facingOffset);

    const float carSpeed = sample("tc_car_speed_min", "tc_car_speed_max");
    const float carVelocityAngle = sample("tc_car_velocity_angle_min", "tc_car_velocity_angle_max") * DEG_TO_RAD;
    const Vector carVelocityDirection = rotate2D(facing, carVelocityAngle);

    const float ballSpeed = sample("tc_ball_speed_min", "tc_ball_speed_max");
    const float ballVertical = sample("tc_ball_vertical_min", "tc_ball_vertical_max");
    const float ballDirection = sample("tc_ball_direction_min", "tc_ball_direction_max") * DEG_TO_RAD;
    const Vector ballHorizontalDirection = rotate2D(forward, ballDirection);

    scenario.carPosition = forward * (-distance) + side * (-0.2f * lateral);
    scenario.carPosition.Z = 17.0f;
    scenario.carVelocity = carVelocityDirection * carSpeed;

    scenario.ballPosition = forward * 200.0f + side * lateral;
    scenario.ballPosition.Z = ballHeight;
    scenario.ballVelocity = ballHorizontalDirection * ballSpeed + Vector{0.0f, 0.0f, ballVertical};

    scenario.carRotation = Rotator{
        0,
        static_cast<int>(std::atan2(facing.Y, facing.X) * 32768.0f / PI),
        0};

    scenario.objective = chooseObjective();
    scenario.goalSign = scenario.ballPosition.Y >= 0.0f ? 1 : -1;

    return isScenarioSafe(scenario);
}

bool TakeoffCoach::isScenarioSafe(Scenario scenario) const
{
    if (std::abs(scenario.carPosition.X) > SAFE_X || std::abs(scenario.carPosition.Y) > SAFE_Y)
        return false;

    if (std::abs(scenario.ballPosition.X) > SAFE_X || std::abs(scenario.ballPosition.Y) > SAFE_Y)
        return false;

    if (scenario.ballPosition.Z < SAFE_FLOOR || scenario.ballPosition.Z > SAFE_CEILING)
        return false;

    Vector position = scenario.ballPosition;
    Vector velocity = scenario.ballVelocity;

    for (float t = 0.0f; t < getFloat("tc_contact_max"); t += 1.0f / 60.0f)
    {
        velocity.Z += GRAVITY_Z / 60.0f;
        position = position + velocity * (1.0f / 60.0f);

        if (std::abs(position.X) > SAFE_X
            || std::abs(position.Y) > SAFE_Y
            || position.Z > SAFE_CEILING)
        {
            return false;
        }

        if (position.Z < SAFE_FLOOR)
            break;
    }

    return true;
}

bool TakeoffCoach::applyScenario(Scenario scenario)
{
    auto server = gameWrapper->GetCurrentGameState();
    auto car = gameWrapper->GetLocalCar();

    if (server.IsNull() || car.IsNull())
        return false;

    auto ball = server.GetBall();
    if (ball.IsNull())
        return false;

    car.SetLocation(scenario.carPosition);
    car.SetRotation(scenario.carRotation);
    car.SetVelocity(scenario.carVelocity);
    car.SetAngularVelocity(Vector{0.0f, 0.0f, 0.0f}, false);

    ball.SetLocation(scenario.ballPosition);
    ball.SetVelocity(scenario.ballVelocity);

    return true;
}

void TakeoffCoach::onVehicleInput(CarWrapper car, void* params, std::string)
{
    if (params == nullptr || !gameWrapper->IsInFreeplay())
        return;

    auto localCar = gameWrapper->GetLocalCar();
    if (localCar.IsNull() || car.memory_address != localCar.memory_address)
        return;

    const float now = nowSeconds();
    updateAttempt(car, now);

    auto* input = static_cast<ControllerInput*>(params);
    const bool jumpPressed = input->Jump != 0;

    if (jumpPressed && !attempt_.previousJump && attempt_.phase == Phase::Reading)
    {
        attempt_.jumpAt = now;
        attempt_.jumpSolution = attempt_.solution;
        attempt_.phase = Phase::Airborne;

        if (attempt_.jumpSolution.valid)
        {
            attempt_.timingErrorMs =
                (now - attempt_.jumpSolution.idealJumpAbsolute) * 1000.0f;
            attempt_.alignmentErrorDeg = attempt_.jumpSolution.alignmentErrorDeg;
        }
        else
        {
            attempt_.timingErrorMs = 9999.0f;
            attempt_.alignmentErrorDeg = 9999.0f;
        }

        attempt_.headline = "TRACKING AERIAL";
    }

    attempt_.previousJump = jumpPressed;
}

void TakeoffCoach::updateAttempt(CarWrapper car, float now)
{
    if (attempt_.phase == Phase::Idle)
        return;

    auto server = gameWrapper->GetCurrentGameState();
    if (server.IsNull())
        return;

    auto ball = server.GetBall();
    if (ball.IsNull())
        return;

    if (attempt_.phase == Phase::Reading)
        updateReading(car, ball, now);
    else if (attempt_.phase == Phase::Airborne)
        updateAirborne(car, ball, now);
    else if (attempt_.phase == Phase::Feedback && now >= attempt_.feedbackUntil)
    {
        if (getBool("tc_auto_reset"))
            requestNewScenario();
        else
            attempt_.phase = Phase::Idle;
    }
}

void TakeoffCoach::updateReading(CarWrapper car, BallWrapper ball, float now)
{
    const float hz = std::max(5.0f, getFloat("tc_solver_hz"));

    if (now - attempt_.lastSolveAt >= 1.0f / hz)
    {
        attempt_.solution = solve(car, ball, now);
        attempt_.lastSolveAt = now;
    }

    const float distanceNow = length(ball.GetLocation() - car.GetLocation());
    attempt_.closestDistance = std::min(attempt_.closestDistance, distanceNow);

    if (!attempt_.solution.valid)
    {
        attempt_.headline = objectiveName(attempt_.objective) + " | NO INTERCEPT AT TARGET HEIGHT";
        return;
    }

    const float greenEarly = getFloat("tc_timing_green_early_ms") / 1000.0f;
    const float greenLate = getFloat("tc_timing_green_late_ms") / 1000.0f;

    if (attempt_.solution.jumpDelay > greenEarly)
        attempt_.headline = objectiveName(attempt_.objective) + " | WAIT";
    else if (attempt_.solution.jumpDelay >= -greenLate)
        attempt_.headline = objectiveName(attempt_.objective) + " | JUMP NOW";
    else
        attempt_.headline = objectiveName(attempt_.objective) + " | LATE";
}

void TakeoffCoach::updateAirborne(CarWrapper car, BallWrapper ball, float now)
{
    const Vector carPosition = car.GetLocation();
    const Vector ballPosition = ball.GetLocation();
    const float distanceNow = length(ballPosition - carPosition);
    attempt_.closestDistance = std::min(attempt_.closestDistance, distanceNow);

    const float ballSpeed = length(ball.GetVelocity());
    const bool nearBall = distanceNow < 220.0f;
    const bool ballChanged = std::abs(ballSpeed - attempt_.previousBallSpeed) > 120.0f;
    attempt_.previousBallSpeed = ballSpeed;

    const bool touched = nearBall && ballChanged;
    const bool scored =
        std::abs(ballPosition.Y) > 5120.0f
        && std::abs(ballPosition.X) < 900.0f
        && ballPosition.Z < 650.0f;

    if (touched || scored)
    {
        finishAttempt(car, ball, true, scored, now);
        return;
    }

    const float deadline =
        attempt_.jumpSolution.valid
        ? attempt_.jumpSolution.idealJumpAbsolute + attempt_.jumpSolution.contactDelay + 0.8f
        : attempt_.jumpAt + 2.8f;

    if (now > deadline)
        finishAttempt(car, ball, false, false, now);
}

void TakeoffCoach::finishAttempt(
    CarWrapper car,
    BallWrapper ball,
    bool touched,
    bool scored,
    float now)
{
    attempt_.touched = touched;
    attempt_.scored = scored;
    attempt_.contactHeight = ball.GetLocation().Z;
    attempt_.phase = Phase::Feedback;
    attempt_.feedbackUntil = now + getFloat("tc_feedback_seconds");

    const float timing = attempt_.timingErrorMs;
    const float greenEarly = getFloat("tc_timing_green_early_ms");
    const float greenLate = getFloat("tc_timing_green_late_ms");
    const float greenAlignment = getFloat("tc_green_alignment");
    const float alignment = std::abs(attempt_.alignmentErrorDeg);

    if (timing < -greenEarly)
    {
        attempt_.headline = "EARLY BY " + format0(-timing) + " ms";
        attempt_.correction = "Stay grounded longer before the first jump.";
    }
    else if (timing > greenLate && timing < 9000.0f)
    {
        attempt_.headline = "LATE BY " + format0(timing) + " ms";
        attempt_.correction = "Start the fast aerial earlier.";
    }
    else if (alignment > greenAlignment && alignment < 9000.0f)
    {
        attempt_.headline = "TIMING GOOD, ALIGNMENT OFF";
        attempt_.correction =
            "Correct the takeoff direction by about "
            + format1(alignment)
            + " degrees.";
    }
    else if (!touched)
    {
        attempt_.headline = "GOOD TAKEOFF, MISSED BALL";
        attempt_.correction = "The takeoff was usable; refine the aerial after jumping.";
    }
    else if (attempt_.objective == Objective::Score)
    {
        attempt_.headline = scored ? "GOAL" : "TOUCH, NO GOAL";
        attempt_.correction =
            scored ? "Good timing and contact side."
                   : "Contact farther behind the ball toward goal.";
    }
    else if (attempt_.objective == Objective::Fast)
    {
        attempt_.headline = "FAST TOUCH COMPLETE";
        attempt_.correction =
            "Car speed at contact: "
            + format0(length(car.GetVelocity()))
            + " uu/s.";
    }
    else
    {
        const float relativeSpeed = length(car.GetVelocity() - ball.GetVelocity());
        attempt_.headline =
            relativeSpeed <= getFloat("tc_control_target")
            ? "CONTROL TOUCH COMPLETE"
            : "TOUCH TOO HARD";

        attempt_.correction =
            "Relative contact speed: "
            + format0(relativeSpeed)
            + " uu/s.";
    }

    std::ostringstream detail;
    detail << "Timing ";

    if (timing < 9000.0f)
        detail << (timing >= 0.0f ? "+" : "") << format0(timing) << " ms";
    else
        detail << "n/a";

    detail << " | Aim ";
    if (alignment < 9000.0f)
        detail << format1(attempt_.alignmentErrorDeg) << " deg";
    else
        detail << "n/a";

    detail << " | Closest " << format0(attempt_.closestDistance) << " uu";
    attempt_.detail = detail.str();
}

TakeoffCoach::Solution TakeoffCoach::solve(
    CarWrapper car,
    BallWrapper ball,
    float now) const
{
    Solution best;
    const Vector carPosition = car.GetLocation();
    const Vector carVelocity = car.GetVelocity();
    const Vector ballPosition = ball.GetLocation();
    const Vector ballVelocity = ball.GetVelocity();

    const float carSpeed = length2D(carVelocity);
    const Vector travelDirection =
        carSpeed > 80.0f
        ? normalized2D(carVelocity)
        : normalized2D(forwardFromRotator(car.GetRotation()));

    const float minContact = getFloat("tc_contact_min");
    const float maxContact = getFloat("tc_contact_max");
    const float horizontalCalibration = getFloat("tc_horizontal_calibration");
    const float verticalCalibration = getFloat("tc_vertical_calibration");
    const float tolerance = getFloat("tc_position_tolerance");

    float bestScore = -1.0e9f;

    for (float contactDelay = minContact;
         contactDelay <= maxContact;
         contactDelay += 0.025f)
    {
        const Vector predictedBall =
            predictBallPosition(ballPosition, ballVelocity, contactDelay);

        if (predictedBall.Z < SAFE_FLOOR
            || predictedBall.Z > SAFE_CEILING
            || std::abs(predictedBall.X) > SAFE_X
            || std::abs(predictedBall.Y) > SAFE_Y)
        {
            continue;
        }

        float targetHeightMin = getFloat("tc_target_height_min");
        float targetHeightMax = getFloat("tc_target_height_max");

        if (targetHeightMin > targetHeightMax)
            std::swap(targetHeightMin, targetHeightMax);

        if (predictedBall.Z < targetHeightMin
            || predictedBall.Z > targetHeightMax)
        {
            continue;
        }

        Vector desiredOutgoing;

        if (attempt_.objective == Objective::Score)
        {
            const Vector goal{
                0.0f,
                static_cast<float>(attempt_.scenario.goalSign) * 5120.0f,
                300.0f};
            desiredOutgoing = normalized2D(goal - predictedBall);
        }
        else if (attempt_.objective == Objective::Control)
        {
            desiredOutgoing =
                length2D(ballVelocity) > 80.0f
                ? normalized2D(ballVelocity)
                : normalized2D(predictedBall - carPosition);
        }
        else
        {
            desiredOutgoing = normalized2D(predictedBall - carPosition);
        }

        const float behind =
            attempt_.objective == Objective::Control ? 125.0f : 150.0f;

        const Vector targetCar =
            predictedBall
            - desiredOutgoing * behind
            - Vector{0.0f, 0.0f, 45.0f};

        const Vector delta = targetCar - carPosition;
        const float horizontalNeed = length2D(delta);
        const float verticalNeed = std::max(0.0f, delta.Z);

        float requiredDuration = -1.0f;
        float confidence = 0.0f;

        for (float duration = 0.34f;
             duration <= std::min(2.0f, contactDelay + 0.2f);
             duration += 1.0f / 120.0f)
        {
            const float horizontalReach =
                carSpeed * duration
                + 0.5f * BOOST_ACCEL * horizontalCalibration * duration * duration
                + tolerance;

            const float secondJumpTime = std::max(0.0f, duration - 0.14f);
            const float verticalReach =
                FIRST_JUMP * duration
                + SECOND_JUMP * secondJumpTime
                + 0.5f
                    * (BOOST_ACCEL * verticalCalibration + GRAVITY_Z)
                    * duration
                    * duration
                + tolerance;

            if (horizontalReach >= horizontalNeed && verticalReach >= verticalNeed)
            {
                requiredDuration = duration;

                const float horizontalMargin =
                    (horizontalReach - horizontalNeed) / std::max(1.0f, tolerance);

                const float verticalMargin =
                    (verticalReach - verticalNeed) / std::max(1.0f, tolerance);

                confidence = clamp(
                    std::min(horizontalMargin, verticalMargin),
                    0.0f,
                    1.0f);

                break;
            }
        }

        if (requiredDuration < 0.0f)
            continue;

        const float jumpDelay = contactDelay - requiredDuration;
        const Vector requiredDirection = normalized2D(delta);
        const float alignment =
            signedAngleDeg2D(travelDirection, requiredDirection);

        float score = jumpDelay * 1000.0f + confidence * 180.0f;

        if (attempt_.objective == Objective::Fast)
            score += carSpeed * 0.15f;
        else if (attempt_.objective == Objective::Control)
            score -= std::abs(carSpeed - length2D(ballVelocity)) * 0.08f;
        else if (attempt_.objective == Objective::Score)
            score += 100.0f;

        score -= std::abs(alignment) * 10.0f;

        if (score > bestScore)
        {
            bestScore = score;
            best.valid = true;
            best.jumpDelay = jumpDelay;
            best.idealJumpAbsolute = now + jumpDelay;
            best.contactDelay = contactDelay;
            best.confidence = confidence;
            best.alignmentErrorDeg = alignment;
            best.contactPoint = predictedBall;
            best.requiredDirection = requiredDirection;
        }
    }

    return best;
}

Vector TakeoffCoach::predictBallPosition(
    Vector position,
    Vector velocity,
    float t) const
{
    return Vector{
        position.X + velocity.X * t,
        position.Y + velocity.Y * t,
        position.Z + velocity.Z * t + 0.5f * GRAVITY_Z * t * t};
}

void TakeoffCoach::renderHud(CanvasWrapper canvas)
{
    if (!getBool("tc_show_hud") || !gameWrapper->IsInFreeplay())
        return;

    const int screenWidth = static_cast<int>(canvas.GetSize().X);
    const int placement = getInt("tc_hud_position");
    const int alpha = static_cast<int>(255.0f * getFloat("tc_hud_opacity"));
    const bool feedback = attempt_.phase == Phase::Feedback;

    Vector2 origin;

    if (placement == 1)
        origin = Vector2{25, 120};
    else if (placement == 2)
        origin = Vector2{screenWidth - 455, 120};
    else
        origin = Vector2{screenWidth / 2 - 450, 24};

    const int panelWidth = placement == 0 ? 900 : 430;
    const int panelHeight = feedback ? (placement == 0 ? 205 : 330)
                                     : (placement == 0 ? 145 : 270);

    setColor(canvas, 0, 0, 0, alpha);
    canvas.SetPosition(origin);
    canvas.FillBox(Vector2{panelWidth, panelHeight});

    setColor(canvas, 255, 255, 255, 255);
    canvas.SetPosition(Vector2{origin.X + 16, origin.Y + 10});
    canvas.DrawString(attempt_.headline, 1.42f, 1.42f, true);

    auto mapAsymmetric = [](float value,
                            float greenNegative,
                            float greenPositive,
                            float yellowNegative,
                            float yellowPositive,
                            float displayNegative,
                            float displayPositive)
    {
        if (value < 0.0f)
        {
            const float magnitude = -value;

            if (magnitude <= greenNegative)
                return -0.16f * magnitude / std::max(1.0f, greenNegative);

            if (magnitude <= yellowNegative)
            {
                const float t = (magnitude - greenNegative)
                    / std::max(1.0f, yellowNegative - greenNegative);
                return -(0.16f + 0.44f * t);
            }

            const float t = (magnitude - yellowNegative)
                / std::max(1.0f, displayNegative - yellowNegative);
            return -(0.60f + 0.40f * clamp(t, 0.0f, 1.0f));
        }

        if (value <= greenPositive)
            return 0.16f * value / std::max(1.0f, greenPositive);

        if (value <= yellowPositive)
        {
            const float t = (value - greenPositive)
                / std::max(1.0f, yellowPositive - greenPositive);
            return 0.16f + 0.44f * t;
        }

        const float t = (value - yellowPositive)
            / std::max(1.0f, displayPositive - yellowPositive);
        return 0.60f + 0.40f * clamp(t, 0.0f, 1.0f);
    };

    const float timingMs = feedback
        ? attempt_.timingErrorMs
        : (attempt_.solution.valid
            ? -attempt_.solution.jumpDelay * 1000.0f
            : getFloat("tc_timing_display_late_ms"));

    const float timingNorm = mapAsymmetric(
        timingMs,
        getFloat("tc_timing_green_early_ms"),
        getFloat("tc_timing_green_late_ms"),
        getFloat("tc_timing_yellow_early_ms"),
        getFloat("tc_timing_yellow_late_ms"),
        getFloat("tc_timing_display_early_ms"),
        getFloat("tc_timing_display_late_ms"));

    const float alignment = feedback
        ? attempt_.alignmentErrorDeg
        : (attempt_.solution.valid
            ? attempt_.solution.alignmentErrorDeg
            : getFloat("tc_alignment_display"));

    const float alignmentAbs = std::abs(alignment);
    const float alignmentSign = alignment < 0.0f ? -1.0f : 1.0f;
    float alignmentNormMagnitude = 0.0f;

    if (alignmentAbs <= getFloat("tc_green_alignment"))
    {
        alignmentNormMagnitude =
            0.16f * alignmentAbs
            / std::max(0.1f, getFloat("tc_green_alignment"));
    }
    else if (alignmentAbs <= getFloat("tc_yellow_alignment"))
    {
        const float t =
            (alignmentAbs - getFloat("tc_green_alignment"))
            / std::max(
                0.1f,
                getFloat("tc_yellow_alignment")
                    - getFloat("tc_green_alignment"));

        alignmentNormMagnitude = 0.16f + 0.44f * t;
    }
    else
    {
        const float t =
            (alignmentAbs - getFloat("tc_yellow_alignment"))
            / std::max(
                0.1f,
                getFloat("tc_alignment_display")
                    - getFloat("tc_yellow_alignment"));

        alignmentNormMagnitude =
            0.60f + 0.40f * clamp(t, 0.0f, 1.0f);
    }

    const float alignmentNorm =
        alignmentSign * alignmentNormMagnitude;

    float targetMin = getFloat("tc_target_height_min");
    float targetMax = getFloat("tc_target_height_max");

    if (targetMin > targetMax)
        std::swap(targetMin, targetMax);

    const float actualHeight = feedback
        ? attempt_.contactHeight
        : (attempt_.solution.valid
            ? attempt_.solution.contactPoint.Z
            : targetMin - getFloat("tc_height_display_margin"));

    const float targetCenter = 0.5f * (targetMin + targetMax);
    const float halfGreen = std::max(1.0f, 0.5f * (targetMax - targetMin));
    const float heightError = actualHeight - targetCenter;

    const float heightNorm = mapAsymmetric(
        heightError,
        halfGreen,
        halfGreen,
        halfGreen + getFloat("tc_height_yellow_margin"),
        halfGreen + getFloat("tc_height_yellow_margin"),
        halfGreen + getFloat("tc_height_display_margin"),
        halfGreen + getFloat("tc_height_display_margin"));

    int gaugeIndex = 0;

    auto gaugeOrigin = [&](int index)
    {
        if (placement == 0)
            return Vector2{origin.X + 16 + index * 294, origin.Y + 72};

        return Vector2{origin.X + 16, origin.Y + 72 + index * 64};
    };

    if (getBool("tc_show_timing"))
    {
        drawGauge(
            canvas,
            gaugeOrigin(gaugeIndex++),
            "TIMING",
            format0(timingMs) + " ms",
            timingNorm,
            true);
    }

    if (getBool("tc_show_alignment"))
    {
        drawGauge(
            canvas,
            gaugeOrigin(gaugeIndex++),
            "ALIGN",
            format1(alignment) + " deg",
            alignmentNorm,
            true);
    }

    if (getBool("tc_show_height"))
    {
        drawGauge(
            canvas,
            gaugeOrigin(gaugeIndex++),
            "HEIGHT",
            format0(actualHeight) + " uu",
            heightNorm,
            true);
    }

    if (feedback)
    {
        const int textY = placement == 0
            ? origin.Y + 142
            : origin.Y + 72 + gaugeIndex * 64 + 8;

        canvas.SetPosition(Vector2{origin.X + 16, textY});
        canvas.DrawString(attempt_.detail, 1.02f, 1.02f, true);

        canvas.SetPosition(Vector2{origin.X + 16, textY + 30});
        canvas.DrawString(attempt_.correction, 1.08f, 1.08f, true);
    }

    setColor(canvas, 255, 255, 255, 255);
}

void TakeoffCoach::drawGauge(
    CanvasWrapper& canvas,
    Vector2 origin,
    const std::string& label,
    const std::string& value,
    float normalized,
    bool centered)
{
    const int width = 270;
    const int height = 22;

    if (centered)
    {
        setColor(canvas, 200, 55, 55, 240);
        canvas.SetPosition(origin);
        canvas.FillBox(Vector2{54, height});

        setColor(canvas, 235, 175, 45, 240);
        canvas.SetPosition(Vector2{origin.X + 54, origin.Y});
        canvas.FillBox(Vector2{58, height});

        setColor(canvas, 55, 195, 95, 248);
        canvas.SetPosition(Vector2{origin.X + 112, origin.Y});
        canvas.FillBox(Vector2{46, height});

        setColor(canvas, 235, 175, 45, 240);
        canvas.SetPosition(Vector2{origin.X + 158, origin.Y});
        canvas.FillBox(Vector2{58, height});

        setColor(canvas, 200, 55, 55, 240);
        canvas.SetPosition(Vector2{origin.X + 216, origin.Y});
        canvas.FillBox(Vector2{54, height});
    }
    else
    {
        setColor(canvas, 200, 55, 55, 240);
        canvas.SetPosition(origin);
        canvas.FillBox(Vector2{90, height});

        setColor(canvas, 235, 175, 45, 240);
        canvas.SetPosition(Vector2{origin.X + 90, origin.Y});
        canvas.FillBox(Vector2{90, height});

        setColor(canvas, 55, 195, 95, 248);
        canvas.SetPosition(Vector2{origin.X + 180, origin.Y});
        canvas.FillBox(Vector2{90, height});
    }

    const float marker01 = centered
        ? clamp((normalized + 1.0f) * 0.5f, 0.02f, 0.98f)
        : clamp(normalized, 0.02f, 0.98f);

    const int markerX =
        origin.X + static_cast<int>(marker01 * static_cast<float>(width));

    setColor(canvas, 255, 255, 255, 255);
    canvas.FillTriangle(
        Vector2{markerX, origin.Y - 9},
        Vector2{markerX - 7, origin.Y - 1},
        Vector2{markerX + 7, origin.Y - 1});

    const float textScale = getFloat("tc_indicator_text_scale");
    canvas.SetPosition(Vector2{origin.X, origin.Y + 27});
    canvas.DrawString(
        getBool("tc_show_numbers") ? label + "  " + value : label,
        textScale,
        textScale,
        true);
}

void TakeoffCoach::RenderSettings()
{
    if (ImGui::BeginTabBar("TakeoffCoachTabs"))
    {
        if (ImGui::BeginTabItem("Drill"))
        {
            renderDrillTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Setup"))
        {
            renderSetupTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Feedback"))
        {
            renderFeedbackTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void TakeoffCoach::renderDrillTab()
{
    int objective = getInt("tc_objective");
    const char* items[] = {"Fast Touch", "Control Touch", "Random Call", "Score"};

    if (ImGui::Combo("Objective", &objective, items, 4))
        setValue("tc_objective", objective);

    if (ImGui::Button("Start / New Scenario"))
        cvarManager->executeCommand("tc_new");

    ImGui::SameLine();

    if (ImGui::Button("Stop"))
        cvarManager->executeCommand("tc_stop");

    bool autoReset = getBool("tc_auto_reset");
    if (ImGui::Checkbox("Automatically start the next attempt", &autoReset))
        setValue("tc_auto_reset", autoReset);

    float feedbackSeconds = getFloat("tc_feedback_seconds");
    if (ImGui::SliderFloat(
            "Result duration",
            &feedbackSeconds,
            0.5f,
            8.0f,
            "%.1f s"))
    {
        setValue("tc_feedback_seconds", feedbackSeconds);
    }

    ImGui::TextWrapped(
        "Fast Touch rewards speed. Control Touch rewards lower relative speed. "
        "Random Call chooses one of those two each attempt.");
}

void TakeoffCoach::renderSetupTab()
{
    ImGui::TextWrapped(
        "Each pair is a random range. Put both handles together for a fixed value.");

    ImGui::Separator();
    ImGui::Text("POSITION");

    rangeControl("Approach distance", "tc_distance_min", "tc_distance_max",
        1200.0f, 6000.0f, "%.0f uu");
    rangeControl("Lateral offset", "tc_lateral_min", "tc_lateral_max",
        -2200.0f, 2200.0f, "%.0f uu");
    rangeControl("Ball height", "tc_ball_height_min", "tc_ball_height_max",
        120.0f, 1400.0f, "%.0f uu");
    rangeControl("Target contact height", "tc_target_height_min", "tc_target_height_max",
        120.0f, 1700.0f, "%.0f uu");
    ImGui::TextWrapped(
        "Default target surrounds crossbar height. The solver will not wait for a lower interception.");
    rangeControl("Setup rotation", "tc_setup_rotation_min", "tc_setup_rotation_max",
        -180.0f, 180.0f, "%.0f deg");
    rangeControl("Car facing offset", "tc_car_facing_min", "tc_car_facing_max",
        -90.0f, 90.0f, "%.0f deg");

    ImGui::Separator();
    ImGui::Text("MOTION");

    rangeControl("Car speed", "tc_car_speed_min", "tc_car_speed_max",
        0.0f, 2300.0f, "%.0f uu/s");
    rangeControl("Car velocity angle", "tc_car_velocity_angle_min", "tc_car_velocity_angle_max",
        -120.0f, 120.0f, "%.0f deg");
    rangeControl("Ball horizontal speed", "tc_ball_speed_min", "tc_ball_speed_max",
        0.0f, 2200.0f, "%.0f uu/s");
    rangeControl("Ball vertical speed", "tc_ball_vertical_min", "tc_ball_vertical_max",
        -500.0f, 1600.0f, "%.0f uu/s");
    rangeControl("Ball travel direction", "tc_ball_direction_min", "tc_ball_direction_max",
        -180.0f, 180.0f, "%.0f deg");

    ImGui::TextWrapped(
        "Ball direction: 0 = away, +/-90 = crossing, +/-180 = toward.");
}

void TakeoffCoach::renderFeedbackTab()
{
    bool showHud = getBool("tc_show_hud");
    if (ImGui::Checkbox("Show HUD", &showHud))
        setValue("tc_show_hud", showHud);

    bool showNumbers = getBool("tc_show_numbers");
    if (ImGui::Checkbox("Show numeric values", &showNumbers))
        setValue("tc_show_numbers", showNumbers);

    bool showTiming = getBool("tc_show_timing");
    if (ImGui::Checkbox("Show timing indicator", &showTiming))
        setValue("tc_show_timing", showTiming);

    bool showAlignment = getBool("tc_show_alignment");
    if (ImGui::Checkbox("Show alignment indicator", &showAlignment))
        setValue("tc_show_alignment", showAlignment);

    bool showHeight = getBool("tc_show_height");
    if (ImGui::Checkbox("Show contact-height indicator", &showHeight))
        setValue("tc_show_height", showHeight);

    int placement = getInt("tc_hud_position");
    const char* positions[] = {"Top", "Left", "Right"};

    if (ImGui::Combo("HUD position", &placement, positions, 3))
        setValue("tc_hud_position", placement);

    float opacity = getFloat("tc_hud_opacity");
    if (ImGui::SliderFloat("Panel opacity", &opacity, 0.2f, 1.0f, "%.2f"))
        setValue("tc_hud_opacity", opacity);

    float textScale = getFloat("tc_indicator_text_scale");
    if (ImGui::SliderFloat("Indicator text size", &textScale, 0.80f, 1.50f, "%.2f"))
        setValue("tc_indicator_text_scale", textScale);

    ImGui::Separator();
    ImGui::Text("TIMING RANGES");

    float greenEarly = getFloat("tc_timing_green_early_ms");
    if (ImGui::SliderFloat("Green early side", &greenEarly, 10.0f, 250.0f, "%.0f ms"))
        setValue("tc_timing_green_early_ms", greenEarly);

    float greenLate = getFloat("tc_timing_green_late_ms");
    if (ImGui::SliderFloat("Green late side", &greenLate, 10.0f, 250.0f, "%.0f ms"))
        setValue("tc_timing_green_late_ms", greenLate);

    float yellowEarly = getFloat("tc_timing_yellow_early_ms");
    if (ImGui::SliderFloat("Yellow early limit", &yellowEarly, 40.0f, 800.0f, "%.0f ms"))
        setValue("tc_timing_yellow_early_ms", yellowEarly);

    float yellowLate = getFloat("tc_timing_yellow_late_ms");
    if (ImGui::SliderFloat("Yellow late limit", &yellowLate, 40.0f, 800.0f, "%.0f ms"))
        setValue("tc_timing_yellow_late_ms", yellowLate);

    float displayEarly = getFloat("tc_timing_display_early_ms");
    if (ImGui::SliderFloat("Full early display range", &displayEarly, 150.0f, 2000.0f, "%.0f ms"))
        setValue("tc_timing_display_early_ms", displayEarly);

    float displayLate = getFloat("tc_timing_display_late_ms");
    if (ImGui::SliderFloat("Full late display range", &displayLate, 150.0f, 2000.0f, "%.0f ms"))
        setValue("tc_timing_display_late_ms", displayLate);

    ImGui::Separator();
    ImGui::Text("ALIGNMENT RANGES");

    float greenAlignment = getFloat("tc_green_alignment");
    if (ImGui::SliderFloat("Green alignment", &greenAlignment, 1.0f, 15.0f, "%.1f deg"))
        setValue("tc_green_alignment", greenAlignment);

    float yellowAlignment = getFloat("tc_yellow_alignment");
    if (ImGui::SliderFloat("Yellow alignment limit", &yellowAlignment, 4.0f, 30.0f, "%.1f deg"))
        setValue("tc_yellow_alignment", yellowAlignment);

    float alignmentDisplay = getFloat("tc_alignment_display");
    if (ImGui::SliderFloat("Full alignment display range", &alignmentDisplay, 10.0f, 90.0f, "%.1f deg"))
        setValue("tc_alignment_display", alignmentDisplay);

    ImGui::Separator();
    ImGui::Text("CONTACT HEIGHT RANGES");

    rangeControl(
        "Green target-height band",
        "tc_target_height_min",
        "tc_target_height_max",
        120.0f,
        1700.0f,
        "%.0f uu");

    float heightYellow = getFloat("tc_height_yellow_margin");
    if (ImGui::SliderFloat("Yellow height margin", &heightYellow, 20.0f, 600.0f, "%.0f uu"))
        setValue("tc_height_yellow_margin", heightYellow);

    float heightDisplay = getFloat("tc_height_display_margin");
    if (ImGui::SliderFloat("Full height display margin", &heightDisplay, 100.0f, 1200.0f, "%.0f uu"))
        setValue("tc_height_display_margin", heightDisplay);
}

bool TakeoffCoach::rangeControl(
    const char* label,
    const std::string& minName,
    const std::string& maxName,
    float low,
    float high,
    const char* format)
{
    float minimum = getFloat(minName);
    float maximum = getFloat(maxName);

    const bool changed = ImGui::DragFloatRange2(
        label,
        &minimum,
        &maximum,
        (high - low) / 500.0f,
        low,
        high,
        format,
        format);

    if (changed)
    {
        if (minimum > maximum)
            std::swap(minimum, maximum);

        setValue(minName, minimum);
        setValue(maxName, maximum);
    }

    return changed;
}

std::string TakeoffCoach::GetPluginName()
{
    return "Takeoff Coach";
}

void TakeoffCoach::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

float TakeoffCoach::nowSeconds() const
{
    using Clock = std::chrono::steady_clock;
    static const auto origin = Clock::now();

    return std::chrono::duration<float>(Clock::now() - origin).count();
}

float TakeoffCoach::getFloat(const std::string& name) const
{
    return cvarManager->getCvar(name).getFloatValue();
}

int TakeoffCoach::getInt(const std::string& name) const
{
    return cvarManager->getCvar(name).getIntValue();
}

bool TakeoffCoach::getBool(const std::string& name) const
{
    return cvarManager->getCvar(name).getBoolValue();
}

void TakeoffCoach::setValue(const std::string& name, float value)
{
    cvarManager->getCvar(name).setValue(value);
}

void TakeoffCoach::setValue(const std::string& name, int value)
{
    cvarManager->getCvar(name).setValue(value);
}

void TakeoffCoach::setValue(const std::string& name, bool value)
{
    cvarManager->getCvar(name).setValue(value ? 1 : 0);
}

float TakeoffCoach::sample(
    const std::string& minName,
    const std::string& maxName)
{
    float minimum = getFloat(minName);
    float maximum = getFloat(maxName);

    if (minimum > maximum)
        std::swap(minimum, maximum);

    if (std::abs(maximum - minimum) < 0.0001f)
        return minimum;

    std::uniform_real_distribution<float> distribution(minimum, maximum);
    return distribution(rng_);
}

TakeoffCoach::Objective TakeoffCoach::chooseObjective()
{
    const Objective configured =
        static_cast<Objective>(getInt("tc_objective"));

    if (configured == Objective::RandomCall)
    {
        std::uniform_int_distribution<int> distribution(0, 1);

        return distribution(rng_) == 0
            ? Objective::Fast
            : Objective::Control;
    }

    return configured;
}

std::string TakeoffCoach::objectiveName(Objective objective)
{
    switch (objective)
    {
    case Objective::Fast:
        return "FAST TOUCH";
    case Objective::Control:
        return "CONTROL TOUCH";
    case Objective::RandomCall:
        return "RANDOM CALL";
    case Objective::Score:
        return "SCORE";
    default:
        return "TAKEOFF";
    }
}

float TakeoffCoach::clamp(float value, float low, float high)
{
    return std::max(low, std::min(value, high));
}

float TakeoffCoach::length(const Vector& value)
{
    return std::sqrt(
        value.X * value.X
        + value.Y * value.Y
        + value.Z * value.Z);
}

float TakeoffCoach::length2D(const Vector& value)
{
    return std::sqrt(value.X * value.X + value.Y * value.Y);
}

Vector TakeoffCoach::normalized2D(const Vector& value)
{
    const float magnitude = length2D(value);

    if (magnitude < 0.001f)
        return Vector{1.0f, 0.0f, 0.0f};

    return Vector{
        value.X / magnitude,
        value.Y / magnitude,
        0.0f};
}

Vector TakeoffCoach::rotate2D(const Vector& value, float radians)
{
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);

    return Vector{
        cosine * value.X - sine * value.Y,
        sine * value.X + cosine * value.Y,
        value.Z};
}

float TakeoffCoach::signedAngleDeg2D(
    const Vector& from,
    const Vector& to)
{
    const Vector a = normalized2D(from);
    const Vector b = normalized2D(to);

    const float cross = a.X * b.Y - a.Y * b.X;
    const float dot = clamp(a.X * b.X + a.Y * b.Y, -1.0f, 1.0f);

    return std::atan2(cross, dot) * RAD_TO_DEG;
}

Vector TakeoffCoach::forwardFromRotator(Rotator rotation)
{
    const float pitch =
        static_cast<float>(rotation.Pitch) * PI / 32768.0f;

    const float yaw =
        static_cast<float>(rotation.Yaw) * PI / 32768.0f;

    const float cosPitch = std::cos(pitch);

    return Vector{
        cosPitch * std::cos(yaw),
        cosPitch * std::sin(yaw),
        std::sin(pitch)};
}

std::string TakeoffCoach::format0(float value)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(0) << value;
    return output.str();
}

std::string TakeoffCoach::format1(float value)
{
    std::ostringstream output;
    output << std::fixed << std::setprecision(1) << value;
    return output.str();
}

