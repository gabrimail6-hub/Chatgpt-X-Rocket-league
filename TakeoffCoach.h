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
#include <vector>
#include <deque>

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
        FastTouch = 0,
        Shoot = 1
    };

    enum class SurfaceType : int
    {
        None, Ground, Ceiling, SideWall, BackWall, DiagonalCorner,
        GoalFloor, GoalBackWall, GoalCeiling, GoalPost, Crossbar
    };

    enum class BouncePolicy : int
    {
        None = 0, GroundOnly = 1, WallOnly = 2, Any = 3
    };

    enum class AimMode : int
    {
        Centre = 0, NearPost = 1, FarPost = 2, RandomCorridor = 3, Custom = 4
    };

    enum class AerialProfile : int
    {
        SingleJump, FastAerial, DelayedSecondJump, Conservative
    };

    enum class GuidanceStyle : int
    {
        Read = 0,
        ReactionCue = 1
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
        Objective objective = Objective::FastTouch;
        GuidanceStyle guidanceStyle = GuidanceStyle::Read;
        int goalSign = 1;
    };

    struct BallPredictionSlice
    {
        float absoluteTime = 0.0f;
        Vector position{};
        Vector velocity{};
        Vector angularVelocity{};
        int bounceCount = 0;
        SurfaceType surface = SurfaceType::None;
        bool supportedGeometry = true;
    };

    struct ContactTarget
    {
        bool valid = false;
        float contactAbsoluteTime = 0.0f;
        Vector ballPosition{};
        Vector ballVelocity{};
        Vector desiredCarPosition{};
        Vector desiredFacingDirection{};
        Vector desiredGoalPoint{};
        Vector predictedOutgoingVelocity{};
        int bounceCount = 0;
        SurfaceType lastSurface = SurfaceType::None;
        float shotAimErrorDeg = 0.0f;
        float predictedShotSpeed = 0.0f;
    };

    struct Solution
    {
        bool valid = false;
        float idealJumpAbsolute = 0.0f;
        float jumpDelay = 0.0f;
        float contactDelay = 0.0f;
        float requiredAerialDuration = 0.0f;
        float confidence = 0.0f;
        float robustness = 0.0f;
        float alignmentErrorDeg = 0.0f;
        float requiredBoost = 0.0f;
        Vector contactPoint{};
        Vector requiredDirection{};
        Vector idealTakeoffPosition{};
        AerialProfile profile = AerialProfile::FastAerial;
        ContactTarget target{};
    };

    struct Attempt
    {
        uint64_t generation = 0;
        Phase phase = Phase::Idle;
        Objective objective = Objective::FastTouch;
        GuidanceStyle guidanceStyle = GuidanceStyle::Read;
        Scenario scenario{};
        Solution solution{};
        Solution lockedSolution{};
        Solution validationCandidate{};
        Solution lastValidSolution{};
        Solution jumpSolution{};

        bool previousJump = false;
        bool touched = false;
        bool scored = false;
        bool everHadSolution = false;
        bool targetLocked = false;
        bool allowUnreachable = false;
        bool invalidNonGroundBounce = false;

        float startedAt = 0.0f;
        float jumpAt = 0.0f;
        float feedbackUntil = 0.0f;
        float lastSolveAt = -100.0f;
        float closestDistance = 99999.0f;
        float timingErrorMs = 0.0f;
        float alignmentErrorDeg = 0.0f;
        float previousBallSpeed = 0.0f;
        Vector previousBallVelocity{};
        float contactHeight = 0.0f;
        float validationDeadline = 0.0f;
        float validationStableSince = 0.0f;
        float lockedContactAbsolute = 0.0f;
        int rejectedSetups = 0;
        int failedValidationSamples = 0;
        int pathCorrections = 0;
        float settledAt = 0.0f;
        float initialCandidateTime = 0.0f;
        float initialAvailableJumpDelay = 0.0f;
        float cueTime = 0.0f;
        float reactionRawMs = 0.0f;
        float reactionAfterAllowanceMs = 0.0f;
        float possibleTimeSavedMs = 0.0f;
        bool cueShown = false;
        bool reactionCaptured = false;
        Vector snapshotBallPosition{};
        Vector snapshotBallVelocity{};
        std::vector<BallPredictionSlice> ballPath;
        ContactTarget candidateTarget{};
        ContactTarget lockedTarget{};

        std::string headline = "Open settings and start a drill.";
        std::string detail;
        std::string correction;
    };

    struct SessionStats
    {
        int attempts = 0;
        int touches = 0;
        int goals = 0;
        double absTimingTotal = 0.0;
        double angleTotal = 0.0;
        double possibleSavedTotal = 0.0;
        double touchHeightTotal = 0.0;
        double contactSpeedTotal = 0.0;
        std::vector<float> reactions;
        void reset() { *this = SessionStats{}; }
    };

    Attempt attempt_;
    SessionStats session_;
    int consecutiveRerolls_ = 0;
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

    Solution solve(CarWrapper car, BallWrapper ball, float now);
    Solution solveLockedTarget(CarWrapper car, float now) const;
    std::vector<BallPredictionSlice> buildBallPath(Vector position, Vector velocity, Vector angularVelocity, float now) const;
    bool collideBall(Vector& position, Vector& velocity, float radius, SurfaceType& surface) const;
    ContactTarget selectContactTarget(CarWrapper car, const std::vector<BallPredictionSlice>& path, float now) const;
    Solution solveTargetWithProfiles(CarWrapper car, const ContactTarget& target, float now) const;
    bool simulateAerialProfile(CarWrapper car, const ContactTarget& target, AerialProfile profile, float duration, Solution& out) const;
    bool samplePath(float absoluteTime, BallPredictionSlice& out) const;
    Vector chooseGoalPoint(const ContactTarget& base, Vector carPosition) const;

    void renderHud(CanvasWrapper canvas);
    void drawGauge(
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
        float displayPositive);

    void renderDrillTab();
    void renderSetupTab();
    void renderFeedbackTab();
    void renderAdvancedTab();
    void saveModePreset(int objective);
    void loadModePreset(int objective);
    void resetModePreset(int preset);
    std::string modePrefix(int preset) const;
    void applyPreset(int preset);
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

