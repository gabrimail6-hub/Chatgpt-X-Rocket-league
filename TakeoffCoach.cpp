#include "TakeoffCoach.h"

#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

BAKKESMOD_PLUGIN(
    TakeoffCoach,
    "Takeoff Coach",
    "5.2.0 Functional Baseline",
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

    gameWrapper->HookEvent(
        "Function TAGame.Car_TA.OnHitBall",
        [this](std::string eventName) { onBallTouchEvent(eventName); });
    gameWrapper->HookEvent(
        "Function TAGame.GameEvent_Soccar_TA.EventGoalScored",
        [this](std::string eventName) { onGoalEvent(eventName); });

    gameWrapper->RegisterDrawable(
        [this](CanvasWrapper canvas)
        {
            renderHud(canvas);
        });

    cvarManager->log("Takeoff Coach 5.2 Functional Baseline loaded.");
}

void TakeoffCoach::onUnload()
{
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.OnHitBall");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.EventGoalScored");
}

void TakeoffCoach::registerCvars()
{
    auto reg = [this](const std::string& name, const std::string& value, float low, float high)
    {
        cvarManager->registerCvar(name, value, "", true, true, low, true, high);
    };

    reg("tc_objective", "1", 0.0f, 1.0f);
    reg("tc_setup_preset", "0", 0.0f, 2.0f);
    reg("tc_guidance_style", "0", 0.0f, 1.0f);
    reg("tc_reaction_allowance_ms", "100", 0.0f, 400.0f);
    reg("tc_hide_timing_before_cue", "1", 0.0f, 1.0f);
    reg("tc_hide_alignment_before_cue", "1", 0.0f, 1.0f);
    reg("tc_hide_ball_marker_before_cue", "1", 0.0f, 1.0f);
    reg("tc_hide_takeoff_marker_before_cue", "1", 0.0f, 1.0f);
    reg("tc_show_reaction_cue", "1", 0.0f, 1.0f);
    reg("tc_cue_width", "180", 40.0f, 500.0f);
    reg("tc_cue_height", "34", 12.0f, 140.0f);
    reg("tc_cue_position_x", "0.50", 0.0f, 1.0f);
    reg("tc_cue_position_y", "0.88", 0.0f, 1.0f);
    reg("tc_cue_red_r", "255", 0.0f, 255.0f);
    reg("tc_cue_red_g", "40", 0.0f, 255.0f);
    reg("tc_cue_red_b", "40", 0.0f, 255.0f);
    reg("tc_cue_green_r", "0", 0.0f, 255.0f);
    reg("tc_cue_green_g", "255", 0.0f, 255.0f);
    reg("tc_cue_green_b", "67", 0.0f, 255.0f);
    reg("tc_auto_reset", "1", 0.0f, 1.0f);
    reg("tc_feedback_seconds", "2.5", 0.5f, 8.0f);

    reg("tc_distance_min", "1200", -1800.0f, 6000.0f);
    reg("tc_distance_max", "3000", -1800.0f, 6000.0f);
    reg("tc_lateral_min", "-900", -2200.0f, 2200.0f);
    reg("tc_lateral_max", "900", -2200.0f, 2200.0f);
    reg("tc_ball_height_min", "120", 93.0f, 2044.0f);
    reg("tc_ball_height_max", "1500", 93.0f, 2044.0f);
    reg("tc_setup_rotation_min", "90", -180.0f, 180.0f);
    reg("tc_setup_rotation_max", "90", -180.0f, 180.0f);
    reg("tc_car_facing_min", "-15", -90.0f, 90.0f);
    reg("tc_car_facing_max", "15", -90.0f, 90.0f);

    reg("tc_car_speed_min", "1500", 0.0f, 2300.0f);
    reg("tc_car_speed_max", "2300", 0.0f, 2300.0f);
    reg("tc_car_velocity_angle_min", "-10", -120.0f, 120.0f);
    reg("tc_car_velocity_angle_max", "10", -120.0f, 120.0f);

    reg("tc_ball_speed_min", "500", 0.0f, 2200.0f);
    reg("tc_ball_speed_max", "1500", 0.0f, 2200.0f);
    reg("tc_ball_vertical_min", "1000", -500.0f, 1600.0f);
    reg("tc_ball_vertical_max", "1600", -500.0f, 1600.0f);
    reg("tc_ball_direction_min", "-45", -180.0f, 180.0f);
    reg("tc_ball_direction_max", "45", -180.0f, 180.0f);

    reg("tc_contact_min", "0.55", 0.25f, 1.5f);
    reg("tc_fast_use_descending_only", "0", 0.0f, 1.0f);
    reg("tc_contact_max", "2.35", 0.8f, 3.5f);
    reg("tc_solver_hz", "20", 5.0f, 60.0f);
    reg("tc_prediction_timestep", "0.008333", 0.004167f, 0.033333f);
    reg("tc_prediction_horizon", "4.0", 1.0f, 8.0f);
    reg("tc_ball_restitution", "0.60", 0.10f, 1.00f);
    reg("tc_tangential_damping", "0.985", 0.70f, 1.00f);
    reg("tc_reject_unsupported_geometry", "1", 0.0f, 1.0f);
    reg("tc_allow_post_bounce_fallback", "0", 0.0f, 1.0f);
    reg("tc_initial_settle_time", "0.15", 0.0f, 1.5f);
    reg("tc_allowed_failed_samples", "3", 0.0f, 30.0f);
    reg("tc_max_path_corrections", "3", 0.0f, 20.0f);
    reg("tc_robustness_margin", "0.08", 0.0f, 0.5f);
    reg("tc_horizontal_calibration", "0.76", 0.4f, 1.2f);
    reg("tc_vertical_calibration", "0.78", 0.4f, 1.2f);
    reg("tc_ball_path_position_tolerance", "150", 20.0f, 500.0f);
    reg("tc_ball_path_velocity_tolerance", "120", 10.0f, 800.0f);
    reg("tc_car_contact_tolerance", "115", 30.0f, 300.0f);
    reg("tc_takeoff_marker_tolerance", "100", 20.0f, 300.0f);
    reg("tc_max_candidate_count", "14", 4.0f, 32.0f);
    reg("tc_max_profile_simulations", "160", 16.0f, 1000.0f);
    reg("tc_takeoff_confidence_min", "0.25", 0.0f, 1.0f);
    reg("tc_unreachable_setup_chance", "0", 0.0f, 100.0f);
    reg("tc_setup_validation_delay", "2.50", 0.00f, 2.50f);
    reg("tc_validation_stable_time", "0.10", 0.00f, 1.50f);
    reg("tc_max_setup_rejections", "100", 1.0f, 500.0f);
    reg("tc_min_initial_jump_delay_ms", "0", 0.0f, 1500.0f);

    // Desired ball-centre height at contact. Standard Soccar crossbar height
    // is approximately 642.775 uu, so the default band surrounds it.
    reg("tc_target_height_min", "600", 93.0f, 2044.0f);
    reg("tc_target_height_max", "685", 93.0f, 2044.0f);
    reg("tc_height_yellow_margin", "180", 20.0f, 600.0f);
    reg("tc_height_display_margin", "530", 100.0f, 1200.0f);


    reg("tc_show_hud", "1", 0.0f, 1.0f);
    reg("tc_show_numbers", "1", 0.0f, 1.0f);
    reg("tc_show_timing", "1", 0.0f, 1.0f);
    reg("tc_show_alignment", "1", 0.0f, 1.0f);
    reg("tc_show_height", "1", 0.0f, 1.0f);
    reg("tc_hud_position", "0", 0.0f, 2.0f);
    reg("tc_hud_opacity", "0.67", 0.2f, 1.0f);
    reg("tc_indicator_text_scale", "1.00", 0.80f, 1.50f);

    reg("tc_timing_green_early_ms", "90", 10.0f, 250.0f);
    reg("tc_timing_green_late_ms", "60", 10.0f, 250.0f);
    reg("tc_timing_yellow_early_ms", "260", 40.0f, 800.0f);
    reg("tc_timing_yellow_late_ms", "190", 40.0f, 800.0f);
    reg("tc_timing_display_early_ms", "1000", 150.0f, 2000.0f);
    reg("tc_timing_display_late_ms", "900", 150.0f, 2000.0f);

    reg("tc_green_alignment", "4", 1.0f, 15.0f);
    reg("tc_yellow_alignment", "13.5", 4.0f, 30.0f);
    reg("tc_alignment_display", "90", 10.0f, 90.0f);

    reg("tc_color_red_r", "100", 0.0f, 255.0f);
    reg("tc_color_red_g", "100", 0.0f, 255.0f);
    reg("tc_color_red_b", "100", 0.0f, 255.0f);
    reg("tc_color_yellow_r", "50", 0.0f, 255.0f);
    reg("tc_color_yellow_g", "111", 0.0f, 255.0f);
    reg("tc_color_yellow_b", "144", 0.0f, 255.0f);
    reg("tc_color_green_r", "0", 0.0f, 255.0f);
    reg("tc_color_green_g", "255", 0.0f, 255.0f);
    reg("tc_color_green_b", "67", 0.0f, 255.0f);
    reg("tc_show_contact_marker", "1", 0.0f, 1.0f);
    reg("tc_marker_color_r", "200", 0.0f, 255.0f);
    reg("tc_marker_color_g", "100", 0.0f, 255.0f);
    reg("tc_marker_color_b", "180", 0.0f, 255.0f);
    reg("tc_marker_candidate_r", "180", 0.0f, 255.0f);
    reg("tc_marker_candidate_g", "180", 0.0f, 255.0f);
    reg("tc_marker_candidate_b", "180", 0.0f, 255.0f);
    reg("tc_marker_shape", "1", 0.0f, 2.0f);
    reg("tc_marker_size", "18", 6.0f, 80.0f);
    reg("tc_marker_opacity", "0.90", 0.05f, 1.0f);
    reg("tc_marker_pulse", "1", 0.0f, 1.0f);
    reg("tc_marker_show_time", "1", 0.0f, 1.0f);
    reg("tc_marker_show_bounces", "1", 0.0f, 1.0f);
    reg("tc_marker_ground_projection", "1", 0.0f, 1.0f);

    reg("tc_show_takeoff_marker", "1", 0.0f, 1.0f);
    reg("tc_show_takeoff_arrow", "1", 0.0f, 1.0f);
    reg("tc_takeoff_marker_r", "0", 0.0f, 255.0f);
    reg("tc_takeoff_marker_g", "220", 0.0f, 255.0f);
    reg("tc_takeoff_marker_b", "255", 0.0f, 255.0f);
    reg("tc_takeoff_marker_size", "18", 6.0f, 60.0f);
    reg("tc_show_takeoff_line", "1", 0.0f, 1.0f);
    reg("tc_takeoff_arrow_length", "180", 40.0f, 600.0f);
    reg("tc_takeoff_line_thickness", "3", 1.0f, 10.0f);
    reg("tc_takeoff_candidate_r", "160", 0.0f, 255.0f);
    reg("tc_takeoff_candidate_g", "160", 0.0f, 255.0f);
    reg("tc_takeoff_candidate_b", "160", 0.0f, 255.0f);

    reg("tc_shoot_aim_mode", "0", 0.0f, 4.0f);
    reg("tc_shoot_custom_aim_x", "0", -850.0f, 850.0f);
    reg("tc_shoot_goal_height", "320", 120.0f, 620.0f);
    reg("tc_shoot_max_aim_error", "12", 1.0f, 45.0f);
    reg("tc_shoot_distance_min", "1200", -1800.0f, 6000.0f);
    reg("tc_shoot_distance_max", "3000", -1800.0f, 6000.0f);
    reg("tc_shoot_lateral_min", "-900", -2200.0f, 2200.0f);
    reg("tc_shoot_lateral_max", "900", -2200.0f, 2200.0f);
    reg("tc_shoot_ball_height_min", "120", 93.0f, 2044.0f);
    reg("tc_shoot_ball_height_max", "1500", 93.0f, 2044.0f);
    reg("tc_shoot_target_height_min", "600", 93.0f, 2044.0f);
    reg("tc_shoot_target_height_max", "685", 93.0f, 2044.0f);
    reg("tc_shoot_setup_rotation_min", "90", -180.0f, 180.0f);
    reg("tc_shoot_setup_rotation_max", "90", -180.0f, 180.0f);
    reg("tc_shoot_car_facing_min", "-15", -90.0f, 90.0f);
    reg("tc_shoot_car_facing_max", "15", -90.0f, 90.0f);
    reg("tc_shoot_car_speed_min", "1500", 0.0f, 2300.0f);
    reg("tc_shoot_car_speed_max", "2300", 0.0f, 2300.0f);
    reg("tc_shoot_car_velocity_angle_min", "-10", -120.0f, 120.0f);
    reg("tc_shoot_car_velocity_angle_max", "10", -120.0f, 120.0f);
    reg("tc_shoot_ball_speed_min", "500", 0.0f, 2200.0f);
    reg("tc_shoot_ball_speed_max", "1500", 0.0f, 2200.0f);
    reg("tc_shoot_ball_vertical_min", "1000", -1000.0f, 1800.0f);
    reg("tc_shoot_ball_vertical_max", "1600", -1000.0f, 1800.0f);
    reg("tc_shoot_ball_direction_min", "-45", -180.0f, 180.0f);
    reg("tc_shoot_ball_direction_max", "45", -180.0f, 180.0f);

    reg("tc_fast_distance_min", "-300", -1800.0f, 6000.0f);
    reg("tc_fast_distance_max", "2800", -1800.0f, 6000.0f);
    reg("tc_fast_lateral_min", "-1200", -2200.0f, 2200.0f);
    reg("tc_fast_lateral_max", "1200", -2200.0f, 2200.0f);
    reg("tc_fast_ball_height_min", "250", 93.0f, 2044.0f);
    reg("tc_fast_ball_height_max", "1700", 93.0f, 2044.0f);
    reg("tc_fast_target_height_min", "450", 93.0f, 2044.0f);
    reg("tc_fast_target_height_max", "1200", 93.0f, 2044.0f);
    reg("tc_fast_setup_rotation_min", "-180", -180.0f, 180.0f);
    reg("tc_fast_setup_rotation_max", "180", -180.0f, 180.0f);
    reg("tc_fast_car_facing_min", "-25", -90.0f, 90.0f);
    reg("tc_fast_car_facing_max", "25", -90.0f, 90.0f);
    reg("tc_fast_car_speed_min", "900", 0.0f, 2300.0f);
    reg("tc_fast_car_speed_max", "2300", 0.0f, 2300.0f);
    reg("tc_fast_car_velocity_angle_min", "-20", -120.0f, 120.0f);
    reg("tc_fast_car_velocity_angle_max", "20", -120.0f, 120.0f);
    reg("tc_fast_ball_speed_min", "350", 0.0f, 2200.0f);
    reg("tc_fast_ball_speed_max", "1800", 0.0f, 2200.0f);
    reg("tc_fast_ball_vertical_min", "400", -1000.0f, 1800.0f);
    reg("tc_fast_ball_vertical_max", "1600", -1000.0f, 1800.0f);
    reg("tc_fast_ball_direction_min", "-180", -180.0f, 180.0f);
    reg("tc_fast_ball_direction_max", "180", -180.0f, 180.0f);

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
    const int maximum = std::max(1, getInt("tc_max_setup_rejections"));
    if (consecutiveRerolls_ >= maximum)
    {
        attempt_.phase = Phase::Idle;
        attempt_.validationState = ValidationState::Failed;
        attempt_.headline = "Stopped after too many rejected setups.";
        attempt_.lastFailureReason = "Maximum setup rejections reached";
        return;
    }
    ++consecutiveRerolls_;
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

    const uint64_t appliedGeneration = generation;
    const Scenario appliedScenario = scenario;

    gameWrapper->SetTimeout(
        [this, appliedGeneration, appliedScenario](GameWrapper*)
        {
            if (attempt_.generation == appliedGeneration)
                applyScenario(appliedScenario);
        },
        0.04f);

    attempt_.phase = Phase::Reading;
    attempt_.validationState = ValidationState::Settling;
    attempt_.objective = scenario.objective;
    attempt_.guidanceStyle = static_cast<GuidanceStyle>(
        std::clamp(getInt("tc_guidance_style"), 0, 1));
    attempt_.scenario = scenario;
    attempt_.solution = Solution{};
    attempt_.lockedSolution = Solution{};
    attempt_.validationCandidate = Solution{};
    attempt_.lastValidSolution = Solution{};
    attempt_.jumpSolution = Solution{};
    attempt_.previousJump = false;
    attempt_.touched = false;
    attempt_.scored = false;
    attempt_.everHadSolution = false;
    attempt_.predictedBounceMismatch = false;
    attempt_.targetLocked = false;

    std::uniform_real_distribution<float> unreachableRoll(0.0f, 100.0f);
    attempt_.allowUnreachable =
        unreachableRoll(rng_) < getFloat("tc_unreachable_setup_chance");
    attempt_.startedAt = nowSeconds();
    attempt_.scenarioStartAbsolute = attempt_.startedAt;
    attempt_.settledAt = attempt_.startedAt + getFloat("tc_initial_settle_time");
    attempt_.validationDeadline = attempt_.settledAt + getFloat("tc_setup_validation_delay");
    attempt_.ballPath.clear();
    attempt_.candidateTarget = ContactTarget{};
    attempt_.lockedTarget = ContactTarget{};
    attempt_.failedValidationSamples = 0;
    attempt_.pathCorrections = 0;
    attempt_.validationStableSince = 0.0f;
    attempt_.reactionCaptured = false;
    attempt_.cueTriggered = false;
    attempt_.cueTime = 0.0f;
    attempt_.frozenCueTime = 0.0f;
    ++session_.attempts;
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
        {
            attempt_.previousBallSpeed = length(ball.GetVelocity());
            attempt_.previousBallVelocity = ball.GetVelocity();
        }
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
    scenario.goalSign = forward.Y >= 0.0f ? 1 : -1;

    return isScenarioSafe(scenario);
}

bool TakeoffCoach::isScenarioSafe(Scenario scenario) const
{
    if (std::abs(scenario.carPosition.X) > 3900.0f || std::abs(scenario.carPosition.Y) > 5000.0f)
        return false;
    if (scenario.ballPosition.Z < 93.0f || scenario.ballPosition.Z > 2044.0f)
        return false;

    Vector position = scenario.ballPosition;
    Vector velocity = scenario.ballVelocity;
    float targetMin = getFloat("tc_target_height_min");
    float targetMax = getFloat("tc_target_height_max");
    if (targetMin > targetMax) std::swap(targetMin, targetMax);
    const float dt = 1.0f / 120.0f;
    const float horizon = getFloat("tc_contact_max");

    for (float time = 0.0f; time <= horizon; time += dt)
    {
        if (position.Z >= targetMin && position.Z <= targetMax)
        {
            if (scenario.objective == Objective::FastTouch || velocity.Z < 0.0f)
                return true;
        }

        velocity.Z += GRAVITY_Z * dt;
        const Vector next = position + velocity * dt;
        // Setup safety is deliberately pre-bounce. Ceiling, floor, wall, post or crossbar
        // contact before the useful target makes the scenario unsuitable.
        if (next.Z <= 93.0f || next.Z >= 2044.0f ||
            std::abs(next.X) >= 4096.0f - 93.0f ||
            std::abs(next.Y) >= 5120.0f - 93.0f)
            return false;
        position = next;
    }
    return false;
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

void TakeoffCoach::onVehicleInput(CarWrapper car, void* params, std::string eventName)
{
    (void)eventName;
    if (params == nullptr || !gameWrapper->IsInFreeplay()) return;
    auto localCar = gameWrapper->GetLocalCar();
    if (localCar.IsNull() || car.memory_address != localCar.memory_address) return;

    const float now = nowSeconds();
    updateAttempt(car, now);
    auto* input = static_cast<ControllerInput*>(params);
    attempt_.input.throttle = input->Throttle;
    attempt_.input.steer = input->Steer;
    attempt_.input.boost = input->ActivateBoost != 0 || input->HoldingBoost != 0;
    attempt_.input.handbrake = input->Handbrake != 0;
    const bool jumpPressed = input->Jump != 0;

    if (jumpPressed && !attempt_.previousJump && attempt_.phase == Phase::Reading)
    {
        attempt_.jumpAt = now;
        attempt_.jumpAbsolute = now;
        attempt_.timingFrozen = true;
        attempt_.angleFrozen = true;
        if (attempt_.guidanceStyle == GuidanceStyle::ReactionCue && attempt_.cueTriggered)
        {
            attempt_.reactionCaptured = true;
            attempt_.reactionAllowanceMs = getFloat("tc_reaction_allowance_ms");
            attempt_.reactionRawMs = (now - attempt_.frozenCueTime) * 1000.0f;
            attempt_.reactionLossMs = std::max(0.0f, attempt_.reactionRawMs - attempt_.reactionAllowanceMs);
        }
        else
        {
            attempt_.reactionCaptured = false;
            attempt_.reactionRawMs = 0.0f;
            attempt_.reactionLossMs = 0.0f;
        }

        attempt_.jumpSolution = attempt_.solution.valid ? attempt_.solution
            : (attempt_.everHadSolution ? attempt_.lastValidSolution : Solution{});
        attempt_.phase = Phase::Airborne;

        if (attempt_.jumpSolution.valid)
        {
            attempt_.timingErrorMs = (now - attempt_.jumpSolution.idealJumpAbsolute) * 1000.0f;
            attempt_.alignmentErrorDeg = signedAngleDeg2D(
                normalized2D(forwardFromRotator(car.GetRotation())),
                normalized2D(attempt_.lockedTarget.ballPosition - car.GetLocation()));
            attempt_.jumpSolution.target = attempt_.lockedTarget;
            attempt_.jumpSolution.requiredDirection = attempt_.lockedTarget.desiredFacingDirection;
            Solution remaining = solveTargetWithProfiles(car, attempt_.lockedTarget, now);
            attempt_.targetReachableAfterJump = remaining.valid;
        }
        else
        {
            attempt_.timingErrorMs = 9999.0f;
            attempt_.alignmentErrorDeg = 9999.0f;
            attempt_.targetReachableAfterJump = false;
        }
        attempt_.headline = "TRACKING AERIAL";
    }
    attempt_.previousJump = jumpPressed;
}

void TakeoffCoach::onBallTouchEvent(std::string eventName)
{
    (void)eventName;
    if (attempt_.phase != Phase::Airborne || attempt_.touched) return;
    attempt_.touchEventSeen = true;
}

void TakeoffCoach::onGoalEvent(std::string eventName)
{
    (void)eventName;
    if (attempt_.phase != Phase::Airborne) return;
    attempt_.goalEventSeen = true;
}

void TakeoffCoach::updateAttempt(CarWrapper car, float now)
{
    if (attempt_.phase == Phase::Idle) return;
    auto server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;
    auto ball = server.GetBall();
    if (ball.IsNull()) return;

    const Vector currentVelocity = ball.GetVelocity();
    const Vector velocityDelta = currentVelocity - attempt_.previousBallVelocity;
    if (attempt_.targetLocked && length(velocityDelta) > 220.0f)
    {
        BallPredictionSlice expected;
        if (samplePath(now, expected))
        {
            const float posError = length(ball.GetLocation() - expected.position);
            const float velError = length(currentVelocity - expected.velocity);
            attempt_.predictedBounceMismatch =
                posError > getFloat("tc_ball_path_position_tolerance") * 1.75f ||
                velError > getFloat("tc_ball_path_velocity_tolerance") * 1.75f;
            if (attempt_.predictedBounceMismatch) attempt_.pathStillValid = false;
        }
    }
    attempt_.previousBallVelocity = currentVelocity;

    if (attempt_.phase == Phase::Reading) updateReading(car, ball, now);
    else if (attempt_.phase == Phase::Airborne) updateAirborne(car, ball, now);
    else if (attempt_.phase == Phase::Feedback && now >= attempt_.feedbackUntil)
    {
        if (getBool("tc_auto_reset")) requestNewScenario();
        else attempt_.phase = Phase::Idle;
    }
}

void TakeoffCoach::updateReading(CarWrapper car, BallWrapper ball, float now)
{
    const float hz = std::max(5.0f, getFloat("tc_solver_hz"));
    attempt_.closestDistance = std::min(attempt_.closestDistance, length(ball.GetLocation() - car.GetLocation()));
    if (now < attempt_.settledAt)
    {
        attempt_.validationState = ValidationState::Settling;
        attempt_.headline = objectiveName(attempt_.objective) + " | SETTLING";
        return;
    }

    if (now - attempt_.lastSolveAt >= 1.0f / hz)
    {
        attempt_.lastSolveAt = now;
        if (!attempt_.targetLocked)
        {
            if (attempt_.ballPath.empty())
            {
                attempt_.snapshotBallPosition = ball.GetLocation();
                attempt_.snapshotBallVelocity = ball.GetVelocity();
                attempt_.ballPath = buildBallPath(ball.GetLocation(), ball.GetVelocity(), ball.GetAngularVelocity(), now);
                attempt_.validationState = ValidationState::SelectingCandidates;
                attempt_.candidateTarget = selectContactTarget(car, attempt_.ballPath, now, &attempt_.validationCandidate);
                attempt_.candidateCreatedAbsolute = now;
                attempt_.solution = attempt_.validationCandidate;
                attempt_.initialCandidateTime = now;
                attempt_.initialAvailableJumpDelay = attempt_.validationCandidate.jumpDelay;
                if (attempt_.validationCandidate.valid &&
                    attempt_.initialAvailableJumpDelay < getFloat("tc_min_initial_jump_delay_ms") / 1000.0f)
                    attempt_.validationCandidate = Solution{};
                attempt_.cueTime = attempt_.validationCandidate.idealJumpAbsolute;
                if (!attempt_.validationCandidate.valid) attempt_.lastFailureReason = "No reachable target";
            }

            BallPredictionSlice expected;
            const bool sampled = samplePath(now, expected);
            const bool pathMatches = sampled &&
                length(ball.GetLocation() - expected.position) <= getFloat("tc_ball_path_position_tolerance") &&
                length(ball.GetVelocity() - expected.velocity) <= getFloat("tc_ball_path_velocity_tolerance");

            if (pathMatches)
            {
                attempt_.failedValidationSamples = 0;
                if (attempt_.validationStableSince <= 0.0f) attempt_.validationStableSince = now;
            }
            else
            {
                ++attempt_.failedValidationSamples;
                attempt_.validationStableSince = 0.0f;
                if (!attempt_.cueTriggered && attempt_.failedValidationSamples > getInt("tc_allowed_failed_samples") &&
                    attempt_.pathCorrections < getInt("tc_max_path_corrections"))
                {
                    ++attempt_.pathCorrections;
                    attempt_.failedValidationSamples = 0;
                    attempt_.ballPath = buildBallPath(ball.GetLocation(), ball.GetVelocity(), ball.GetAngularVelocity(), now);
                    attempt_.candidateTarget = selectContactTarget(car, attempt_.ballPath, now, &attempt_.validationCandidate);
                    attempt_.solution = attempt_.validationCandidate;
                    attempt_.initialCandidateTime = now;
                    attempt_.initialAvailableJumpDelay = attempt_.validationCandidate.jumpDelay;
                    if (attempt_.validationCandidate.valid &&
                        attempt_.initialAvailableJumpDelay < getFloat("tc_min_initial_jump_delay_ms") / 1000.0f)
                        attempt_.validationCandidate = Solution{};
                    attempt_.cueTime = attempt_.validationCandidate.idealJumpAbsolute;
                }
            }

            const bool stable = attempt_.validationStableSince > 0.0f &&
                now - attempt_.validationStableSince >= getFloat("tc_validation_stable_time");
            if (stable && attempt_.validationCandidate.valid)
            {
                attempt_.lockedTarget = attempt_.candidateTarget;
                attempt_.lockedSolution = attempt_.validationCandidate;
                attempt_.solution = attempt_.lockedSolution;
                attempt_.lastValidSolution = attempt_.lockedSolution;
                attempt_.everHadSolution = true;
                attempt_.targetLocked = true;
                attempt_.validationState = ValidationState::Locked;
                attempt_.targetLockedAbsolute = now;
                attempt_.lockedContactAbsolute = attempt_.lockedTarget.contactAbsoluteTime;
                attempt_.cueTime = attempt_.lockedSolution.idealJumpAbsolute;
                consecutiveRerolls_ = 0;
            }

            if (!attempt_.cueTriggered && attempt_.guidanceStyle == GuidanceStyle::ReactionCue
                && attempt_.validationCandidate.valid && now >= attempt_.cueTime)
            {
                attempt_.cueTriggered = true;
                attempt_.frozenCueTime = attempt_.cueTime;
                attempt_.cueAbsolute = attempt_.frozenCueTime;
                attempt_.lockedTarget = attempt_.candidateTarget;
                attempt_.lockedSolution = attempt_.validationCandidate;
                attempt_.solution = attempt_.lockedSolution;
                attempt_.targetLocked = true;
                attempt_.targetLockedAbsolute = now;
                attempt_.validationState = ValidationState::Locked;
            }

            if (now >= attempt_.validationDeadline && !attempt_.targetLocked)
            {
                attempt_.validationState = ValidationState::Failed;
                attempt_.lastFailureReason = attempt_.validationCandidate.valid ? "Path did not validate in time" : "No reachable target";
                requestNewScenario();
                return;
            }
        }
        else
        {
            if (!attempt_.cueTriggered)
            {
                BallPredictionSlice expected;
                if (samplePath(now, expected))
                {
                    const bool stillMatches =
                        length(ball.GetLocation() - expected.position) <= getFloat("tc_ball_path_position_tolerance") * 1.5f &&
                        length(ball.GetVelocity() - expected.velocity) <= getFloat("tc_ball_path_velocity_tolerance") * 1.5f;
                    if (!stillMatches && attempt_.guidanceStyle == GuidanceStyle::ReactionCue)
                    {
                        attempt_.targetLocked = false;
                        attempt_.ballPath.clear();
                        attempt_.validationStableSince = 0.0f;
                        return;
                    }
                }
            }
            attempt_.solution = solveLockedTarget(car, now);
            if (attempt_.solution.valid) attempt_.lastValidSolution = attempt_.solution;
        }
    }

    if (!attempt_.targetLocked)
    {
        attempt_.headline = attempt_.candidateTarget.valid
            ? objectiveName(attempt_.objective) + " | CANDIDATE"
            : objectiveName(attempt_.objective) + " | VERIFYING PATH";
        return;
    }

    if (attempt_.guidanceStyle == GuidanceStyle::ReactionCue && !attempt_.cueTriggered && now >= attempt_.cueTime)
    {
        attempt_.cueTriggered = true;
        attempt_.frozenCueTime = attempt_.cueTime;
        attempt_.lockedTarget = attempt_.lockedSolution.target;
        attempt_.solution = attempt_.lockedSolution;
        attempt_.cueAbsolute = attempt_.frozenCueTime;
    }

    const bool reactionCue = attempt_.guidanceStyle == GuidanceStyle::ReactionCue;
    if (reactionCue && !attempt_.cueTriggered) attempt_.headline = objectiveName(attempt_.objective) + " | WAIT FOR GREEN";
    else if (attempt_.solution.jumpDelay > getFloat("tc_timing_green_early_ms") / 1000.0f) attempt_.headline = objectiveName(attempt_.objective) + " | WAIT";
    else if (attempt_.solution.jumpDelay >= -getFloat("tc_timing_green_late_ms") / 1000.0f) attempt_.headline = objectiveName(attempt_.objective) + " | JUMP NOW";
    else attempt_.headline = objectiveName(attempt_.objective) + " | LATE";
}

void TakeoffCoach::updateAirborne(CarWrapper car, BallWrapper ball, float now)
{
    const Vector carPosition = car.GetLocation();
    const Vector ballPosition = ball.GetLocation();
    const float distanceNow = length(ballPosition - carPosition);
    attempt_.closestDistance = std::min(attempt_.closestDistance, distanceNow);

    const Vector velocityDelta = ball.GetVelocity() - attempt_.previousBallVelocity;
    const Vector away = normalized(ballPosition - carPosition);
    const bool geometricTouch = distanceNow < 205.0f && length(velocityDelta) > 35.0f && dot(velocityDelta, away) > 10.0f;
    const bool newTouch = !attempt_.touched && (attempt_.touchEventSeen || geometricTouch);
    const bool positionGoal = std::abs(ballPosition.Y) > 5120.0f && std::abs(ballPosition.X) < 900.0f && ballPosition.Z < 650.0f;
    const bool scored = attempt_.goalEventSeen || positionGoal;

    if (newTouch)
    {
        attempt_.touched = true;
        attempt_.actualTouchAbsolute = now;
        attempt_.touchAbsolute = now;
        const bool comparableTouch = attempt_.targetLocked && attempt_.pathStillValid
            && !attempt_.predictedBounceMismatch
            && length(ballPosition - attempt_.lockedTarget.ballPosition) <= getFloat("tc_car_contact_tolerance") * 2.0f;
        attempt_.possibleTimeSavedMs = comparableTouch
            ? std::max(0.0f, (now - attempt_.lockedTarget.contactAbsoluteTime) * 1000.0f)
            : -1.0f;
        if (attempt_.objective != Objective::Shoot)
        {
            finishAttempt(car, ball, true, false, now);
            return;
        }
    }
    if (scored)
    {
        finishAttempt(car, ball, attempt_.touched, true, now);
        return;
    }
    const float deadline = attempt_.touched && attempt_.objective == Objective::Shoot
        ? attempt_.lockedTarget.contactAbsoluteTime + 4.0f
        : attempt_.lockedTarget.contactAbsoluteTime + 1.0f;
    if (now > deadline) finishAttempt(car, ball, attempt_.touched, false, now);
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

    if (attempt_.predictedBounceMismatch)
    {
        attempt_.headline = "TIMING INVALID: WALL OR CEILING BOUNCE";
        attempt_.correction =
            "This attempt is not graded because the locked prediction did not include that bounce.";
    }
    else if (timing < -greenEarly)
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
    else if (attempt_.objective == Objective::Shoot)
    {
        attempt_.headline = scored ? "GOAL" : "TOUCH, NO GOAL";
        attempt_.correction =
            scored ? "Good timing and contact side."
                   : "Contact farther behind the ball toward goal.";
    }
    else
    {
        attempt_.headline = "FAST TOUCH COMPLETE";
        attempt_.correction =
            "Car speed at contact: "
            + format0(length(car.GetVelocity()))
            + " uu/s.";
    }

    if (attempt_.guidanceStyle == GuidanceStyle::ReactionCue && attempt_.reactionCaptured)
    {
        attempt_.correction += " Raw reaction " + format0(attempt_.reactionRawMs)
            + " ms; allowance " + format0(attempt_.reactionAllowanceMs)
            + " ms; reaction loss " + format0(attempt_.reactionLossMs) + " ms.";
        if (touched && attempt_.possibleTimeSavedMs >= 0.0f)
            attempt_.correction += " Total contact-time loss "
                + format0(attempt_.possibleTimeSavedMs) + " ms.";
        else if (touched)
            attempt_.correction += " Contact-time comparison unavailable: target path changed.";
        else
            attempt_.correction += " Closest approach " + format0(attempt_.closestDistance)
                + " uu; target " + std::string(attempt_.targetReachableAfterJump ? "was still reachable." : "was no longer reachable.");
    }

    if (touched) ++session_.touches;
    if (scored) ++session_.goals;
    if (timing < 9000.0f)
    {
        ++session_.gradedTimingCount;
        session_.absTimingTotal += std::abs(timing);
    }
    if (alignment < 9000.0f)
    {
        ++session_.gradedAngleCount;
        session_.absoluteAngleTotal += std::abs(attempt_.alignmentErrorDeg);
    }
    if (touched)
    {
        if (attempt_.possibleTimeSavedMs >= 0.0f)
        {
            ++session_.possibleSavedCount;
            session_.possibleSavedTotal += attempt_.possibleTimeSavedMs;
        }
        session_.touchHeightTotal += attempt_.contactHeight;
        session_.contactSpeedTotal += length(car.GetVelocity());
    }
    if (attempt_.guidanceStyle == GuidanceStyle::ReactionCue && attempt_.reactionCaptured)
    {
        ++session_.reactionCount;
        session_.reactions.push_back(attempt_.reactionRawMs);
    }

    std::ostringstream detail;
    detail << "Timing ";

    if (attempt_.everHadSolution && timing < 9000.0f)
        detail << (timing >= 0.0f ? "+" : "") << format0(timing) << " ms";
    else
        detail << "n/a";

    detail << " | Aim ";
    if (attempt_.everHadSolution && alignment < 9000.0f)
        detail << format1(attempt_.alignmentErrorDeg) << " deg";
    else
        detail << "n/a";

    detail << " | Closest " << format0(attempt_.closestDistance) << " uu";
    attempt_.detail = detail.str();
}

TakeoffCoach::Solution TakeoffCoach::solve(
    CarWrapper car,
    BallWrapper ball,
    float now)
{
    const Vector ballPosition = ball.GetLocation();
    const Vector ballVelocity = ball.GetVelocity();
    const Vector angularVelocity = ball.GetAngularVelocity();

    attempt_.ballPath = buildBallPath(ballPosition, ballVelocity, angularVelocity, now);
    attempt_.candidateTarget = selectContactTarget(car, attempt_.ballPath, now);
    if (!attempt_.candidateTarget.valid)
        return Solution{};

    return solveTargetWithProfiles(car, attempt_.candidateTarget, now);
}

std::vector<TakeoffCoach::BallPredictionSlice> TakeoffCoach::buildBallPath(
    Vector position, Vector velocity, Vector angularVelocity, float now)
{
    std::vector<BallPredictionSlice> path;
    attempt_.bounceEvents.clear();
    const float dt = getFloat("tc_prediction_timestep");
    const float horizon = getFloat("tc_prediction_horizon");
    const float radius = 93.0f;
    int bounceCount = 0;
    SurfaceType lastBounce = SurfaceType::None;

    for (float t = 0.0f; t <= horizon; t += dt)
    {
        BallPredictionSlice slice;
        slice.absoluteTime = now + t;
        slice.position = position;
        slice.velocity = velocity;
        slice.angularVelocity = angularVelocity;
        slice.bounceCount = bounceCount;
        slice.lastBounceSurface = lastBounce;
        slice.supportedGeometry = !isUnsupportedTransition(position, radius);
        path.push_back(slice);

        float remaining = dt;
        for (int sub = 0; sub < 4 && remaining > 0.00001f; ++sub)
        {
            const float step = remaining / static_cast<float>(4 - sub);
            velocity.Z += GRAVITY_Z * step;
            Vector next = position + velocity * step;
            SurfaceType surface = SurfaceType::None;
            Vector incoming = velocity;
            if (collideBall(next, velocity, angularVelocity, radius, surface))
            {
                ++bounceCount;
                lastBounce = surface;
                BounceEvent event;
                event.absoluteTime = now + t + step;
                event.position = next;
                event.incomingVelocity = incoming;
                event.outgoingVelocity = velocity;
                event.surface = surface;
                attempt_.bounceEvents.push_back(event);
            }
            position = next;
            remaining -= step;
        }
    }
    return path;
}

bool TakeoffCoach::isUnsupportedTransition(const Vector& p, float r) const
{
    const float side = 4096.0f - r;
    const float back = 5120.0f - r;
    const float ceiling = 2044.0f - r;
    const bool floorSideCurve = p.Z < 260.0f && std::abs(p.X) > side - 180.0f;
    const bool floorBackCurve = p.Z < 260.0f && std::abs(p.Y) > back - 180.0f && std::abs(p.X) > 950.0f;
    const bool wallCeilingCurve = p.Z > ceiling - 180.0f && (std::abs(p.X) > side - 180.0f || std::abs(p.Y) > back - 180.0f);
    const bool mouthCurve = std::abs(std::abs(p.Y) - 5120.0f) < 170.0f && std::abs(std::abs(p.X) - 893.0f) < 180.0f && p.Z < 300.0f;
    return floorSideCurve || floorBackCurve || wallCeilingCurve || mouthCurve;
}

bool TakeoffCoach::collideBall(Vector& p, Vector& v, Vector& w, float r, SurfaceType& surface) const
{
    struct Candidate { float penetration; Vector normal; SurfaceType surface; Vector corrected; };
    std::vector<Candidate> hits;
    const float side = 4096.0f - r;
    const float back = 5120.0f - r;
    const float ceil = 2044.0f - r;
    const float goalHalf = 893.0f - r;
    const float goalHeight = 642.0f - r;
    const float goalDepth = 880.0f;

    if (p.Z < r) hits.push_back({r-p.Z,{0,0,1},SurfaceType::Ground,{p.X,p.Y,r}});
    if (p.Z > ceil) hits.push_back({p.Z-ceil,{0,0,-1},SurfaceType::Ceiling,{p.X,p.Y,ceil}});
    if (std::abs(p.X) > side)
    {
        float s=p.X>0?1.0f:-1.0f;
        hits.push_back({std::abs(p.X)-side,{-s,0,0},SurfaceType::SideWall,{s*side,p.Y,p.Z}});
    }
    const bool inGoalMouth = std::abs(p.X) < goalHalf && p.Z < goalHeight;
    if (!inGoalMouth && std::abs(p.Y) > back)
    {
        float s=p.Y>0?1.0f:-1.0f;
        hits.push_back({std::abs(p.Y)-back,{0,-s,0},SurfaceType::BackWall,{p.X,s*back,p.Z}});
    }
    if (inGoalMouth && std::abs(p.Y) > 5120.0f)
    {
        float s=p.Y>0?1.0f:-1.0f;
        if (std::abs(p.Y) > 5120.0f+goalDepth-r)
            hits.push_back({std::abs(p.Y)-(5120.0f+goalDepth-r),{0,-s,0},SurfaceType::GoalBackWall,{p.X,s*(5120.0f+goalDepth-r),p.Z}});
        if (p.Z < r) hits.push_back({r-p.Z,{0,0,1},SurfaceType::GoalFloor,{p.X,p.Y,r}});
        if (p.Z > goalHeight) hits.push_back({p.Z-goalHeight,{0,0,-1},SurfaceType::GoalCeiling,{p.X,p.Y,goalHeight}});
    }

    // Goal posts and crossbar: choose only the deepest penetration this substep.
    for (float sy : {-1.0f,1.0f}) for (float sx : {-1.0f,1.0f})
    {
        Vector centre{sx*893.0f,sy*5120.0f,p.Z};
        Vector d{p.X-centre.X,p.Y-centre.Y,0};
        float len=length2D(d), rr=r+82.0f;
        if (len < rr && p.Z < 720.0f && len>0.01f)
        {
            Vector n{d.X/len,d.Y/len,0};
            hits.push_back({rr-len,n,SurfaceType::GoalPost,centre+n*rr});
        }
    }
    if (std::abs(std::abs(p.Y)-5120.0f) < r+85.0f && std::abs(p.X)<893.0f && std::abs(p.Z-642.0f)<r+85.0f)
    {
        float sy=p.Y>0?1.0f:-1.0f;
        Vector centre{p.X,sy*5120.0f,642.0f};
        Vector d{0,p.Y-centre.Y,p.Z-centre.Z};
        float len=std::sqrt(d.Y*d.Y+d.Z*d.Z), rr=r+85.0f;
        if (len<rr && len>0.01f)
        {
            Vector n{0,d.Y/len,d.Z/len};
            hits.push_back({rr-len,n,SurfaceType::Crossbar,centre+n*rr});
        }
    }
    if (hits.empty()) return false;
    const Candidate& hit=*std::max_element(hits.begin(),hits.end(),[](const Candidate&a,const Candidate&b){return a.penetration<b.penetration;});
    p=hit.corrected; surface=hit.surface;
    const Vector n=normalized(hit.normal);
    const Vector contactVelocity=v+cross(w,n*(-r));
    const Vector normalPart=n*dot(v,n);
    const Vector tangentPart=v-normalPart;
    const Vector contactTangent=contactVelocity-n*dot(contactVelocity,n);
    const float restitution=getFloat("tc_ball_restitution");
    const float damping=getFloat("tc_tangential_damping");
    v=normalPart*(-restitution)+tangentPart*damping;
    const Vector tangentialImpulse=(damping-1.0f)*contactTangent;
    v=v+tangentialImpulse*0.35f;
    w=w+cross(n*(-r),tangentialImpulse)*(0.00012f);
    w=w*0.995f;
    return true;
}

Vector TakeoffCoach::chooseGoalPoint(const ContactTarget& base, Vector carPosition) const
{
    const float goalY = static_cast<float>(attempt_.scenario.goalSign) * 5120.0f;
    const int mode = getInt("tc_shoot_aim_mode");
    const Vector approach = normalized2D(base.ballPosition - carPosition);
    const Vector leftPost{-650.0f, goalY, getFloat("tc_shoot_goal_height")};
    const Vector rightPost{650.0f, goalY, getFloat("tc_shoot_goal_height")};
    const float leftAngle = std::abs(signedAngleDeg2D(approach, normalized2D(leftPost - base.ballPosition)));
    const float rightAngle = std::abs(signedAngleDeg2D(approach, normalized2D(rightPost - base.ballPosition)));
    const float nearX = leftAngle < rightAngle ? -650.0f : 650.0f;
    const float farX = -nearX;
    float x = 0.0f;
    if (mode == static_cast<int>(AimMode::NearPost)) x = nearX;
    else if (mode == static_cast<int>(AimMode::FarPost)) x = farX;
    else if (mode == static_cast<int>(AimMode::RandomCorridor)) x = std::sin(base.contactAbsoluteTime * 13.37f) * 650.0f;
    else if (mode == static_cast<int>(AimMode::Custom)) x = getFloat("tc_shoot_custom_aim_x");
    return Vector{x, goalY, getFloat("tc_shoot_goal_height")};
}

TakeoffCoach::ContactTarget TakeoffCoach::selectContactTarget(
    CarWrapper car, const std::vector<BallPredictionSlice>& path, float now, Solution* selectedSolution) const
{
    struct Candidate { ContactTarget target; float delay; float correction; bool preBounce; bool descending; };
    struct Ranked { ContactTarget target; Solution solution; float delay; float correction; bool preBounce; bool descending; };
    std::vector<Candidate> candidates;
    float minHeight = getFloat("tc_target_height_min");
    float maxHeight = getFloat("tc_target_height_max");
    if (minHeight > maxHeight) std::swap(minHeight, maxHeight);
    const float minContact = getFloat("tc_contact_min");
    const float maxContact = getFloat("tc_contact_max");

    for (const BallPredictionSlice& slice : path)
    {
        const float delay = slice.absoluteTime - now;
        if (delay < minContact || delay > maxContact) continue;
        if (!slice.supportedGeometry && getBool("tc_reject_unsupported_geometry")) break;
        if (slice.position.Z < minHeight || slice.position.Z > maxHeight) continue;
        const bool preBounce = slice.bounceCount == 0;
        if (!preBounce && !getBool("tc_allow_post_bounce_fallback")) continue;
        const bool descending = slice.velocity.Z < 0.0f;
        if (attempt_.objective == Objective::Shoot && !descending) continue;
        if (attempt_.objective == Objective::FastTouch && getBool("tc_fast_use_descending_only") && !descending) continue;

        ContactTarget target;
        target.valid = true;
        target.contactAbsoluteTime = slice.absoluteTime;
        target.ballPosition = slice.position;
        target.ballVelocity = slice.velocity;
        target.bounceCount = slice.bounceCount;
        target.lastSurface = slice.lastBounceSurface;

        if (attempt_.objective == Objective::Shoot)
        {
            target.desiredGoalPoint = chooseGoalPoint(target, car.GetLocation());
            const Vector goalDirection = normalized2D(target.desiredGoalPoint - target.ballPosition);
            target.desiredFacingDirection = goalDirection;
            target.desiredCarPosition = target.ballPosition - goalDirection * 145.0f - Vector{0, 0, 45};
            // Informational approximation only. It never rejects a reachable target.
            const Vector normal = normalized(goalDirection + Vector{0, 0, 0.06f});
            const float relativeNormal = std::max(0.0f, dot(car.GetVelocity() - slice.velocity, normal));
            target.predictedOutgoingVelocity = slice.velocity + normal * (380.0f + 1.25f * relativeNormal);
            target.predictedShotSpeed = length(target.predictedOutgoingVelocity);
            target.shotAimErrorDeg = std::abs(signedAngleDeg2D(normalized2D(target.predictedOutgoingVelocity), goalDirection));
        }
        else
        {
            target.desiredFacingDirection = normalized2D(target.ballPosition - car.GetLocation());
            target.desiredCarPosition = target.ballPosition - target.desiredFacingDirection * 145.0f - Vector{0, 0, 45};
        }

        if (!cheapReachable(car, target, now)) continue;
        const float correction = std::abs(signedAngleDeg2D(
            normalized2D(forwardFromRotator(car.GetRotation())),
            normalized2D(target.ballPosition - car.GetLocation())));
        candidates.push_back({target, delay, correction, preBounce, descending});
    }

    if (candidates.empty()) return ContactTarget{};
    const bool hasPreBounce = std::any_of(candidates.begin(), candidates.end(), [](const Candidate& c){ return c.preBounce; });
    if (hasPreBounce)
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [](const Candidate& c){ return !c.preBounce; }), candidates.end());

    std::stable_sort(candidates.begin(), candidates.end(), [this](const Candidate& a, const Candidate& b)
    {
        if (attempt_.objective == Objective::Shoot)
        {
            if (a.preBounce != b.preBounce) return a.preBounce > b.preBounce;
            if (a.descending != b.descending) return a.descending > b.descending;
            if (std::abs(a.target.shotAimErrorDeg - b.target.shotAimErrorDeg) > 0.01f)
                return a.target.shotAimErrorDeg < b.target.shotAimErrorDeg;
        }
        if (std::abs(a.delay - b.delay) > 0.001f) return a.delay < b.delay;
        return a.correction < b.correction;
    });

    const int maximum = std::min<int>(12, static_cast<int>(candidates.size()));
    std::vector<Ranked> reachable;
    const auto solveStarted = std::chrono::steady_clock::now();
    const_cast<Attempt&>(attempt_).targetCandidatesTested = maximum;
    const_cast<Attempt&>(attempt_).reachableCandidates = 0;
    const_cast<Attempt&>(attempt_).profileSimulations = 0;

    for (int i = 0; i < maximum; ++i)
    {
        Solution solution = solveTargetWithProfiles(car, candidates[i].target, now);
        if (!solution.valid) solution = buildSimpleFallback(car, candidates[i].target, now);
        if (!solution.valid) continue;
        ++const_cast<Attempt&>(attempt_).reachableCandidates;
        reachable.push_back({candidates[i].target, solution, candidates[i].delay,
                             candidates[i].correction, candidates[i].preBounce, candidates[i].descending});
    }

    const_cast<Attempt&>(attempt_).lastSolveDurationMs =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - solveStarted).count();
    if (reachable.empty()) return ContactTarget{};

    std::stable_sort(reachable.begin(), reachable.end(), [this](const Ranked& a, const Ranked& b)
    {
        if (attempt_.objective == Objective::Shoot)
        {
            if (a.preBounce != b.preBounce) return a.preBounce > b.preBounce;
            if (a.descending != b.descending) return a.descending > b.descending;
            if (std::abs(a.target.shotAimErrorDeg - b.target.shotAimErrorDeg) > 0.01f)
                return a.target.shotAimErrorDeg < b.target.shotAimErrorDeg;
        }
        if (std::abs(a.delay - b.delay) > 0.001f) return a.delay < b.delay;
        if (std::abs(a.correction - b.correction) > 0.01f) return a.correction < b.correction;
        return a.solution.robustness > b.solution.robustness;
    });

    Ranked best = reachable.front();
    // Visible takeoff marker: continue the player's current state; never show ideal bot steering.
    Vector p = car.GetLocation();
    Vector v = car.GetVelocity();
    float yaw = std::atan2(forwardFromRotator(car.GetRotation()).Y, forwardFromRotator(car.GetRotation()).X);
    const float groundTime = std::max(0.0f, best.solution.jumpDelay);
    const float dt = 1.0f / 120.0f;
    for (float t = 0.0f; t < groundTime; t += dt)
    {
        const float step = std::min(dt, groundTime - t);
        const float speed = length2D(v);
        const float turnRate = (attempt_.input.handbrake ? 2.2f : 1.25f) * clamp(1.0f - speed / 3000.0f, 0.25f, 1.0f);
        yaw += attempt_.input.steer * turnRate * step;
        const Vector nose{std::cos(yaw), std::sin(yaw), 0.0f};
        v = v + nose * (attempt_.input.throttle * 900.0f * step);
        if (attempt_.input.boost) v = v + nose * (BOOST_ACCEL * step);
        Vector horizontal{v.X, v.Y, 0.0f};
        if (length2D(horizontal) > 2300.0f) { horizontal = normalized2D(horizontal) * 2300.0f; v.X = horizontal.X; v.Y = horizontal.Y; }
        p = p + v * step;
        p.Z = car.GetLocation().Z;
    }
    best.solution.takeoff = TakeoffState{p, v, Vector{std::cos(yaw), std::sin(yaw), 0.0f}, best.solution.idealJumpAbsolute};
    best.solution.idealTakeoffPosition = p;

    if (selectedSolution) *selectedSolution = best.solution;
    const_cast<Attempt&>(attempt_).solverBackend = best.solution.simpleFallback ? "SIMPLE FALLBACK" : "ADVANCED + LIVE TAKEOFF";
    return best.target;
}

bool TakeoffCoach::simulateAerialProfile(
    CarWrapper car, const ContactTarget& target, AerialProfile profile, float duration, Solution& out) const
{
    if (duration<=0.0f) return false;
    const float now=nowSeconds();
    const float available=target.contactAbsoluteTime-now;
    const float jumpDelay=available-duration;
    if (jumpDelay<0.0f) return false;
    Vector p=car.GetLocation(), v=car.GetVelocity();
    Vector forward=normalized(forwardFromRotator(car.GetRotation()));
    float yaw=std::atan2(forward.Y,forward.X), pitch=std::asin(clamp(forward.Z,-1.0f,1.0f));
    const Vector desiredGround=normalized2D(target.desiredCarPosition-p);
    const float groundDt=1.0f/120.0f;
    for (float t=0.0f;t<jumpDelay;t+=groundDt)
    {
        const float angle=signedAngleDeg2D(Vector{std::cos(yaw),std::sin(yaw),0},desiredGround);
        yaw+=clamp(angle*DEG_TO_RAD,-2.6f*groundDt,2.6f*groundDt);
        Vector nose{std::cos(yaw),std::sin(yaw),0};
        const float speed=length2D(v);
        const float accel=(speed<1400.0f?1000.0f:420.0f) * getFloat("tc_horizontal_calibration");
        const float throttle = clamp(attempt_.input.throttle, -1.0f, 1.0f);
        const float liveAccel = accel * throttle;
        v=v+nose*(liveAccel*groundDt);
        if (length2D(v)>2300.0f) { Vector hv=normalized2D(v)*2300.0f; v.X=hv.X; v.Y=hv.Y; }
        p=p+v*groundDt; p.Z=car.GetLocation().Z;
    }
    TakeoffState takeoff{p,v,Vector{std::cos(yaw),std::sin(yaw),0},now+jumpDelay};

    float secondJumpDelay=999.0f,jumpHold=0.0f,boostDelay=0.0f,pitchDelay=0.0f,pitchStrength=1.0f;
    switch(profile){
    case AerialProfile::SingleJump: jumpHold=.16f;boostDelay=.08f;pitchDelay=.08f;break;
    case AerialProfile::FastAerial: secondJumpDelay=.10f;jumpHold=.20f;boostDelay=0;pitchDelay=.03f;break;
    case AerialProfile::DelayedSecondJump: secondJumpDelay=.28f;jumpHold=.16f;boostDelay=.06f;pitchDelay=.08f;break;
    case AerialProfile::Conservative: secondJumpDelay=.18f;jumpHold=.12f;boostDelay=.12f;pitchDelay=.12f;pitchStrength=.82f;break; }
    const float verticalCalibration = getFloat("tc_vertical_calibration");
    v.Z+=FIRST_JUMP * verticalCalibration;
    bool second=false; float boostSeconds=0.0f; const float dt=1.0f/120.0f;
    const Vector targetDir=normalized(target.desiredCarPosition-p);
    for(float t=0;t<duration;t+=dt)
    {
        if(t>=pitchDelay)
        {
            const float desiredPitch=std::asin(clamp(targetDir.Z,-0.95f,0.95f));
            const float pitchError=desiredPitch-pitch;
            pitch+=clamp(pitchError*5.0f,-2.8f,2.8f)*dt*pitchStrength;
            const float desiredYaw=std::atan2(targetDir.Y,targetDir.X);
            float yawError=desiredYaw-yaw;
            while(yawError>PI)yawError-=2*PI; while(yawError<-PI)yawError+=2*PI;
            yaw+=clamp(yawError*4.0f,-2.5f,2.5f)*dt;
        }
        forward=Vector{std::cos(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),std::sin(pitch)};
        if(!second&&t>=secondJumpDelay){v=v+forward*(SECOND_JUMP * verticalCalibration);second=true;}
        if(t<jumpHold)v.Z+=1458.0f*verticalCalibration*dt;
        if(t>=boostDelay){v=v+forward*(BOOST_ACCEL*verticalCalibration*dt);boostSeconds+=dt;}
        if(length(v)>2300.0f)v=normalized(v)*2300.0f;
        v.Z+=GRAVITY_Z*dt; p=p+v*dt;
    }
    const float error=length(target.desiredCarPosition-p);
    const float tolerance=getFloat("tc_car_contact_tolerance");
    if(error>tolerance)return false;
    out.valid=true;out.profile=profile;out.requiredAerialDuration=duration;out.requiredBoost=boostSeconds*33.3f;
    float availableBoost = 0.0f;
    auto boost = car.GetBoostComponent();
    if (!boost.IsNull()) availableBoost = boost.GetCurrentBoostAmount();
    out.availableBoost = availableBoost;
    out.boostDeficit=out.requiredBoost>availableBoost;
    out.carContactError=error;out.confidence=clamp(1.0f-error/std::max(1.0f,tolerance),0.0f,1.0f);
    out.robustness=clamp(out.confidence-getFloat("tc_robustness_margin"),0.0f,1.0f);
    out.takeoff=takeoff;out.idealTakeoffPosition=takeoff.position;out.requiredDirection=target.desiredFacingDirection;
    out.contactPoint=target.ballPosition;out.target=target;return true;
}

bool TakeoffCoach::cheapReachable(CarWrapper car, const ContactTarget& target, float now) const
{
    const float delay = target.contactAbsoluteTime - now;
    if (delay <= 0.05f) return false;
    const Vector delta = target.desiredCarPosition - car.GetLocation();
    const float horizontal = length2D(delta);
    const float vertical = delta.Z;
    const float currentSpeed = length2D(car.GetVelocity());
    const float horizontalReach = currentSpeed * delay + 0.5f * 900.0f * getFloat("tc_horizontal_calibration") * delay * delay + 180.0f;
    const float verticalReach = std::max(0.0f, 292.0f * getFloat("tc_vertical_calibration") * delay
        + 0.5f * (BOOST_ACCEL * getFloat("tc_vertical_calibration") + 650.0f) * delay * delay);
    return horizontal <= horizontalReach && vertical <= verticalReach + 180.0f;
}

TakeoffCoach::Solution TakeoffCoach::buildSimpleFallback(CarWrapper car, const ContactTarget& target, float now) const
{
    Solution result;
    if (!target.valid || !cheapReachable(car, target, now)) return result;
    const float delay = target.contactAbsoluteTime - now;
    const Vector delta = target.desiredCarPosition - car.GetLocation();
    const float duration = clamp(0.28f + length2D(delta) / 1800.0f + std::max(0.0f, delta.Z) / 1500.0f, 0.32f, std::max(0.32f, delay));
    result.valid = duration <= delay;
    if (!result.valid) return Solution{};
    result.simpleFallback = true;
    result.contactDelay = delay;
    result.requiredAerialDuration = duration;
    result.jumpDelay = delay - duration;
    result.idealJumpAbsolute = target.contactAbsoluteTime - duration;
    result.target = target;
    result.contactPoint = target.ballPosition;
    result.requiredDirection = normalized2D(target.ballPosition - car.GetLocation());
    result.alignmentErrorDeg = signedAngleDeg2D(normalized2D(forwardFromRotator(car.GetRotation())), result.requiredDirection);
    result.confidence = 0.25f;
    result.robustness = 0.15f;
    result.takeoff.position = car.GetLocation() + car.GetVelocity() * std::max(0.0f, result.jumpDelay);
    result.takeoff.position.Z = car.GetLocation().Z;
    result.takeoff.velocity = car.GetVelocity();
    result.takeoff.facingDirection = normalized2D(forwardFromRotator(car.GetRotation()));
    result.takeoff.absoluteTime = result.idealJumpAbsolute;
    result.idealTakeoffPosition = result.takeoff.position;
    return result;
}

TakeoffCoach::Solution TakeoffCoach::solveTargetWithProfiles(
    CarWrapper car,
    const ContactTarget& target,
    float now) const
{
    Solution best;
    const float available = target.contactAbsoluteTime - now;
    const AerialProfile profiles[] = {
        AerialProfile::FastAerial, AerialProfile::DelayedSecondJump,
        AerialProfile::SingleJump};

    for (AerialProfile profile : profiles)
    {

        const float maximumDuration = std::min(2.5f, available + 0.4f);
        float firstReachable = -1.0f;
        for (float duration = 0.28f; duration <= maximumDuration; duration += 0.08f)
        {
            if (attempt_.profileSimulations >= getInt("tc_max_profile_simulations")) break;
            ++const_cast<Attempt&>(attempt_).profileSimulations;
            Solution candidate;
            if (simulateAerialProfile(car, target, profile, duration, candidate)) { firstReachable = duration; break; }
        }
        if (firstReachable < 0.0f) continue;
        const float refineStart = std::max(0.28f, firstReachable - 0.08f);
        for (float duration = refineStart; duration <= firstReachable + 0.001f; duration += 0.01f)
        {
            if (attempt_.profileSimulations >= getInt("tc_max_profile_simulations")) break;
            ++const_cast<Attempt&>(attempt_).profileSimulations;
            Solution candidate;
            if (!simulateAerialProfile(car, target, profile, duration, candidate)) continue;
            candidate.jumpDelay = available - duration;
            candidate.contactDelay = available;
            candidate.idealJumpAbsolute = target.contactAbsoluteTime - duration;
            candidate.alignmentErrorDeg = signedAngleDeg2D(
                normalized2D(forwardFromRotator(car.GetRotation())),
                normalized2D(target.ballPosition - car.GetLocation()));
            if (!best.valid || duration < best.requiredAerialDuration ||
                (std::abs(duration - best.requiredAerialDuration) < 0.001f && candidate.robustness > best.robustness))
                best = candidate;
            break;
        }
    }
    return best;
}

TakeoffCoach::Solution TakeoffCoach::solveLockedTarget(
    CarWrapper car,
    float now) const
{
    if (!attempt_.targetLocked || !attempt_.lockedTarget.valid)
        return Solution{};
    if (attempt_.cueTriggered || attempt_.phase == Phase::Airborne)
    {
        Solution frozen = attempt_.lockedSolution;
        frozen.jumpDelay = frozen.idealJumpAbsolute - now;
        frozen.contactDelay = attempt_.lockedTarget.contactAbsoluteTime - now;
        frozen.alignmentErrorDeg = signedAngleDeg2D(normalized2D(forwardFromRotator(car.GetRotation())), normalized2D(attempt_.lockedTarget.ballPosition - car.GetLocation()));
        return frozen;
    }
    Solution live = solveTargetWithProfiles(car, attempt_.lockedTarget, now);
    if (!live.valid)
    {
        live = attempt_.lockedSolution;
        live.jumpDelay = live.idealJumpAbsolute - now;
        live.contactDelay = attempt_.lockedTarget.contactAbsoluteTime - now;
        live.alignmentErrorDeg = signedAngleDeg2D(
            normalized2D(forwardFromRotator(car.GetRotation())),
            normalized2D(attempt_.lockedTarget.ballPosition - car.GetLocation()));
    }
    return live;
}

bool TakeoffCoach::samplePath(float absoluteTime, BallPredictionSlice& out) const
{
    if (attempt_.ballPath.empty()) return false;
    auto it = std::lower_bound(attempt_.ballPath.begin(), attempt_.ballPath.end(), absoluteTime,
        [](const BallPredictionSlice& s, float t) { return s.absoluteTime < t; });
    if (it == attempt_.ballPath.end()) return false;
    out = *it;
    return true;
}

void TakeoffCoach::renderHud(CanvasWrapper canvas)
{
    if (!getBool("tc_show_hud") || !gameWrapper->IsInFreeplay())
        return;

    const int screenWidth =
        static_cast<int>(canvas.GetSize().X);

    const int placement =
        getInt("tc_hud_position");

    const int alpha =
        static_cast<int>(
            255.0f * getFloat("tc_hud_opacity"));

    const bool feedback =
        attempt_.phase == Phase::Feedback;

    const bool jumped =
        attempt_.phase == Phase::Airborne
        || attempt_.phase == Phase::Feedback;
    const float hudNow = nowSeconds();
    const bool cueMode = attempt_.guidanceStyle == GuidanceStyle::ReactionCue;
    const bool beforeCue = cueMode && !attempt_.cueTriggered;
    const bool timingVisible = !(beforeCue && getBool("tc_hide_timing_before_cue"));
    const bool alignmentVisible = !(beforeCue && getBool("tc_hide_alignment_before_cue"));
    const bool ballMarkerVisible = !(beforeCue && getBool("tc_hide_ball_marker_before_cue"));
    const bool takeoffMarkerVisible = !(beforeCue && getBool("tc_hide_takeoff_marker_before_cue"));

    Vector2 origin;

    if (placement == 1)
        origin = Vector2{25, 120};
    else if (placement == 2)
        origin = Vector2{screenWidth - 455, 120};
    else
        origin = Vector2{screenWidth / 2 - 450, 24};

    const int panelWidth =
        placement == 0 ? 900 : 430;

    const int panelHeight =
        feedback
        ? (placement == 0 ? 205 : 330)
        : (placement == 0 ? 145 : 270);

    setColor(canvas, 0, 0, 0, alpha);
    canvas.SetPosition(origin);
    canvas.FillBox(
        Vector2{panelWidth, panelHeight});

    setColor(canvas, 255, 255, 255, 255);
    canvas.SetPosition(
        Vector2{origin.X + 16, origin.Y + 10});

    canvas.DrawString(
        attempt_.headline,
        1.42f,
        1.42f,
        true);

    const Solution displayedSolution = jumped
        ? attempt_.jumpSolution
        : (attempt_.targetLocked ? attempt_.solution : attempt_.validationCandidate);

    const float timingMs = displayedSolution.valid
        ? (jumped ? attempt_.timingErrorMs : (hudNow - displayedSolution.idealJumpAbsolute) * 1000.0f)
        : getFloat("tc_timing_display_late_ms");

    float alignment = getFloat("tc_alignment_display");
    if (displayedSolution.valid)
    {
        if (jumped) alignment = attempt_.alignmentErrorDeg;
        else
        {
            auto liveCar = gameWrapper->GetLocalCar();
            const ContactTarget& angleTarget = attempt_.targetLocked ? attempt_.lockedTarget : displayedSolution.target;
            if (!liveCar.IsNull() && angleTarget.valid)
                alignment = signedAngleDeg2D(
                    normalized2D(forwardFromRotator(liveCar.GetRotation())),
                    normalized2D(angleTarget.ballPosition - liveCar.GetLocation()));
        }
    }

    float targetMin =
        getFloat("tc_target_height_min");

    float targetMax =
        getFloat("tc_target_height_max");

    if (targetMin > targetMax)
        std::swap(targetMin, targetMax);

    const float targetCenter =
        0.5f * (targetMin + targetMax);

    const float halfGreen =
        std::max(
            1.0f,
            0.5f * (targetMax - targetMin));

    float liveBallHeight = targetCenter;

    if (!feedback)
    {
        auto server =
            gameWrapper->GetCurrentGameState();

        if (!server.IsNull())
        {
            auto ball = server.GetBall();

            if (!ball.IsNull())
                liveBallHeight =
                    ball.GetLocation().Z;
        }
    }

    const float actualHeight =
        feedback
        ? attempt_.contactHeight
        : liveBallHeight;

    const float heightError =
        actualHeight - targetCenter;

    int gaugeIndex = 0;

    auto gaugeOrigin = [&](int index)
    {
        if (placement == 0)
        {
            return Vector2{
                origin.X + 16 + index * 294,
                origin.Y + 72};
        }

        return Vector2{
            origin.X + 16,
            origin.Y + 72 + index * 64};
    };

    if (timingVisible && getBool("tc_show_timing"))
    {
        drawGauge(
            canvas,
            gaugeOrigin(gaugeIndex++),
            "TIMING",
            displayedSolution.valid
                ? format0(timingMs) + " ms"
                : "n/a",
            timingMs,
            getFloat("tc_timing_green_early_ms"),
            getFloat("tc_timing_green_late_ms"),
            getFloat("tc_timing_yellow_early_ms"),
            getFloat("tc_timing_yellow_late_ms"),
            getFloat("tc_timing_display_early_ms"),
            getFloat("tc_timing_display_late_ms"));
    }

    if (alignmentVisible && getBool("tc_show_alignment"))
    {
        drawGauge(
            canvas,
            gaugeOrigin(gaugeIndex++),
            "ALIGN",
            displayedSolution.valid
                ? format1(alignment) + " deg"
                : "n/a",
            alignment,
            getFloat("tc_green_alignment"),
            getFloat("tc_green_alignment"),
            getFloat("tc_yellow_alignment"),
            getFloat("tc_yellow_alignment"),
            getFloat("tc_alignment_display"),
            getFloat("tc_alignment_display"));
    }

    if (getBool("tc_show_height"))
    {
        drawGauge(
            canvas,
            gaugeOrigin(gaugeIndex++),
            "HEIGHT",
            format0(actualHeight) + " uu",
            heightError,
            halfGreen,
            halfGreen,
            halfGreen
                + getFloat("tc_height_yellow_margin"),
            halfGreen
                + getFloat("tc_height_yellow_margin"),
            halfGreen
                + getFloat("tc_height_display_margin"),
            halfGreen
                + getFloat("tc_height_display_margin"));
    }

    const Solution visualTarget =
        attempt_.targetLocked ? attempt_.lockedSolution : attempt_.validationCandidate;

    auto onScreen = [&](Vector2 p)
    {
        return p.X > 2 && p.Y > 2 && p.X < canvas.GetSize().X - 2 && p.Y < canvas.GetSize().Y - 2;
    };
    auto cameraFacing = [&](const Vector& worldPoint)
    {
        auto camera=gameWrapper->GetCamera();
        if (camera.IsNull()) return true;
        const Vector cameraForward=forwardFromRotator(camera.GetRotation());
        return dot(cameraForward,worldPoint-camera.GetLocation())>0.0f;
    };

    if (cueMode && getBool("tc_show_reaction_cue") && !feedback)
    {
        // The reaction cue is deliberately independent of markers, gauges and the advanced solver.
        const bool green = attempt_.cueTriggered;
        setColor(canvas,
            getInt(green ? "tc_cue_green_r" : "tc_cue_red_r"),
            getInt(green ? "tc_cue_green_g" : "tc_cue_red_g"),
            getInt(green ? "tc_cue_green_b" : "tc_cue_red_b"), 255);
        const int cx = static_cast<int>(canvas.GetSize().X * getFloat("tc_cue_position_x"));
        const int cy = static_cast<int>(canvas.GetSize().Y * getFloat("tc_cue_position_y"));
        const int width = static_cast<int>(getFloat("tc_cue_width"));
        const int height = static_cast<int>(getFloat("tc_cue_height"));
        canvas.SetPosition(Vector2{cx - width / 2, cy - height / 2});
        canvas.FillBox(Vector2{width, height});
    }

    if (ballMarkerVisible && getBool("tc_show_contact_marker") && visualTarget.valid && !feedback)
    {
        const Vector2 marker = canvas.Project(visualTarget.target.ballPosition);
        if (cameraFacing(visualTarget.target.ballPosition) && onScreen(marker))
        {
            const bool locked = attempt_.targetLocked;
            const float pulse = getBool("tc_marker_pulse")
                ? 0.80f + 0.20f * std::abs(std::sin(hudNow * 5.0f)) : 1.0f;
            const int size = static_cast<int>(getFloat("tc_marker_size") * pulse);
            setColor(canvas,
                getInt(locked ? "tc_marker_color_r" : "tc_marker_candidate_r"),
                getInt(locked ? "tc_marker_color_g" : "tc_marker_candidate_g"),
                getInt(locked ? "tc_marker_color_b" : "tc_marker_candidate_b"),
                static_cast<int>(255.0f * getFloat("tc_marker_opacity")));
            const int shape = getInt("tc_marker_shape");
            if (shape == 0)
            {
                canvas.DrawLine(Vector2{marker.X-size,marker.Y},Vector2{marker.X+size,marker.Y},3.0f);
                canvas.DrawLine(Vector2{marker.X,marker.Y-size},Vector2{marker.X,marker.Y+size},3.0f);
            }
            else if (shape == 1)
                canvas.DrawRect(Vector2{marker.X-size,marker.Y-size},Vector2{marker.X+size,marker.Y+size});
            else
            {
                for (int i=0;i<24;++i)
                {
                    const float a=2.0f*PI*i/24.0f, b=2.0f*PI*(i+1)/24.0f;
                    const int ax = static_cast<int>(std::lround(marker.X + std::cos(a) * size));
                    const int ay = static_cast<int>(std::lround(marker.Y + std::sin(a) * size));
                    const int bx = static_cast<int>(std::lround(marker.X + std::cos(b) * size));
                    const int by = static_cast<int>(std::lround(marker.Y + std::sin(b) * size));
                    canvas.DrawLine(Vector2{ax, ay}, Vector2{bx, by}, 2.0f);
                }
            }

            if (getBool("tc_marker_show_time"))
            {
                canvas.SetPosition(Vector2{marker.X + size + 6, marker.Y - 8});
                canvas.DrawString(format1(visualTarget.target.contactAbsoluteTime - hudNow) + " s", 0.9f, 0.9f, true);
            }
            if (getBool("tc_marker_show_bounces"))
            {
                canvas.SetPosition(Vector2{marker.X + size + 6, marker.Y + 10});
                canvas.DrawString("bounces " + std::to_string(visualTarget.target.bounceCount), 0.8f, 0.8f, true);
            }
        }

        if (getBool("tc_marker_ground_projection"))
        {
            Vector ground = visualTarget.target.ballPosition;
            ground.Z = 0.0f;
            Vector2 gp = canvas.Project(ground);
            if (cameraFacing(ground) && onScreen(marker) && onScreen(gp)) canvas.DrawLine(marker, gp, 1.5f);
        }
    }

    if (takeoffMarkerVisible && visualTarget.valid && visualTarget.takeoff.absoluteTime > 0.0f
        && visualTarget.confidence >= getFloat("tc_takeoff_confidence_min")
        && getBool("tc_show_takeoff_marker") && !feedback)
    {
        Vector takeoff = visualTarget.idealTakeoffPosition;
        takeoff.Z = 17.0f;
        const Vector direction = visualTarget.target.desiredFacingDirection;
        const Vector arrowEnd = takeoff + direction * getFloat("tc_takeoff_arrow_length");
        Vector2 p0 = canvas.Project(takeoff);
        Vector2 p1 = canvas.Project(arrowEnd);
        Vector2 target2 = canvas.Project(visualTarget.target.ballPosition);
        if (cameraFacing(takeoff) && onScreen(p0))
        {
            const bool locked = attempt_.targetLocked;
            setColor(canvas,
                getInt(locked ? "tc_takeoff_marker_r" : "tc_takeoff_candidate_r"),
                getInt(locked ? "tc_takeoff_marker_g" : "tc_takeoff_candidate_g"),
                getInt(locked ? "tc_takeoff_marker_b" : "tc_takeoff_candidate_b"), 235);
            const int sz = static_cast<int>(getFloat("tc_takeoff_marker_size"));
            canvas.DrawRect(Vector2{p0.X - sz, p0.Y - sz}, Vector2{p0.X + sz, p0.Y + sz});
            if (getBool("tc_show_takeoff_arrow") && onScreen(p1))
                canvas.DrawLine(p0, p1, getFloat("tc_takeoff_line_thickness"));
            if (getBool("tc_show_takeoff_line") && onScreen(target2))
                canvas.DrawLine(p0, target2, getFloat("tc_takeoff_line_thickness"));
        }
    }

    if (!feedback && attempt_.objective == Objective::Shoot && visualTarget.valid && getBool("tc_show_numbers"))
    {
        canvas.SetPosition(Vector2{origin.X+16,origin.Y+panelHeight-24});
        canvas.DrawString(std::string("Target goal: ")+(attempt_.scenario.goalSign>0?"Orange":"Blue")
            +" | Aim "+format0(visualTarget.target.desiredGoalPoint.X)+", "
            +format0(visualTarget.target.desiredGoalPoint.Y)+", "+format0(visualTarget.target.desiredGoalPoint.Z),0.82f,0.82f,true);
    }

    if (feedback)
    {
        const int textY =
            placement == 0
            ? origin.Y + 142
            : origin.Y + 72 + gaugeIndex * 64 + 8;

        canvas.SetPosition(
            Vector2{origin.X + 16, textY});

        canvas.DrawString(
            attempt_.detail,
            1.02f,
            1.02f,
            true);

        canvas.SetPosition(
            Vector2{origin.X + 16, textY + 30});

        canvas.DrawString(
            attempt_.correction,
            1.08f,
            1.08f,
            true);
    }

    setColor(canvas, 255, 255, 255, 255);
}

void TakeoffCoach::drawGauge(
    CanvasWrapper& canvas,
    Vector2 origin,
    const std::string& label,
    const std::string& valueText,
    float value,
    float greenNegative,
    float greenPositive,
    float yellowNegative,
    float yellowPositive,
    float displayNegative,
    float displayPositive)
{
    const int width = 270;
    const int height = 22;

    greenNegative =
        clamp(greenNegative, 0.0f, displayNegative);

    greenPositive =
        clamp(greenPositive, 0.0f, displayPositive);

    yellowNegative =
        clamp(
            yellowNegative,
            greenNegative,
            displayNegative);

    yellowPositive =
        clamp(
            yellowPositive,
            greenPositive,
            displayPositive);

    const float fullRange =
        std::max(
            1.0f,
            displayNegative + displayPositive);

    auto xForValue = [&](float numericValue)
    {
        const float clampedValue =
            clamp(
                numericValue,
                -displayNegative,
                displayPositive);

        const float fraction =
            (clampedValue + displayNegative)
            / fullRange;

        return origin.X
            + static_cast<int>(
                fraction
                * static_cast<float>(width));
    };

    const int redR = getInt("tc_color_red_r");
    const int redG = getInt("tc_color_red_g");
    const int redB = getInt("tc_color_red_b");

    const int yellowR = getInt("tc_color_yellow_r");
    const int yellowG = getInt("tc_color_yellow_g");
    const int yellowB = getInt("tc_color_yellow_b");

    const int greenR = getInt("tc_color_green_r");
    const int greenG = getInt("tc_color_green_g");
    const int greenB = getInt("tc_color_green_b");

    auto drawSegment = [&](
        float from,
        float to,
        int r,
        int g,
        int b)
    {
        const int x1 = xForValue(from);
        const int x2 = xForValue(to);

        const int segmentWidth =
            std::max(1, x2 - x1);

        setColor(canvas, r, g, b, 242);

        canvas.SetPosition(
            Vector2{x1, origin.Y});

        canvas.FillBox(
            Vector2{segmentWidth, height});
    };

    drawSegment(
        -displayNegative,
        -yellowNegative,
        redR,
        redG,
        redB);

    drawSegment(
        -yellowNegative,
        -greenNegative,
        yellowR,
        yellowG,
        yellowB);

    drawSegment(
        -greenNegative,
        greenPositive,
        greenR,
        greenG,
        greenB);

    drawSegment(
        greenPositive,
        yellowPositive,
        yellowR,
        yellowG,
        yellowB);

    drawSegment(
        yellowPositive,
        displayPositive,
        redR,
        redG,
        redB);

    const int markerX =
        static_cast<int>(
            clamp(
                static_cast<float>(xForValue(value)),
                static_cast<float>(origin.X + 4),
                static_cast<float>(
                    origin.X + width - 4)));

    setColor(canvas, 255, 255, 255, 255);

    canvas.FillTriangle(
        Vector2{markerX, origin.Y - 9},
        Vector2{markerX - 7, origin.Y - 1},
        Vector2{markerX + 7, origin.Y - 1});

    const float textScale =
        getFloat("tc_indicator_text_scale");

    canvas.SetPosition(
        Vector2{origin.X, origin.Y + 27});

    canvas.DrawString(
        getBool("tc_show_numbers")
            ? label + "  " + valueText
            : label,
        textScale,
        textScale,
        true);
}

void TakeoffCoach::RenderSettings()
{
    ImGui::TextUnformatted("Takeoff Coach build: 5.3.0-runtime-wiring");
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

        if (ImGui::BeginTabItem("Solver / Advanced"))
        {
            renderAdvancedTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

std::string TakeoffCoach::modePrefix(int preset) const
{
    if (preset == 0)
        return "tc_shoot_";
    return preset == 0 ? "tc_shoot_" : "tc_fast_";
}

namespace
{
    const std::vector<std::pair<std::string, std::string>>& presetFields()
    {
        static const std::vector<std::pair<std::string, std::string>> fields = {
            {"distance_min","tc_distance_min"},{"distance_max","tc_distance_max"},
            {"lateral_min","tc_lateral_min"},{"lateral_max","tc_lateral_max"},
            {"ball_height_min","tc_ball_height_min"},{"ball_height_max","tc_ball_height_max"},
            {"target_height_min","tc_target_height_min"},{"target_height_max","tc_target_height_max"},
            {"setup_rotation_min","tc_setup_rotation_min"},{"setup_rotation_max","tc_setup_rotation_max"},
            {"car_facing_min","tc_car_facing_min"},{"car_facing_max","tc_car_facing_max"},
            {"car_speed_min","tc_car_speed_min"},{"car_speed_max","tc_car_speed_max"},
            {"car_velocity_angle_min","tc_car_velocity_angle_min"},
            {"car_velocity_angle_max","tc_car_velocity_angle_max"},
            {"ball_speed_min","tc_ball_speed_min"},{"ball_speed_max","tc_ball_speed_max"},
            {"ball_vertical_min","tc_ball_vertical_min"},{"ball_vertical_max","tc_ball_vertical_max"},
            {"ball_direction_min","tc_ball_direction_min"},{"ball_direction_max","tc_ball_direction_max"}};
        return fields;
    }
}

void TakeoffCoach::saveModePreset(int /*preset*/)
{
    // Built-in setup memories are immutable.
}

void TakeoffCoach::loadModePreset(int preset)
{
    const std::string prefix = modePrefix(preset);
    for (const auto& field : presetFields())
        setValue(field.second, getFloat(prefix + field.first));
}

void TakeoffCoach::applyPreset(int preset)
{
    preset = std::max(0, std::min(preset, 1));
    setValue("tc_setup_preset", preset);
    loadModePreset(preset);
}

void TakeoffCoach::resetModePreset(int preset)
{
    if (preset == 0)
    {
        setValue("tc_shoot_distance_min",1200.0f); setValue("tc_shoot_distance_max",3000.0f);
        setValue("tc_shoot_lateral_min",-900.0f); setValue("tc_shoot_lateral_max",900.0f);
        setValue("tc_shoot_ball_height_min",120.0f); setValue("tc_shoot_ball_height_max",1500.0f);
        setValue("tc_shoot_target_height_min",600.0f); setValue("tc_shoot_target_height_max",685.0f);
        setValue("tc_shoot_setup_rotation_min",90.0f); setValue("tc_shoot_setup_rotation_max",90.0f);
        setValue("tc_shoot_car_facing_min",-15.0f); setValue("tc_shoot_car_facing_max",15.0f);
        setValue("tc_shoot_car_speed_min",1500.0f); setValue("tc_shoot_car_speed_max",2300.0f);
        setValue("tc_shoot_car_velocity_angle_min",-10.0f); setValue("tc_shoot_car_velocity_angle_max",10.0f);
        setValue("tc_shoot_ball_speed_min",500.0f); setValue("tc_shoot_ball_speed_max",1500.0f);
        setValue("tc_shoot_ball_vertical_min",1000.0f); setValue("tc_shoot_ball_vertical_max",1600.0f);
        setValue("tc_shoot_ball_direction_min",-45.0f); setValue("tc_shoot_ball_direction_max",45.0f);
    }
    else if (preset == 1)
    {
        setValue("tc_fast_distance_min",-300.0f); setValue("tc_fast_distance_max",2800.0f);
        setValue("tc_fast_lateral_min",-1200.0f); setValue("tc_fast_lateral_max",1200.0f);
        setValue("tc_fast_ball_height_min",250.0f); setValue("tc_fast_ball_height_max",1700.0f);
        setValue("tc_fast_target_height_min",450.0f); setValue("tc_fast_target_height_max",1200.0f);
        setValue("tc_fast_setup_rotation_min",-180.0f); setValue("tc_fast_setup_rotation_max",180.0f);
        setValue("tc_fast_car_facing_min",-25.0f); setValue("tc_fast_car_facing_max",25.0f);
        setValue("tc_fast_car_speed_min",900.0f); setValue("tc_fast_car_speed_max",2300.0f);
        setValue("tc_fast_car_velocity_angle_min",-20.0f); setValue("tc_fast_car_velocity_angle_max",20.0f);
        setValue("tc_fast_ball_speed_min",350.0f); setValue("tc_fast_ball_speed_max",1800.0f);
        setValue("tc_fast_ball_vertical_min",400.0f); setValue("tc_fast_ball_vertical_max",1600.0f);
        setValue("tc_fast_ball_direction_min",-180.0f); setValue("tc_fast_ball_direction_max",180.0f);
    }
    if (preset >= 0 && preset <= 1 && getInt("tc_setup_preset") == preset) loadModePreset(preset);
}

void TakeoffCoach::renderDrillTab()
{
    int objective = getInt("tc_objective");
    const char* objectives[] = {"Fast Touch", "Shoot"};
    if (ImGui::Combo("Objective", &objective, objectives, 2))
    {
        setValue("tc_objective", objective);
    }

    int guidance = getInt("tc_guidance_style");
    const char* guidanceModes[] = {"Read", "Reaction Cue"};
    if (ImGui::Combo("Guidance", &guidance, guidanceModes, 2))
        setValue("tc_guidance_style", guidance);

    if (ImGui::Button("Start / New Scenario")) cvarManager->executeCommand("tc_new");
    ImGui::SameLine();
    if (ImGui::Button("Stop")) cvarManager->executeCommand("tc_stop");

    bool autoReset = getBool("tc_auto_reset");
    if (ImGui::Checkbox("Automatically next attempt", &autoReset)) setValue("tc_auto_reset", autoReset);

    float result = getFloat("tc_feedback_seconds");
    if (ImGui::SliderFloat("Result duration", &result, 0.5f, 8.0f, "%.1f s")) setValue("tc_feedback_seconds", result);

    if (guidance == 1)
    {
        float allowance = getFloat("tc_reaction_allowance_ms");
        if (ImGui::SliderFloat("Human reaction allowance", &allowance, 0.0f, 400.0f, "%.0f ms"))
            setValue("tc_reaction_allowance_ms", allowance);
        bool showCue=getBool("tc_show_reaction_cue"); if(ImGui::Checkbox("Show reaction cue",&showCue))setValue("tc_show_reaction_cue",showCue);
        bool hideTiming=getBool("tc_hide_timing_before_cue"); if(ImGui::Checkbox("Hide timing before cue",&hideTiming))setValue("tc_hide_timing_before_cue",hideTiming);
        bool hideAlignment=getBool("tc_hide_alignment_before_cue"); if(ImGui::Checkbox("Hide alignment before cue",&hideAlignment))setValue("tc_hide_alignment_before_cue",hideAlignment);
        bool hideBall=getBool("tc_hide_ball_marker_before_cue"); if(ImGui::Checkbox("Hide ball marker before cue",&hideBall))setValue("tc_hide_ball_marker_before_cue",hideBall);
        bool hideTakeoff=getBool("tc_hide_takeoff_marker_before_cue"); if(ImGui::Checkbox("Hide takeoff marker before cue",&hideTakeoff))setValue("tc_hide_takeoff_marker_before_cue",hideTakeoff);
        float cueWidth=getFloat("tc_cue_width"); if(ImGui::SliderFloat("Cue width",&cueWidth,40.0f,500.0f,"%.0f px"))setValue("tc_cue_width",cueWidth);
        float cueHeight=getFloat("tc_cue_height"); if(ImGui::SliderFloat("Cue height",&cueHeight,12.0f,140.0f,"%.0f px"))setValue("tc_cue_height",cueHeight);
        float cueX=getFloat("tc_cue_position_x"); if(ImGui::SliderFloat("Cue X",&cueX,0.0f,1.0f,"%.2f"))setValue("tc_cue_position_x",cueX);
        float cueY=getFloat("tc_cue_position_y"); if(ImGui::SliderFloat("Cue Y",&cueY,0.0f,1.0f,"%.2f"))setValue("tc_cue_position_y",cueY);
        float cueRed[3] = {getFloat("tc_cue_red_r")/255.0f, getFloat("tc_cue_red_g")/255.0f, getFloat("tc_cue_red_b")/255.0f};
        if (ImGui::ColorEdit3("Cue red", cueRed)) { setValue("tc_cue_red_r", cueRed[0]*255.0f); setValue("tc_cue_red_g", cueRed[1]*255.0f); setValue("tc_cue_red_b", cueRed[2]*255.0f); }
        float cueGreen[3] = {getFloat("tc_cue_green_r")/255.0f, getFloat("tc_cue_green_g")/255.0f, getFloat("tc_cue_green_b")/255.0f};
        if (ImGui::ColorEdit3("Cue green", cueGreen)) { setValue("tc_cue_green_r", cueGreen[0]*255.0f); setValue("tc_cue_green_g", cueGreen[1]*255.0f); setValue("tc_cue_green_b", cueGreen[2]*255.0f); }
    }

    ImGui::Separator();
    ImGui::TextWrapped(objective == 0
        ? "Fast Touch ignores both goals and selects the earliest useful reachable touch."
        : "Shoot selects a descending crossbar-height contact and evaluates the outgoing shot corridor.");

    ImGui::Separator();
    const int attempts = session_.attempts;
    const int touches = session_.touches;
    const int goals = session_.goals;
    std::vector<float> reactions = session_.reactions;
    float medianReaction = 0.0f;
    if (!reactions.empty())
    {
        std::sort(reactions.begin(), reactions.end());
        medianReaction = reactions[reactions.size() / 2];
    }
    ImGui::Text("Session: %d attempts | %d touches (%.1f%%) | %d goals (%.1f%%)",
        attempts, touches, attempts ? 100.0f * touches / attempts : 0.0f,
        goals, attempts ? 100.0f * goals / attempts : 0.0f);
    ImGui::Text("Avg |timing| %.0f ms | angle %.1f deg | median reaction %.0f ms",
        session_.gradedTimingCount ? session_.absTimingTotal / session_.gradedTimingCount : 0.0,
        session_.gradedAngleCount ? session_.absoluteAngleTotal / session_.gradedAngleCount : 0.0,
        medianReaction);
    ImGui::Text("Avg possible saved %.0f ms | touch height %.0f | contact speed %.0f",
        session_.possibleSavedCount ? session_.possibleSavedTotal / session_.possibleSavedCount : 0.0,
        touches ? session_.touchHeightTotal / touches : 0.0,
        touches ? session_.contactSpeedTotal / touches : 0.0);
    if (ImGui::Button("Reset session")) session_.reset();
}

void TakeoffCoach::renderSetupTab()
{
    ImGui::TextWrapped(
        "Each pair is a random range. Put both handles together for a fixed value.");

    int preset = getInt("tc_setup_preset");
    const char* presets[] = {"Shoot Setup", "Fast Touch Setup"};
    if (ImGui::Combo("Setup preset", &preset, presets, 2))
        applyPreset(preset);

    if (ImGui::Button("Load")) applyPreset(preset);
    ImGui::SameLine();
    if (ImGui::Button("Reset Shoot")) resetModePreset(0);
    ImGui::SameLine();
    if (ImGui::Button("Reset Fast Touch")) resetModePreset(1);

    ImGui::Separator();
    ImGui::Text("POSITION");

    rangeControl("Approach distance", "tc_distance_min", "tc_distance_max",
        -1800.0f, 6000.0f, "%.0f uu");
    ImGui::TextWrapped("Negative allows the ball to start behind the car; zero allows overhead setups.");
    rangeControl("Lateral offset", "tc_lateral_min", "tc_lateral_max",
        -2200.0f, 2200.0f, "%.0f uu");
    rangeControl("Ball-centre height", "tc_ball_height_min", "tc_ball_height_max",
        93.0f, 2044.0f, "%.0f uu");
    rangeControl("Target ball-centre contact height", "tc_target_height_min", "tc_target_height_max",
        93.0f, 2044.0f, "%.0f uu");
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

    bool showContactMarker =
        getBool("tc_show_contact_marker");

    if (ImGui::Checkbox(
            "Show future crossbar-height marker",
            &showContactMarker))
    {
        setValue(
            "tc_show_contact_marker",
            showContactMarker);
    }

    float markerColor[3] = {
        getFloat("tc_marker_color_r") / 255.0f,
        getFloat("tc_marker_color_g") / 255.0f,
        getFloat("tc_marker_color_b") / 255.0f};

    if (ImGui::ColorEdit3(
            "Future marker color",
            markerColor))
    {
        setValue(
            "tc_marker_color_r",
            markerColor[0] * 255.0f);

        setValue(
            "tc_marker_color_g",
            markerColor[1] * 255.0f);

        setValue(
            "tc_marker_color_b",
            markerColor[2] * 255.0f);
    }

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
        "Green ball-centre contact-height band",
        "tc_target_height_min",
        "tc_target_height_max",
        93.0f,
        2044.0f,
        "%.0f uu");

    float heightYellow = getFloat("tc_height_yellow_margin");
    if (ImGui::SliderFloat("Yellow height margin", &heightYellow, 20.0f, 600.0f, "%.0f uu"))
        setValue("tc_height_yellow_margin", heightYellow);

    float heightDisplay = getFloat("tc_height_display_margin");
    if (ImGui::SliderFloat("Full height display margin", &heightDisplay, 100.0f, 1200.0f, "%.0f uu"))
        setValue("tc_height_display_margin", heightDisplay);
    ImGui::Separator();
    ImGui::Text("INDICATOR COLORS");

    float redColor[3] = {
        getFloat("tc_color_red_r") / 255.0f,
        getFloat("tc_color_red_g") / 255.0f,
        getFloat("tc_color_red_b") / 255.0f};

    if (ImGui::ColorEdit3("Red zones", redColor))
    {
        setValue("tc_color_red_r", redColor[0] * 255.0f);
        setValue("tc_color_red_g", redColor[1] * 255.0f);
        setValue("tc_color_red_b", redColor[2] * 255.0f);
    }

    float yellowColor[3] = {
        getFloat("tc_color_yellow_r") / 255.0f,
        getFloat("tc_color_yellow_g") / 255.0f,
        getFloat("tc_color_yellow_b") / 255.0f};

    if (ImGui::ColorEdit3("Yellow zones", yellowColor))
    {
        setValue("tc_color_yellow_r", yellowColor[0] * 255.0f);
        setValue("tc_color_yellow_g", yellowColor[1] * 255.0f);
        setValue("tc_color_yellow_b", yellowColor[2] * 255.0f);
    }

    float greenColor[3] = {
        getFloat("tc_color_green_r") / 255.0f,
        getFloat("tc_color_green_g") / 255.0f,
        getFloat("tc_color_green_b") / 255.0f};

    if (ImGui::ColorEdit3("Green zones", greenColor))
    {
        setValue("tc_color_green_r", greenColor[0] * 255.0f);
        setValue("tc_color_green_g", greenColor[1] * 255.0f);
        setValue("tc_color_green_b", greenColor[2] * 255.0f);
    }

    if (ImGui::Button("Reset to red / yellow / green"))
    {
        setValue("tc_color_red_r", 200.0f);
        setValue("tc_color_red_g", 55.0f);
        setValue("tc_color_red_b", 55.0f);

        setValue("tc_color_yellow_r", 235.0f);
        setValue("tc_color_yellow_g", 175.0f);
        setValue("tc_color_yellow_b", 45.0f);

        setValue("tc_color_green_r", 55.0f);
        setValue("tc_color_green_g", 195.0f);
        setValue("tc_color_green_b", 95.0f);
    }

}

void TakeoffCoach::renderAdvancedTab()
{
    float solverHz = getFloat("tc_solver_hz");
    if (ImGui::SliderFloat("Solver frequency", &solverHz, 5.0f, 60.0f, "%.0f Hz")) setValue("tc_solver_hz", solverHz);
    float dt = getFloat("tc_prediction_timestep");
    if (ImGui::SliderFloat("Prediction timestep", &dt, 1.0f/240.0f, 1.0f/30.0f, "%.4f s")) setValue("tc_prediction_timestep", dt);
    float horizon = getFloat("tc_prediction_horizon");
    if (ImGui::SliderFloat("Prediction horizon", &horizon, 1.0f, 8.0f, "%.1f s")) setValue("tc_prediction_horizon", horizon);
    float rest = getFloat("tc_ball_restitution");
    if (ImGui::SliderFloat("Restitution", &rest, 0.1f, 1.0f, "%.2f")) setValue("tc_ball_restitution", rest);
    float damp = getFloat("tc_tangential_damping");
    if (ImGui::SliderFloat("Tangential damping", &damp, 0.70f, 1.0f, "%.3f")) setValue("tc_tangential_damping", damp);
    bool allowPostBounce = getBool("tc_allow_post_bounce_fallback");
    if (ImGui::Checkbox("Allow post-bounce fallback", &allowPostBounce)) setValue("tc_allow_post_bounce_fallback", allowPostBounce);

    float settle = getFloat("tc_initial_settle_time");
    if (ImGui::SliderFloat("Initial settle time", &settle, 0.0f, 1.5f, "%.2f s")) setValue("tc_initial_settle_time", settle);
    float verify = getFloat("tc_setup_validation_delay");
    if (ImGui::SliderFloat("Verification window", &verify, 0.0f, 2.5f, "%.2f s")) setValue("tc_setup_validation_delay", verify);
    float stable = getFloat("tc_validation_stable_time");
    if (ImGui::SliderFloat("Required stable time", &stable, 0.0f, 1.5f, "%.2f s")) setValue("tc_validation_stable_time", stable);
    float posTol = getFloat("tc_ball_path_position_tolerance");
    if (ImGui::SliderFloat("Ball path position tolerance", &posTol, 20.0f, 500.0f, "%.0f uu")) setValue("tc_ball_path_position_tolerance", posTol);
    float velTol = getFloat("tc_ball_path_velocity_tolerance");
    if (ImGui::SliderFloat("Ball path velocity tolerance", &velTol, 10.0f, 800.0f, "%.0f uu/s")) setValue("tc_ball_path_velocity_tolerance", velTol);
    float carTol=getFloat("tc_car_contact_tolerance"); if(ImGui::SliderFloat("Car contact tolerance",&carTol,30.0f,300.0f,"%.0f uu"))setValue("tc_car_contact_tolerance",carTol);
    float markerTol=getFloat("tc_takeoff_marker_tolerance"); if(ImGui::SliderFloat("Takeoff marker tolerance",&markerTol,20.0f,300.0f,"%.0f uu"))setValue("tc_takeoff_marker_tolerance",markerTol);

    if (ImGui::CollapsingHeader("Diagnostics"))
    {
        ImGui::Text("Validation state: %d", static_cast<int>(attempt_.validationState));
        ImGui::Text("Solver backend: %s", attempt_.solverBackend.c_str());
        ImGui::Text("Ball path slices: %d", static_cast<int>(attempt_.ballPath.size()));
        ImGui::Text("Candidates tested/reachable: %d / %d", attempt_.targetCandidatesTested, attempt_.reachableCandidates);
        ImGui::Text("Profile simulations: %d", attempt_.profileSimulations);
        ImGui::Text("Solve duration: %.2f ms", attempt_.lastSolveDurationMs);
        ImGui::Text("Target locked: %s", attempt_.targetLocked ? "yes" : "no");
        ImGui::Text("Cue state: %s", attempt_.cueTriggered ? "GREEN" : "RED");
        ImGui::Text("Cue time/current: %.3f / %.3f", attempt_.cueTriggered ? attempt_.frozenCueTime : attempt_.cueTime, nowSeconds());
        ImGui::Text("Timing: %s", attempt_.timingFrozen ? "frozen" : "live");
        ImGui::Text("Target contact / ideal jump: %.3f / %.3f", attempt_.lockedTarget.contactAbsoluteTime, attempt_.solution.idealJumpAbsolute);
        ImGui::Text("Last failure: %s", attempt_.lastFailureReason.c_str());
        auto diagnosticCar = gameWrapper->GetLocalCar();
        auto diagnosticServer = gameWrapper->GetCurrentGameState();
        if (!diagnosticCar.IsNull())
        {
            const Vector f = normalized2D(forwardFromRotator(diagnosticCar.GetRotation()));
            const Vector target = attempt_.targetLocked ? attempt_.lockedTarget.ballPosition : attempt_.candidateTarget.ballPosition;
            const Vector to = normalized2D(target - diagnosticCar.GetLocation());
            ImGui::Text("Car forward: %.2f %.2f %.2f", f.X, f.Y, f.Z);
            ImGui::Text("Vector to ball target: %.2f %.2f %.2f", to.X, to.Y, to.Z);
            ImGui::Text("Signed angle: %.2f deg", signedAngleDeg2D(f,to));
            ImGui::Text("Angle target source: ball-centre");
        }
        if (!diagnosticServer.IsNull())
        {
            auto diagnosticBall = diagnosticServer.GetBall();
            if (!diagnosticBall.IsNull())
                ImGui::Text("Real ball height / target / frozen: %.1f / %.1f / %.1f uu",
                    diagnosticBall.GetLocation().Z, attempt_.lockedTarget.ballPosition.Z, attempt_.contactHeight);
        }
    }

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
    return getInt("tc_objective") == 1 ? Objective::Shoot : Objective::FastTouch;
}

std::string TakeoffCoach::objectiveName(Objective objective)
{
    return objective == Objective::Shoot ? "SHOOT" : "FAST TOUCH";
}

float TakeoffCoach::clamp(float value, float low, float high)
{
    return std::max(low, std::min(value, high));
}

float TakeoffCoach::dot(const Vector& a, const Vector& b)
{
    return a.X*b.X+a.Y*b.Y+a.Z*b.Z;
}

Vector TakeoffCoach::cross(const Vector& a, const Vector& b)
{
    return Vector{a.Y*b.Z-a.Z*b.Y,a.Z*b.X-a.X*b.Z,a.X*b.Y-a.Y*b.X};
}

Vector TakeoffCoach::normalized(const Vector& value)
{
    const float m=length(value); return m<0.001f?Vector{1,0,0}:value*(1.0f/m);
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

