#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/CanvasWrapper.h"
#include <chrono>
#include <random>
#include <string>
#include <vector>

class TakeoffCoach final : public BakkesMod::Plugin::BakkesModPlugin,
                           public BakkesMod::Plugin::PluginSettingsWindow {
public:
    void onLoad() override;
    void onUnload() override;
    void RenderSettings() override;
    std::string GetPluginName() override;
    void SetImGuiContext(uintptr_t ctx) override;
private:
    enum class Objective:int { Fast=0, Control=1, Random=2, Score=3 };
    enum class Phase { Idle, Observation, Ground, Airborne, Feedback, ResetPending };
    struct RangeF { float lo{}, hi{}; };
    struct Scenario { Vector carPos{}, carVel{}, ballPos{}, ballVel{}; Rotator carRot{}; Objective objective{Objective::Fast}; int goalSign{1}; };
    struct BallSlice { float t{}; Vector pos{}, vel{}; };
    struct Solution { bool valid{}; float idealAbs{}, jumpDelay{}, contactDelay{}, aerialDuration{}, confidence{}, alignDeg{}, predCarSpeed{}, predRelSpeed{}; Vector contactBall{}, contactCar{}, requiredDir{}; };
    struct Result { bool touched{}, scored{}; float timingMs{}, alignDeg{}, closest{99999}, carSpeed{}, relSpeed{}; std::string title, detail, correction; };
    struct Attempt { unsigned long long id{}; Phase phase{Phase::Idle}; Objective objective{Objective::Fast}; Scenario scenario{}; Solution solution{}, jumpSolution{}; Result result{}; bool prevJump{}, jumped{}; int initialTouchFrame{-1}; float startAbs{}, observationEnd{}, jumpAbs{}, lastSolve{-100}, feedbackEnd{}, closest{99999}; std::string status{"Open settings and start."}; };

    Attempt a_;
    std::mt19937 rng_{std::random_device{}()};

    void registerAll();
    void requestStart();
    void startNow(unsigned long long id);
    void stop();
    bool makeScenario(Scenario& s);
    bool safeScenario(const Scenario& s) const;
    bool applyScenario(const Scenario& s);
    void onInput(CarWrapper car, void* params, std::string eventName);
    void update(CarWrapper car, float now);
    void updateGround(CarWrapper car, BallWrapper ball, float now);
    void updateAir(CarWrapper car, BallWrapper ball, float now);
    void recordJump(CarWrapper car, float now);
    void finish(bool touched, bool scored, CarWrapper car, BallWrapper ball, float now);
    void scheduleReset();
    std::vector<BallSlice> predict(const BallWrapper& ball) const;
    Solution solve(const CarWrapper& car, const BallWrapper& ball, float now) const;
    bool estimate(const CarWrapper& car, const BallSlice& sl, Objective objective, float& duration, float& confidence, Vector& targetCar, Vector& requiredDir, float& carSpeed, float& relSpeed) const;
    void draw(CanvasWrapper canvas);
    void drawGauge(CanvasWrapper& canvas, const std::string& label, const std::string& value, float normalized, int index, bool centered);
    void drawMarkers(CanvasWrapper& canvas);
    void drillTab();
    void setupTab();
    void solverTab();
    void feedbackTab();
    bool rangeUi(const char* label,const std::string& lo,const std::string& hi,float min,float max,const char* fmt);

    float now() const;
    float f(const std::string& n) const;
    int i(const std::string& n) const;
    bool b(const std::string& n) const;
    void set(const std::string& n,float v);
    void set(const std::string& n,int v);
    void set(const std::string& n,bool v);
    RangeF range(const std::string& lo,const std::string& hi) const;
    float sample(RangeF r);
    Objective chosenObjective();
    static std::string objectiveName(Objective o);
    static float clamp(float v,float lo,float hi);
    static float len(const Vector& v);
    static float len2(const Vector& v);
    static Vector norm(const Vector& v);
    static Vector norm2(const Vector& v);
    static Vector rot2(const Vector& v,float rad);
    static float angle2(const Vector& a,const Vector& b);
    static Vector forward(const Rotator& r);
    static std::string n0(float v);
    static std::string n1(float v);
};
