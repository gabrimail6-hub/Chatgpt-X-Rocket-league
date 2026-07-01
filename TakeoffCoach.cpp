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
    "5.0.0 Vector",
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

    cvarManager->log("Takeoff Coach 5.0 Vector loaded.");
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

    reg("tc_objective", "1", 0.0f, 1.0f);
    reg("tc_setup_preset", "0", 0.0f, 2.0f);
    reg("tc_guidance_style", "0", 0.0f, 1.0f);
    reg("tc_reaction_allowance_ms", "100", 0.0f, 400.0f);
    reg("tc_hide_guidance_before_cue", "1", 0.0f, 1.0f);
    reg("tc_cue_size", "44", 18.0f, 120.0f);
    reg("tc_cue_position_x", "0.50", 0.0f, 1.0f);
    reg("tc_cue_position_y", "0.42", 0.0f, 1.0f);
    reg("tc_cue_red_r", "255", 0.0f, 255.0f);
    reg("tc_cue_red_g", "40", 0.0f, 255.0f);
    reg("tc_cue_red_b", "40", 0.0f, 255.0f);
    reg("tc_cue_green_r", "0", 0.0f, 255.0f);
    reg("tc_cue_green_g", "255", 0.0f, 255.0f);
    reg("tc_cue_green_b", "67", 0.0f, 255.0f);
    reg("tc_cue_flash", "1", 0.0f, 1.0f);
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
    reg("tc_contact_max", "2.35", 0.8f, 3.5f);
    reg("tc_solver_hz", "20", 5.0f, 60.0f);
    reg("tc_prediction_timestep", "0.008333", 0.004167f, 0.033333f);
    reg("tc_prediction_horizon", "4.0", 1.0f, 8.0f);
    reg("tc_ball_restitution", "0.60", 0.10f, 1.00f);
    reg("tc_tangential_damping", "0.985", 0.70f, 1.00f);
    reg("tc_bounce_policy", "0", 0.0f, 3.0f);
    reg("tc_max_bounces", "1", 0.0f, 8.0f);
    reg("tc_prefer_prebounce", "1", 0.0f, 1.0f);
    reg("tc_reject_unsupported_geometry", "1", 0.0f, 1.0f);
    reg("tc_initial_settle_time", "0.15", 0.0f, 1.5f);
    reg("tc_velocity_tolerance", "120", 10.0f, 800.0f);
    reg("tc_allowed_failed_samples", "3", 0.0f, 30.0f);
    reg("tc_max_path_corrections", "3", 0.0f, 20.0f);
    reg("tc_profile_single_jump", "1", 0.0f, 1.0f);
    reg("tc_profile_fast_aerial", "1", 0.0f, 1.0f);
    reg("tc_profile_delayed_second", "1", 0.0f, 1.0f);
    reg("tc_profile_conservative", "1", 0.0f, 1.0f);
    reg("tc_robustness_margin", "0.08", 0.0f, 0.5f);
    reg("tc_horizontal_calibration", "0.76", 0.4f, 1.2f);
    reg("tc_vertical_calibration", "0.78", 0.4f, 1.2f);
    reg("tc_position_tolerance", "150", 80.0f, 300.0f);
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
    reg("tc_shoot_score_speed", "1", 0.0f, 1.0f);
    reg("tc_shoot_score_aim", "1", 0.0f, 1.0f);
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

    // Persistent user-editable setup. Built-in Shoot Default and Fast Touch Default presets
    // stay recoverable; only this Custom slot is overwritten by Save Custom.
    reg("tc_custom_distance_min", "1200", -1800.0f, 6000.0f);
    reg("tc_custom_distance_max", "3000", -1800.0f, 6000.0f);
    reg("tc_custom_lateral_min", "-900", -2200.0f, 2200.0f);
    reg("tc_custom_lateral_max", "900", -2200.0f, 2200.0f);
    reg("tc_custom_ball_height_min", "120", 93.0f, 2044.0f);
    reg("tc_custom_ball_height_max", "1500", 93.0f, 2044.0f);
    reg("tc_custom_target_height_min", "600", 93.0f, 2044.0f);
    reg("tc_custom_target_height_max", "685", 93.0f, 2044.0f);
    reg("tc_custom_setup_rotation_min", "90", -180.0f, 180.0f);
    reg("tc_custom_setup_rotation_max", "90", -180.0f, 180.0f);
    reg("tc_custom_car_facing_min", "-15", -90.0f, 90.0f);
    reg("tc_custom_car_facing_max", "15", -90.0f, 90.0f);
    reg("tc_custom_car_speed_min", "1500", 0.0f, 2300.0f);
    reg("tc_custom_car_speed_max", "2300", 0.0f, 2300.0f);
    reg("tc_custom_car_velocity_angle_min", "-10", -120.0f, 120.0f);
    reg("tc_custom_car_velocity_angle_max", "10", -120.0f, 120.0f);
    reg("tc_custom_ball_speed_min", "500", 0.0f, 2200.0f);
    reg("tc_custom_ball_speed_max", "1500", 0.0f, 2200.0f);
    reg("tc_custom_ball_vertical_min", "1000", -1000.0f, 1800.0f);
    reg("tc_custom_ball_vertical_max", "1600", -1000.0f, 1800.0f);
    reg("tc_custom_ball_direction_min", "-45", -180.0f, 180.0f);
    reg("tc_custom_ball_direction_max", "45", -180.0f, 180.0f);

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
    attempt_.objective = scenario.objective;
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
    attempt_.invalidNonGroundBounce = false;
    attempt_.targetLocked = false;

    std::uniform_real_distribution<float> unreachableRoll(0.0f, 100.0f);
    attempt_.allowUnreachable =
        unreachableRoll(rng_) < getFloat("tc_unreachable_setup_chance");
    attempt_.startedAt = nowSeconds();
    attempt_.settledAt = attempt_.startedAt + getFloat("tc_initial_settle_time");
    attempt_.validationDeadline = attempt_.settledAt + getFloat("tc_setup_validation_delay");
    attempt_.ballPath.clear();
    attempt_.candidateTarget = ContactTarget{};
    attempt_.lockedTarget = ContactTarget{};
    attempt_.failedValidationSamples = 0;
    attempt_.pathCorrections = 0;
    attempt_.validationStableSince = 0.0f;
    attempt_.reactionCaptured = false;
    attempt_.cueShown = false;
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

    Vector p = scenario.ballPosition;
    Vector v = scenario.ballVelocity;
    int bounces = 0;
    const float dt = clamp(getFloat("tc_prediction_timestep"), 1.0f / 240.0f, 1.0f / 30.0f);
    for (float t = 0.0f; t < getFloat("tc_contact_max"); t += dt)
    {
        v.Z += GRAVITY_Z * dt;
        p = p + v * dt;
        SurfaceType surface = SurfaceType::None;
        if (collideBall(p, v, 93.0f, surface)) ++bounces;
        if (bounces > getInt("tc_max_bounces")) break;
        if (std::abs(p.X) > 5000.0f || std::abs(p.Y) > 6500.0f || p.Z < 0.0f || p.Z > 2200.0f)
            return false;
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
        attempt_.reactionCaptured = true;
        attempt_.reactionRawMs = (now - attempt_.cueTime) * 1000.0f;
        attempt_.reactionAfterAllowanceMs = attempt_.reactionRawMs - getFloat("tc_reaction_allowance_ms");

        if (attempt_.solution.valid)
            attempt_.jumpSolution = attempt_.solution;
        else if (attempt_.everHadSolution)
            attempt_.jumpSolution = attempt_.lastValidSolution;
        else
            attempt_.jumpSolution = Solution{};

        attempt_.phase = Phase::Airborne;

        if (attempt_.jumpSolution.valid)
        {
            attempt_.timingErrorMs =
                (now - attempt_.jumpSolution.idealJumpAbsolute) * 1000.0f;
            attempt_.alignmentErrorDeg = signedAngleDeg2D(
                normalized2D(forwardFromRotator(car.GetRotation())),
                attempt_.lockedTarget.desiredFacingDirection);
            attempt_.jumpSolution.target = attempt_.lockedTarget;
            attempt_.jumpSolution.requiredDirection = attempt_.lockedTarget.desiredFacingDirection;
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

    const Vector currentBallVelocity =
        ball.GetVelocity();

    const Vector currentBallPosition =
        ball.GetLocation();

    const Vector velocityDelta =
        currentBallVelocity
        - attempt_.previousBallVelocity;

    const bool sharpVelocityChange =
        length(velocityDelta) > 250.0f;

    const bool nearSideWall =
        std::abs(currentBallPosition.X) > 3800.0f;

    const bool nearBackWall =
        std::abs(currentBallPosition.Y) > 4900.0f;

    const bool nearCeiling =
        currentBallPosition.Z > 1900.0f;

    const bool nearGround =
        currentBallPosition.Z < 150.0f;

    if (sharpVelocityChange
        && !nearGround
        && (nearSideWall || nearBackWall || nearCeiling))
    {
        attempt_.invalidNonGroundBounce = true;
    }

    attempt_.previousBallVelocity =
        currentBallVelocity;

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

void TakeoffCoach::updateReading(
    CarWrapper car,
    BallWrapper ball,
    float now)
{
    const float hz = std::max(5.0f, getFloat("tc_solver_hz"));
    const float distanceNow = length(ball.GetLocation() - car.GetLocation());
    attempt_.closestDistance = std::min(attempt_.closestDistance, distanceNow);

    if (now < attempt_.settledAt)
    {
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
                attempt_.ballPath = buildBallPath(
                    attempt_.snapshotBallPosition,
                    attempt_.snapshotBallVelocity,
                    ball.GetAngularVelocity(), now);
                attempt_.candidateTarget = selectContactTarget(car, attempt_.ballPath, now);
                attempt_.validationCandidate = solveTargetWithProfiles(car, attempt_.candidateTarget, now);
                attempt_.solution = attempt_.validationCandidate;
                attempt_.initialCandidateTime = now;
                attempt_.initialAvailableJumpDelay = attempt_.validationCandidate.jumpDelay;
                attempt_.cueTime = attempt_.validationCandidate.idealJumpAbsolute;
                if (attempt_.validationCandidate.valid &&
                    attempt_.initialAvailableJumpDelay < getFloat("tc_min_initial_jump_delay_ms") / 1000.0f)
                    attempt_.validationCandidate = Solution{};
            }

            BallPredictionSlice expected;
            const bool sampled = samplePath(now, expected);
            const float positionError = sampled ? length(ball.GetLocation() - expected.position) : 99999.0f;
            const float velocityError = sampled ? length(ball.GetVelocity() - expected.velocity) : 99999.0f;
            const bool pathMatches = sampled &&
                positionError <= getFloat("tc_position_tolerance") &&
                velocityError <= getFloat("tc_velocity_tolerance");

            if (pathMatches)
            {
                attempt_.failedValidationSamples = 0;
                if (attempt_.validationStableSince <= 0.0f)
                    attempt_.validationStableSince = now;
            }
            else
            {
                ++attempt_.failedValidationSamples;
                attempt_.validationStableSince = 0.0f;
                if (attempt_.failedValidationSamples > getInt("tc_allowed_failed_samples") &&
                    attempt_.pathCorrections < getInt("tc_max_path_corrections"))
                {
                    ++attempt_.pathCorrections;
                    attempt_.failedValidationSamples = 0;
                    attempt_.ballPath = buildBallPath(ball.GetLocation(), ball.GetVelocity(), ball.GetAngularVelocity(), now);
                    attempt_.candidateTarget = selectContactTarget(car, attempt_.ballPath, now);
                    attempt_.validationCandidate = solveTargetWithProfiles(car, attempt_.candidateTarget, now);
                    attempt_.solution = attempt_.validationCandidate;
                    attempt_.cueTime = attempt_.validationCandidate.idealJumpAbsolute;
                }
            }

            const bool stable = attempt_.validationStableSince > 0.0f &&
                now - attempt_.validationStableSince >= getFloat("tc_validation_stable_time");
            const bool windowDone = now >= attempt_.validationDeadline;
            if (stable && windowDone && attempt_.validationCandidate.valid)
            {
                attempt_.lockedTarget = attempt_.candidateTarget;
                attempt_.lockedSolution = attempt_.validationCandidate;
                attempt_.solution = attempt_.lockedSolution;
                attempt_.lastValidSolution = attempt_.lockedSolution;
                attempt_.everHadSolution = true;
                attempt_.targetLocked = true;
                attempt_.lockedContactAbsolute = attempt_.lockedTarget.contactAbsoluteTime;
                attempt_.cueTime = attempt_.lockedSolution.idealJumpAbsolute;
                consecutiveRerolls_ = 0;
            }

            if (windowDone && !attempt_.targetLocked &&
                attempt_.pathCorrections >= getInt("tc_max_path_corrections") &&
                !attempt_.allowUnreachable)
            {
                requestNewScenario();
                return;
            }
        }
        else
        {
            attempt_.solution = solveLockedTarget(car, now);
            attempt_.lastValidSolution = attempt_.solution;
        }
    }

    if (!attempt_.targetLocked)
    {
        attempt_.headline = objectiveName(attempt_.objective) + " | VERIFYING PATH";
        return;
    }

    const bool reactionCue = attempt_.guidanceStyle == GuidanceStyle::ReactionCue;
    if (reactionCue && now < attempt_.cueTime)
        attempt_.headline = objectiveName(attempt_.objective) + " | WAIT FOR GREEN";
    else if (attempt_.solution.jumpDelay > getFloat("tc_timing_green_early_ms") / 1000.0f)
        attempt_.headline = objectiveName(attempt_.objective) + " | WAIT";
    else if (attempt_.solution.jumpDelay >= -getFloat("tc_timing_green_late_ms") / 1000.0f)
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

    const bool newTouch = nearBall && ballChanged && !attempt_.touched;
    const bool scored =
        std::abs(ballPosition.Y) > 5120.0f
        && std::abs(ballPosition.X) < 900.0f
        && ballPosition.Z < 650.0f;

    if (newTouch)
    {
        attempt_.touched = true;
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
        ? attempt_.jumpAt + 5.0f
        : (attempt_.jumpSolution.valid
            ? attempt_.jumpSolution.idealJumpAbsolute + attempt_.jumpSolution.contactDelay + 0.8f
            : attempt_.jumpAt + 2.8f);

    if (now > deadline)
        finishAttempt(car, ball, attempt_.touched, false, now);
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

    if (attempt_.invalidNonGroundBounce)
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
        attempt_.possibleTimeSavedMs = std::max(0.0f, attempt_.reactionAfterAllowanceMs);
        attempt_.correction += " Reaction loss after allowance: "
            + format0(attempt_.reactionAfterAllowanceMs) + " ms.";
        if (touched)
            attempt_.correction += " Optimal route could have contacted about "
                + format0(attempt_.possibleTimeSavedMs) + " ms sooner.";
        else
            attempt_.correction += " Closest approach: " + format0(attempt_.closestDistance) + " uu.";
        session_.reactions.push_back(attempt_.reactionAfterAllowanceMs);
    }

    if (touched) ++session_.touches;
    if (scored) ++session_.goals;
    if (timing < 9000.0f) session_.absTimingTotal += std::abs(timing);
    if (alignment < 9000.0f) session_.angleTotal += alignment;
    session_.possibleSavedTotal += attempt_.possibleTimeSavedMs;
    if (touched)
    {
        session_.touchHeightTotal += attempt_.contactHeight;
        session_.contactSpeedTotal += length(car.GetVelocity());
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
    Vector position,
    Vector velocity,
    Vector angularVelocity,
    float now) const
{
    std::vector<BallPredictionSlice> path;
    const float dt = clamp(getFloat("tc_prediction_timestep"), 1.0f / 240.0f, 1.0f / 30.0f);
    const float horizon = clamp(getFloat("tc_prediction_horizon"), 1.0f, 8.0f);
    const float radius = 93.0f;
    int bounceCount = 0;

    for (float t = 0.0f; t <= horizon + 0.5f * dt; t += dt)
    {
        BallPredictionSlice slice;
        slice.absoluteTime = now + t;
        slice.position = position;
        slice.velocity = velocity;
        slice.angularVelocity = angularVelocity;
        slice.bounceCount = bounceCount;
        path.push_back(slice);

        velocity.Z += GRAVITY_Z * dt;
        position = position + velocity * dt;

        SurfaceType surface = SurfaceType::None;
        if (collideBall(position, velocity, radius, surface))
        {
            ++bounceCount;
            path.back().surface = surface;
            path.back().bounceCount = bounceCount;
            if (bounceCount > getInt("tc_max_bounces"))
                break;
        }
    }
    return path;
}

bool TakeoffCoach::collideBall(
    Vector& p,
    Vector& v,
    float r,
    SurfaceType& surface) const
{
    const float restitution = getFloat("tc_ball_restitution");
    const float tangent = getFloat("tc_tangential_damping");
    const float side = 4096.0f - r;
    const float back = 5120.0f - r;
    const float ceiling = 2044.0f - r;
    const float goalHalfWidth = 893.0f - r;
    const float goalHeight = 642.0f - r;
    const float goalDepth = 880.0f;
    bool hit = false;

    auto reflect = [&](Vector n)
    {
        const float normalSpeed = v.X * n.X + v.Y * n.Y + v.Z * n.Z;
        if (normalSpeed < 0.0f)
            v = (v - n * ((1.0f + restitution) * normalSpeed)) * tangent;
        hit = true;
    };

    if (p.Z < r)
    {
        p.Z = r;
        surface = std::abs(p.Y) > 5120.0f ? SurfaceType::GoalFloor : SurfaceType::Ground;
        reflect(Vector{0.0f, 0.0f, 1.0f});
    }
    else if (p.Z > ceiling)
    {
        p.Z = ceiling;
        surface = std::abs(p.Y) > 5120.0f ? SurfaceType::GoalCeiling : SurfaceType::Ceiling;
        reflect(Vector{0.0f, 0.0f, -1.0f});
    }

    if (std::abs(p.X) > side)
    {
        const float sign = p.X > 0.0f ? 1.0f : -1.0f;
        p.X = sign * side;
        surface = SurfaceType::SideWall;
        reflect(Vector{-sign, 0.0f, 0.0f});
    }

    const bool insideGoalMouth = std::abs(p.X) < goalHalfWidth && p.Z < goalHeight;
    if (std::abs(p.Y) > back && !insideGoalMouth)
    {
        const float sign = p.Y > 0.0f ? 1.0f : -1.0f;
        p.Y = sign * back;
        surface = SurfaceType::BackWall;
        reflect(Vector{0.0f, -sign, 0.0f});
    }
    else if (std::abs(p.Y) > 5120.0f + goalDepth - r)
    {
        const float sign = p.Y > 0.0f ? 1.0f : -1.0f;
        p.Y = sign * (5120.0f + goalDepth - r);
        surface = SurfaceType::GoalBackWall;
        reflect(Vector{0.0f, -sign, 0.0f});
    }

    // Approximate diagonal corner plane and rounded floor/wall transitions.
    const float ax = std::abs(p.X);
    const float ay = std::abs(p.Y);
    if (ax > 3500.0f && ay > 4300.0f && ax + ay > 7900.0f)
    {
        const float sx = p.X > 0.0f ? 1.0f : -1.0f;
        const float sy = p.Y > 0.0f ? 1.0f : -1.0f;
        Vector n{-0.7071067f * sx, -0.7071067f * sy, 0.0f};
        surface = SurfaceType::DiagonalCorner;
        reflect(n);
        p.X -= sx * 8.0f;
        p.Y -= sy * 8.0f;
    }

    // Approximate posts and crossbar as cylinders/spheres around the goal mouth.
    for (float sx : {-1.0f, 1.0f})
    {
        const float postX = sx * 893.0f;
        for (float sy : {-1.0f, 1.0f})
        {
            const float postY = sy * 5120.0f;
            Vector d{p.X - postX, p.Y - postY, 0.0f};
            const float d2 = d.X * d.X + d.Y * d.Y;
            const float rr = r + 75.0f;
            if (p.Z < 650.0f && d2 < rr * rr && d2 > 1.0f)
            {
                const float inv = 1.0f / std::sqrt(d2);
                Vector n{d.X * inv, d.Y * inv, 0.0f};
                surface = SurfaceType::GoalPost;
                reflect(n);
                p = Vector{postX + n.X * rr, postY + n.Y * rr, p.Z};
            }
        }
    }

    if (std::abs(std::abs(p.Y) - 5120.0f) < r + 85.0f &&
        std::abs(p.X) < 893.0f && std::abs(p.Z - 642.0f) < r + 85.0f)
    {
        const float sy = p.Y > 0.0f ? 1.0f : -1.0f;
        Vector d{0.0f, p.Y - sy * 5120.0f, p.Z - 642.0f};
        const float len = std::sqrt(d.Y * d.Y + d.Z * d.Z);
        if (len > 1.0f)
        {
            Vector n{0.0f, d.Y / len, d.Z / len};
            surface = SurfaceType::Crossbar;
            reflect(n);
        }
    }
    return hit;
}

Vector TakeoffCoach::chooseGoalPoint(const ContactTarget& base, Vector carPosition) const
{
    const float goalY = static_cast<float>(attempt_.scenario.goalSign) * 5120.0f;
    const int mode = getInt("tc_shoot_aim_mode");
    float x = 0.0f;
    if (mode == static_cast<int>(AimMode::NearPost))
        x = carPosition.X >= 0.0f ? 650.0f : -650.0f;
    else if (mode == static_cast<int>(AimMode::FarPost))
        x = carPosition.X >= 0.0f ? -650.0f : 650.0f;
    else if (mode == static_cast<int>(AimMode::RandomCorridor))
        x = std::sin(base.contactAbsoluteTime * 13.37f) * 650.0f;
    else if (mode == static_cast<int>(AimMode::Custom))
        x = getFloat("tc_shoot_custom_aim_x");
    return Vector{x, goalY, getFloat("tc_shoot_goal_height")};
}

TakeoffCoach::ContactTarget TakeoffCoach::selectContactTarget(
    CarWrapper car,
    const std::vector<BallPredictionSlice>& path,
    float now) const
{
    ContactTarget best;
    const float minContact = getFloat("tc_contact_min");
    const float maxContact = getFloat("tc_contact_max");
    float minHeight = getFloat("tc_target_height_min");
    float maxHeight = getFloat("tc_target_height_max");
    if (minHeight > maxHeight) std::swap(minHeight, maxHeight);
    const BouncePolicy policy = static_cast<BouncePolicy>(getInt("tc_bounce_policy"));
    const bool preferPrebounce = getBool("tc_prefer_prebounce");
    float bestScore = 1.0e9f;

    for (const BallPredictionSlice& slice : path)
    {
        const float delay = slice.absoluteTime - now;
        if (delay < minContact || delay > maxContact) continue;
        if (slice.position.Z < minHeight || slice.position.Z > maxHeight) continue;
        if (attempt_.objective == Objective::Shoot && slice.velocity.Z >= 0.0f) continue;
        if (slice.bounceCount > getInt("tc_max_bounces")) continue;

        bool bounceAllowed = slice.bounceCount == 0;
        if (slice.bounceCount > 0)
        {
            const bool ground = slice.surface == SurfaceType::Ground || slice.surface == SurfaceType::GoalFloor;
            const bool wall = !ground;
            bounceAllowed = policy == BouncePolicy::Any ||
                (policy == BouncePolicy::GroundOnly && ground) ||
                (policy == BouncePolicy::WallOnly && wall);
        }
        if (!bounceAllowed) continue;

        ContactTarget target;
        target.valid = true;
        target.contactAbsoluteTime = slice.absoluteTime;
        target.ballPosition = slice.position;
        target.ballVelocity = slice.velocity;
        target.bounceCount = slice.bounceCount;
        target.lastSurface = slice.surface;

        Vector desiredDirection;
        if (attempt_.objective == Objective::Shoot)
        {
            target.desiredGoalPoint = chooseGoalPoint(target, car.GetLocation());
            desiredDirection = normalized2D(target.desiredGoalPoint - target.ballPosition);
            target.predictedOutgoingVelocity = desiredDirection * std::max(1200.0f, length(slice.velocity) + 500.0f);
            const Vector goalDirection = normalized2D(target.desiredGoalPoint - target.ballPosition);
            target.shotAimErrorDeg = std::abs(signedAngleDeg2D(desiredDirection, goalDirection));
            target.predictedShotSpeed = length(target.predictedOutgoingVelocity);
            if (target.shotAimErrorDeg > getFloat("tc_shoot_max_aim_error")) continue;
        }
        else
        {
            desiredDirection = normalized2D(target.ballPosition - car.GetLocation());
        }

        target.desiredFacingDirection = desiredDirection;
        target.desiredCarPosition = target.ballPosition - desiredDirection * 145.0f - Vector{0.0f, 0.0f, 45.0f};

        const float heightCentre = 0.5f * (minHeight + maxHeight);
        float score = delay + 0.001f * std::abs(slice.position.Z - heightCentre);
        if (attempt_.objective == Objective::Shoot)
            score = std::abs(slice.position.Z - heightCentre) * 0.01f + delay * 0.15f;
        if (preferPrebounce) score += 0.18f * slice.bounceCount;
        if (score < bestScore)
        {
            bestScore = score;
            best = target;
        }
    }
    return best;
}

bool TakeoffCoach::simulateAerialProfile(
    CarWrapper car,
    const ContactTarget& target,
    AerialProfile profile,
    float duration,
    Solution& out) const
{
    if (duration <= 0.0f) return false;
    const Vector start = car.GetLocation();
    const Vector initialVelocity = car.GetVelocity();
    const Vector delta = target.desiredCarPosition - start;
    const Vector facing = normalized2D(delta);
    const float dt = 1.0f / std::max(60.0f, getFloat("tc_solver_hz") * 4.0f);

    float secondJumpDelay = 999.0f;
    float jumpHold = 0.0f;
    float boostDelay = 0.0f;
    float pitchDelay = 0.0f;
    float pitchStrength = 1.0f;
    switch (profile)
    {
    case AerialProfile::SingleJump: jumpHold = 0.16f; boostDelay = 0.08f; pitchDelay = 0.08f; break;
    case AerialProfile::FastAerial: secondJumpDelay = 0.10f; jumpHold = 0.20f; boostDelay = 0.00f; pitchDelay = 0.03f; break;
    case AerialProfile::DelayedSecondJump: secondJumpDelay = 0.28f; jumpHold = 0.16f; boostDelay = 0.06f; pitchDelay = 0.08f; break;
    case AerialProfile::Conservative: secondJumpDelay = 0.18f; jumpHold = 0.12f; boostDelay = 0.12f; pitchDelay = 0.12f; pitchStrength = 0.82f; break;
    }

    Vector p = start;
    Vector v = initialVelocity + Vector{0.0f, 0.0f, FIRST_JUMP};
    bool secondApplied = false;
    float boostSeconds = 0.0f;
    for (float t = 0.0f; t < duration; t += dt)
    {
        if (!secondApplied && t >= secondJumpDelay)
        {
            v.Z += SECOND_JUMP;
            secondApplied = true;
        }
        if (t < jumpHold) v.Z += 1458.0f * dt;
        if (t >= boostDelay)
        {
            const float pitchBlend = clamp((t - pitchDelay) / 0.35f, 0.0f, 1.0f) * pitchStrength;
            Vector thrust = normalized2D(facing) * (1.0f - 0.25f * pitchBlend);
            thrust.Z = 0.25f + 0.75f * pitchBlend;
            v = v + thrust * (BOOST_ACCEL * dt);
            boostSeconds += dt;
        }
        v.Z += GRAVITY_Z * dt;
        p = p + v * dt;
    }

    const float error = length(target.desiredCarPosition - p);
    const float tolerance = getFloat("tc_position_tolerance");
    if (error > tolerance) return false;

    out.valid = true;
    out.profile = profile;
    out.requiredAerialDuration = duration;
    out.requiredBoost = boostSeconds * 33.3f;
    out.confidence = clamp(1.0f - error / std::max(1.0f, tolerance), 0.0f, 1.0f);
    out.robustness = clamp(out.confidence - getFloat("tc_robustness_margin"), 0.0f, 1.0f);
    out.idealTakeoffPosition = start + initialVelocity * std::max(0.0f, target.contactAbsoluteTime - nowSeconds() - duration);
    out.requiredDirection = target.desiredFacingDirection;
    out.contactPoint = target.ballPosition;
    out.target = target;
    return true;
}

TakeoffCoach::Solution TakeoffCoach::solveTargetWithProfiles(
    CarWrapper car,
    const ContactTarget& target,
    float now) const
{
    Solution best;
    const float available = target.contactAbsoluteTime - now;
    const AerialProfile profiles[] = {
        AerialProfile::SingleJump, AerialProfile::FastAerial,
        AerialProfile::DelayedSecondJump, AerialProfile::Conservative};

    for (AerialProfile profile : profiles)
    {
        if (profile == AerialProfile::SingleJump && !getBool("tc_profile_single_jump")) continue;
        if (profile == AerialProfile::FastAerial && !getBool("tc_profile_fast_aerial")) continue;
        if (profile == AerialProfile::DelayedSecondJump && !getBool("tc_profile_delayed_second")) continue;
        if (profile == AerialProfile::Conservative && !getBool("tc_profile_conservative")) continue;

        for (float duration = 0.28f; duration <= std::min(2.5f, available + 0.4f); duration += 1.0f / 120.0f)
        {
            Solution candidate;
            if (!simulateAerialProfile(car, target, profile, duration, candidate)) continue;
            const float jumpDelay = available - duration;
            candidate.jumpDelay = jumpDelay;
            candidate.contactDelay = available;
            candidate.idealJumpAbsolute = target.contactAbsoluteTime - duration;
            candidate.alignmentErrorDeg = signedAngleDeg2D(
                normalized2D(forwardFromRotator(car.GetRotation())),
                target.desiredFacingDirection);
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
    Solution live = solveTargetWithProfiles(car, attempt_.lockedTarget, now);
    if (!live.valid)
    {
        live = attempt_.lockedSolution;
        live.jumpDelay = live.idealJumpAbsolute - now;
        live.contactDelay = attempt_.lockedTarget.contactAbsoluteTime - now;
        live.alignmentErrorDeg = signedAngleDeg2D(
            normalized2D(forwardFromRotator(car.GetRotation())),
            attempt_.lockedTarget.desiredFacingDirection);
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
    const bool beforeCue = cueMode && attempt_.targetLocked && hudNow < attempt_.cueTime;
    const bool visibleGuidance = !(beforeCue && getBool("tc_hide_guidance_before_cue"));

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

    const Solution displayedSolution =
        jumped
        ? attempt_.jumpSolution
        : attempt_.solution;

    const float timingMs =
        displayedSolution.valid
        ? (
            jumped
            ? attempt_.timingErrorMs
            : -displayedSolution.jumpDelay * 1000.0f)
        : getFloat("tc_timing_display_late_ms");

    const float alignment =
        displayedSolution.valid
        ? (
            jumped
            ? attempt_.alignmentErrorDeg
            : displayedSolution.alignmentErrorDeg)
        : getFloat("tc_alignment_display");

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

    if (visibleGuidance && getBool("tc_show_timing"))
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

    if (visibleGuidance && getBool("tc_show_alignment"))
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

    if (visibleGuidance && getBool("tc_show_height"))
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

    if (cueMode && attempt_.targetLocked && !feedback)
    {
        const bool green = hudNow >= attempt_.cueTime;
        const float flash = getBool("tc_cue_flash") && green
            ? (0.70f + 0.30f * std::abs(std::sin((hudNow - attempt_.cueTime) * 18.0f))) : 1.0f;
        const int cueAlpha = static_cast<int>(255.0f * flash);
        setColor(canvas,
            getInt(green ? "tc_cue_green_r" : "tc_cue_red_r"),
            getInt(green ? "tc_cue_green_g" : "tc_cue_red_g"),
            getInt(green ? "tc_cue_green_b" : "tc_cue_red_b"), cueAlpha);
        const int cx = static_cast<int>(canvas.GetSize().X * getFloat("tc_cue_position_x"));
        const int cy = static_cast<int>(canvas.GetSize().Y * getFloat("tc_cue_position_y"));
        const int size = static_cast<int>(getFloat("tc_cue_size"));
        canvas.SetPosition(Vector2{cx - size / 2, cy - size / 2});
        canvas.FillBox(Vector2{size, size});
        canvas.SetPosition(Vector2{cx - size, cy + size / 2 + 8});
        canvas.DrawString(green ? "JUMP" : "WAIT", 1.25f, 1.25f, true);
    }

    if (visibleGuidance && getBool("tc_show_contact_marker") && visualTarget.valid && !feedback)
    {
        const Vector2 marker = canvas.Project(visualTarget.target.ballPosition);
        if (onScreen(marker))
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
                canvas.DrawLine(Vector2{marker.X - size, marker.Y}, Vector2{marker.X + size, marker.Y}, 3.0f);
                canvas.DrawLine(Vector2{marker.X, marker.Y - size}, Vector2{marker.X, marker.Y + size}, 3.0f);
            }
            else
                canvas.DrawRect(Vector2{marker.X - size, marker.Y - size}, Vector2{marker.X + size, marker.Y + size});

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
            if (onScreen(marker) && onScreen(gp)) canvas.DrawLine(marker, gp, 1.5f);
        }
    }

    if (visibleGuidance && visualTarget.valid && getBool("tc_show_takeoff_marker") && !feedback)
    {
        Vector takeoff = visualTarget.idealTakeoffPosition;
        takeoff.Z = 17.0f;
        const Vector direction = visualTarget.target.desiredFacingDirection;
        const Vector arrowEnd = takeoff + direction * getFloat("tc_takeoff_arrow_length");
        Vector2 p0 = canvas.Project(takeoff);
        Vector2 p1 = canvas.Project(arrowEnd);
        Vector2 target2 = canvas.Project(visualTarget.target.ballPosition);
        if (onScreen(p0))
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
    if (preset == 1)
        return "tc_fast_";
    return "tc_custom_";
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
    // The two built-ins are intentionally immutable. Saving always targets Custom.
    const std::string prefix = modePrefix(2);
    for (const auto& field : presetFields())
        setValue(prefix + field.first, getFloat(field.second));
    setValue("tc_setup_preset", 2);
}

void TakeoffCoach::loadModePreset(int preset)
{
    const std::string prefix = modePrefix(preset);
    for (const auto& field : presetFields())
        setValue(field.second, getFloat(prefix + field.first));
}

void TakeoffCoach::applyPreset(int preset)
{
    preset = std::max(0, std::min(preset, 2));
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
    else
    {
        // Reset Custom to the Shoot defaults, then keep it independently editable.
        resetModePreset(0);
        loadModePreset(0);
        saveModePreset(2);
    }
    applyPreset(preset);
}

void TakeoffCoach::renderDrillTab()
{
    int objective = getInt("tc_objective");
    const char* objectives[] = {"Fast Touch", "Shoot"};
    if (ImGui::Combo("Objective", &objective, objectives, 2))
    {
        setValue("tc_objective", objective);
        applyPreset(objective == 1 ? 0 : 1);
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
        bool hide = getBool("tc_hide_guidance_before_cue");
        if (ImGui::Checkbox("Hide timing/angle/markers before cue", &hide)) setValue("tc_hide_guidance_before_cue", hide);
        float cueSize = getFloat("tc_cue_size");
        if (ImGui::SliderFloat("Cue size", &cueSize, 18.0f, 120.0f, "%.0f px")) setValue("tc_cue_size", cueSize);
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
        attempts ? session_.absTimingTotal / attempts : 0.0,
        attempts ? session_.angleTotal / attempts : 0.0,
        medianReaction);
    ImGui::Text("Avg possible saved %.0f ms | touch height %.0f | contact speed %.0f",
        touches ? session_.possibleSavedTotal / touches : 0.0,
        touches ? session_.touchHeightTotal / touches : 0.0,
        touches ? session_.contactSpeedTotal / touches : 0.0);
    if (ImGui::Button("Reset session")) session_.reset();
}

void TakeoffCoach::renderSetupTab()
{
    ImGui::TextWrapped(
        "Each pair is a random range. Put both handles together for a fixed value.");

    int preset = getInt("tc_setup_preset");
    const char* presets[] = {"Shoot Default", "Fast Touch Default", "Custom"};
    if (ImGui::Combo("Setup preset", &preset, presets, 3))
        applyPreset(preset);

    if (ImGui::Button("Load")) applyPreset(preset);
    ImGui::SameLine();
    if (ImGui::Button("Save current to Custom")) saveModePreset(2);
    if (ImGui::Button("Reset Shoot")) resetModePreset(0);
    ImGui::SameLine();
    if (ImGui::Button("Reset Fast Touch")) resetModePreset(1);
    ImGui::SameLine();
    if (ImGui::Button("Reset Custom")) resetModePreset(2);
    if (ImGui::Button("Copy Shoot to Custom")) { loadModePreset(0); saveModePreset(2); }
    ImGui::SameLine();
    if (ImGui::Button("Copy Fast Touch to Custom")) { loadModePreset(1); saveModePreset(2); }

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

    int bounce = getInt("tc_bounce_policy");
    const char* policies[] = {"No pre-contact bounce", "Ground only", "Wall only", "Any supported bounce"};
    if (ImGui::Combo("Bounce policy", &bounce, policies, 4)) setValue("tc_bounce_policy", bounce);
    int maxBounces = getInt("tc_max_bounces");
    if (ImGui::SliderInt("Maximum bounce count", &maxBounces, 0, 8)) setValue("tc_max_bounces", maxBounces);
    bool prefer = getBool("tc_prefer_prebounce");
    if (ImGui::Checkbox("Prefer pre-bounce/simple target", &prefer)) setValue("tc_prefer_prebounce", prefer);

    float settle = getFloat("tc_initial_settle_time");
    if (ImGui::SliderFloat("Initial settle time", &settle, 0.0f, 1.5f, "%.2f s")) setValue("tc_initial_settle_time", settle);
    float verify = getFloat("tc_setup_validation_delay");
    if (ImGui::SliderFloat("Verification window", &verify, 0.0f, 2.5f, "%.2f s")) setValue("tc_setup_validation_delay", verify);
    float stable = getFloat("tc_validation_stable_time");
    if (ImGui::SliderFloat("Required stable time", &stable, 0.0f, 1.5f, "%.2f s")) setValue("tc_validation_stable_time", stable);
    float posTol = getFloat("tc_position_tolerance");
    if (ImGui::SliderFloat("Position tolerance", &posTol, 20.0f, 500.0f, "%.0f uu")) setValue("tc_position_tolerance", posTol);
    float velTol = getFloat("tc_velocity_tolerance");
    if (ImGui::SliderFloat("Velocity tolerance", &velTol, 10.0f, 800.0f, "%.0f uu/s")) setValue("tc_velocity_tolerance", velTol);

    bool single = getBool("tc_profile_single_jump");
    bool fast = getBool("tc_profile_fast_aerial");
    bool delayed = getBool("tc_profile_delayed_second");
    bool conservative = getBool("tc_profile_conservative");
    if (ImGui::Checkbox("Single jump profile", &single)) setValue("tc_profile_single_jump", single);
    if (ImGui::Checkbox("Fast aerial profile", &fast)) setValue("tc_profile_fast_aerial", fast);
    if (ImGui::Checkbox("Delayed second-jump profile", &delayed)) setValue("tc_profile_delayed_second", delayed);
    if (ImGui::Checkbox("Conservative profile", &conservative)) setValue("tc_profile_conservative", conservative);
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

