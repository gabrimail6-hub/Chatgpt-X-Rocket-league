#include "TakeoffCoach.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "IMGUI/imgui.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

BAKKESMOD_PLUGIN(TakeoffCoach,"Takeoff Coach","3.0.0",PLUGINTYPE_FREEPLAY)

namespace {
constexpr float PI=3.14159265358979323846f, D2R=PI/180.f, R2D=180.f/PI;
constexpr float G=-650.f, BOOST=991.7f, JUMP=292.f, CONTACT=170.f;
constexpr float SAFE_X=3550.f, SAFE_Y=4550.f, SAFE_Z=1850.f, FLOOR_Z=105.f;
void col(CanvasWrapper& c,int r,int g,int b,int a){c.SetColor((char)r,(char)g,(char)b,(char)a);}
}

void TakeoffCoach::onLoad(){
    registerAll();
    gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.SetVehicleInput",[this](CarWrapper c,void* p,std::string e){onInput(c,p,e);});
    gameWrapper->RegisterDrawable([this](CanvasWrapper c){draw(c);});
    cvarManager->log("Takeoff Coach 3 loaded");
}
void TakeoffCoach::onUnload(){gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");gameWrapper->UnregisterDrawables();}

void TakeoffCoach::registerAll(){
    auto r=[this](const char* n,const char* v,float lo,float hi){cvarManager->registerCvar(n,v,"",true,true,lo,true,hi);};
    r("tc_objective","2",0,3); r("tc_auto_reset","1",0,1); r("tc_observation","0.65",0,3); r("tc_feedback_time","2.8",0.5,10); r("tc_goal_sign","1",-1,1);
    r("tc_distance_lo","2800",1500,6000); r("tc_distance_hi","4400",1500,6000); r("tc_lateral_lo","-1000",-2200,2200); r("tc_lateral_hi","1000",-2200,2200);
    r("tc_height_lo","220",120,1500); r("tc_height_hi","700",120,1500); r("tc_rotation_lo","-120",-180,180); r("tc_rotation_hi","120",-180,180);
    r("tc_facing_lo","-15",-90,90); r("tc_facing_hi","15",-90,90); r("tc_car_speed_lo","700",0,2300); r("tc_car_speed_hi","1800",0,2300);
    r("tc_car_angle_lo","-10",-120,120); r("tc_car_angle_hi","10",-120,120); r("tc_ball_speed_lo","450",0,2200); r("tc_ball_speed_hi","1500",0,2200);
    r("tc_ball_up_lo","250",-500,1600); r("tc_ball_up_hi","1000",-500,1600); r("tc_ball_dir_lo","-180",-180,180); r("tc_ball_dir_hi","180",-180,180);
    r("tc_contact_lo","0.65",0.35,2); r("tc_contact_hi","2.30",0.8,4); r("tc_solver_hz","20",5,60); r("tc_strictness","0.55",0,1);
    r("tc_pos_tolerance","145",80,300); r("tc_horiz_cal","0.76",0.4,1.2); r("tc_vert_cal","0.78",0.4,1.2); r("tc_control_target","700",200,1600);
    r("tc_show_hud","1",0,1); r("tc_show_timing","1",0,1); r("tc_show_alignment","1",0,1); r("tc_show_reach","1",0,1); r("tc_show_numbers","1",0,1);
    r("tc_show_contact","1",0,1); r("tc_show_direction","1",0,1); r("tc_hud_position","0",0,2); r("tc_hud_scale","1",0.65,1.6); r("tc_hud_opacity","0.88",0.25,1);
    r("tc_green_ms","55",20,160); r("tc_yellow_early_ms","180",70,500); r("tc_yellow_late_ms","130",50,400); r("tc_green_align","4",1,15); r("tc_yellow_align","11",4,30);
    constexpr unsigned char P=PERMISSION_FREEPLAY;
    cvarManager->registerNotifier("tc_start",[this](std::vector<std::string>){requestStart();},"Start",P);
    cvarManager->registerNotifier("tc_new",[this](std::vector<std::string>){requestStart();},"New setup",P);
    cvarManager->registerNotifier("tc_stop",[this](std::vector<std::string>){stop();},"Stop",P);
}

void TakeoffCoach::requestStart(){auto id=++a_.id;gameWrapper->SetTimeout([this,id](GameWrapper*){startNow(id);},0.06f);}
void TakeoffCoach::startNow(unsigned long long id){
    if(id!=a_.id)return;
    if(!gameWrapper->IsInFreeplay()){a_.phase=Phase::Idle;a_.status="Enter Freeplay first";return;}
    Scenario s; bool ok=false; for(int k=0;k<50&&!ok;k++)ok=makeScenario(s);
    if(!ok||!applyScenario(s)){a_.phase=Phase::Idle;a_.status="No safe setup: narrow ranges";return;}
    a_.scenario=s;a_.objective=s.objective;a_.phase=Phase::Observation;a_.solution={};a_.jumpSolution={};a_.result={};a_.prevJump=false;a_.jumped=false;a_.closest=99999;
    a_.startAbs=now();a_.observationEnd=a_.startAbs+f("tc_observation");a_.lastSolve=-100;a_.status="READ THE SETUP";
    auto car=gameWrapper->GetLocalCar();a_.initialTouchFrame=car.IsNull()?-1:car.GetLastBallTouchFrame();
}
void TakeoffCoach::stop(){auto id=++a_.id;a_=Attempt{};a_.id=id;a_.status="Stopped";}

bool TakeoffCoach::makeScenario(Scenario& s){
    float base=sample(range("tc_rotation_lo","tc_rotation_hi"))*D2R;
    Vector fw=rot2({1,0,0},base), side{-fw.Y,fw.X,0};
    float dist=sample(range("tc_distance_lo","tc_distance_hi")), lat=sample(range("tc_lateral_lo","tc_lateral_hi"));
    Vector face=rot2(fw,sample(range("tc_facing_lo","tc_facing_hi"))*D2R);
    Vector velDir=rot2(face,sample(range("tc_car_angle_lo","tc_car_angle_hi"))*D2R);
    Vector ballDir=rot2(fw,sample(range("tc_ball_dir_lo","tc_ball_dir_hi"))*D2R);
    s.carPos=fw*(-dist)+side*(-0.22f*lat);s.carPos.Z=17;
    s.carVel=velDir*sample(range("tc_car_speed_lo","tc_car_speed_hi"));
    s.ballPos=fw*200.f+side*lat;s.ballPos.Z=sample(range("tc_height_lo","tc_height_hi"));
    s.ballVel=ballDir*sample(range("tc_ball_speed_lo","tc_ball_speed_hi"))+Vector{0,0,sample(range("tc_ball_up_lo","tc_ball_up_hi"))};
    s.carRot={0,(int)(std::atan2(face.Y,face.X)*32768.f/PI),0};s.objective=chosenObjective();s.goalSign=i("tc_goal_sign")>=0?1:-1;
    return safeScenario(s);
}
bool TakeoffCoach::safeScenario(const Scenario& s)const{
    if(std::abs(s.carPos.X)>SAFE_X||std::abs(s.carPos.Y)>SAFE_Y||std::abs(s.ballPos.X)>SAFE_X||std::abs(s.ballPos.Y)>SAFE_Y||s.ballPos.Z<FLOOR_Z||s.ballPos.Z>SAFE_Z)return false;
    Vector p=s.ballPos,v=s.ballVel;float dt=1.f/60.f;
    for(float t=0;t<f("tc_contact_hi");t+=dt){v.Z+=G*dt;p=p+v*dt;if(std::abs(p.X)>SAFE_X||std::abs(p.Y)>SAFE_Y||p.Z>SAFE_Z)return false;if(p.Z<FLOOR_Z)break;}
    return true;
}
bool TakeoffCoach::applyScenario(const Scenario& s){
    auto server=gameWrapper->GetCurrentGameState();auto car=gameWrapper->GetLocalCar();if(server.IsNull()||car.IsNull())return false;auto ball=server.GetBall();if(ball.IsNull())return false;
    car.SetLocation(s.carPos);car.SetRotation(s.carRot);car.SetVelocity(s.carVel);car.SetAngularVelocity({0,0,0},false);ball.SetLocation(s.ballPos);ball.SetVelocity(s.ballVel);return true;
}

void TakeoffCoach::onInput(CarWrapper car,void* params,std::string){
    if(!params||!gameWrapper->IsInFreeplay())return;auto local=gameWrapper->GetLocalCar();if(local.IsNull()||car.memory_address!=local.memory_address)return;
    float t=now();update(car,t);auto* in=(ControllerInput*)params;bool jump=in->Jump!=0;if(jump&&!a_.prevJump&&a_.phase==Phase::Ground)recordJump(car,t);a_.prevJump=jump;
}
void TakeoffCoach::update(CarWrapper car,float t){
    if(a_.phase==Phase::Idle||a_.phase==Phase::ResetPending)return;auto server=gameWrapper->GetCurrentGameState();if(server.IsNull())return;auto ball=server.GetBall();if(ball.IsNull())return;
    if(a_.phase==Phase::Observation){if(t>=a_.observationEnd){a_.phase=Phase::Ground;a_.status="SOLVING";}else a_.status=objectiveName(a_.objective)+" | READ";return;}
    if(a_.phase==Phase::Ground)updateGround(car,ball,t);else if(a_.phase==Phase::Airborne)updateAir(car,ball,t);else if(a_.phase==Phase::Feedback&&t>=a_.feedbackEnd)scheduleReset();
}
void TakeoffCoach::updateGround(CarWrapper car,BallWrapper ball,float t){
    if(t-a_.lastSolve>=1.f/std::max(5.f,f("tc_solver_hz"))){a_.solution=solve(car,ball,t);a_.lastSolve=t;}
    if(!a_.solution.valid){a_.status="NO SAFE INTERCEPT";return;}
    float g=f("tc_green_ms")/1000.f;a_.status=a_.solution.jumpDelay>g?"WAIT":(a_.solution.jumpDelay>=-g?"JUMP NOW":"LATE");a_.closest=std::min(a_.closest,len(ball.GetLocation()-car.GetLocation()));
}
void TakeoffCoach::updateAir(CarWrapper car,BallWrapper ball,float t){
    a_.closest=std::min(a_.closest,len(ball.GetLocation()-car.GetLocation()));bool touched=car.GetLastBallTouchFrame()!=a_.initialTouchFrame&&car.GetLastBallTouchFrame()>=0;
    auto server=gameWrapper->GetCurrentGameState();bool scored=!server.IsNull()&&server.IsInGoal(ball.GetLocation());if(touched||scored){finish(true,scored,car,ball,t);return;}
    float end=a_.jumpSolution.valid?a_.jumpSolution.idealAbs+a_.jumpSolution.aerialDuration+0.8f:a_.jumpAbs+2.8f;if(t>end||(car.IsOnGround()&&t-a_.jumpAbs>0.4f))finish(false,false,car,ball,t);
}
void TakeoffCoach::recordJump(CarWrapper,float t){a_.jumped=true;a_.jumpAbs=t;a_.jumpSolution=a_.solution;a_.phase=Phase::Airborne;a_.result.timingMs=a_.jumpSolution.valid?(t-a_.jumpSolution.idealAbs)*1000.f:9999;a_.result.alignDeg=a_.solution.alignDeg;a_.status="TRACKING";}
void TakeoffCoach::finish(bool touched,bool scored,CarWrapper car,BallWrapper ball,float t){
    if(a_.phase==Phase::Feedback||a_.phase==Phase::ResetPending)return;a_.result.touched=touched;a_.result.scored=scored;a_.result.closest=a_.closest;a_.result.carSpeed=len(car.GetVelocity());a_.result.relSpeed=len(car.GetVelocity()-ball.GetVelocity());
    float e=a_.result.timingMs,g=f("tc_green_ms"),al=std::abs(a_.result.alignDeg);
    if(!a_.jumped){a_.result.title="NO TAKEOFF";a_.result.correction="Jump before the intercept becomes unreachable.";}
    else if(e<-g){a_.result.title="EARLY BY "+n0(-e)+" ms";a_.result.correction="Stay grounded "+n0(-e)+" ms longer.";}
    else if(e>g){a_.result.title="LATE BY "+n0(e)+" ms";a_.result.correction="Start the fast aerial "+n0(e)+" ms earlier.";}
    else if(al>f("tc_green_align")){a_.result.title="TIMING GOOD, AIM OFF";a_.result.correction="Correct ground direction by "+n1(al)+" degrees.";}
    else if(!touched){a_.result.title="GOOD TAKEOFF, MISSED";a_.result.correction="Takeoff was usable; refine the aerial after leaving the ground.";}
    else if(a_.objective==Objective::Score){a_.result.title=scored?"GOAL":"TOUCH, NO GOAL";a_.result.correction=scored?"Requested shot completed.":"Reach was good; contact farther behind the ball.";}
    else if(a_.objective==Objective::Fast){a_.result.title="FAST TOUCH COMPLETE";a_.result.correction="Car speed at contact: "+n0(a_.result.carSpeed)+" uu/s.";}
    else {float target=f("tc_control_target");a_.result.title=a_.result.relSpeed<=target?"CONTROL TOUCH COMPLETE":"TOUCH TOO HARD";a_.result.correction="Relative contact speed: "+n0(a_.result.relSpeed)+" uu/s.";}
    std::ostringstream o;o<<"Timing ";if(a_.jumped&&std::abs(e)<9000)o<<(e>=0?"+":"")<<n0(e)<<" ms";else o<<"n/a";o<<" | Aim "<<n1(a_.result.alignDeg)<<" deg | Closest "<<n0(a_.result.closest)<<" uu";a_.result.detail=o.str();a_.status=a_.result.title;a_.phase=Phase::Feedback;a_.feedbackEnd=t+f("tc_feedback_time");
}
void TakeoffCoach::scheduleReset(){if(!b("tc_auto_reset")){a_.phase=Phase::Idle;a_.status="Press New Scenario";return;}a_.phase=Phase::ResetPending;auto id=++a_.id;gameWrapper->SetTimeout([this,id](GameWrapper*){startNow(id);},0.1f);}

std::vector<TakeoffCoach::BallSlice> TakeoffCoach::predict(const BallWrapper& ball)const{
    std::vector<BallSlice> out;Vector p=ball.GetLocation(),v=ball.GetVelocity();float dt=1.f/120.f;
    for(float t=0;t<=f("tc_contact_hi");t+=dt){if(t>=f("tc_contact_lo"))out.push_back({t,p,v});v.Z+=G*dt;p=p+v*dt;if(p.Z<FLOOR_Z||p.Z>SAFE_Z||std::abs(p.X)>SAFE_X||std::abs(p.Y)>SAFE_Y)break;}return out;
}
TakeoffCoach::Solution TakeoffCoach::solve(const CarWrapper& car,const BallWrapper& ball,float t)const{
    Solution best;float bestScore=-1e30f;auto path=predict(ball);Vector travel=len2(car.GetVelocity())>80?norm2(car.GetVelocity()):norm2(forward(car.GetRotation()));
    for(size_t k=0;k<path.size();k+=3){float dur,conf,cs,rs;Vector target,dir;if(!estimate(car,path[k],a_.objective,dur,conf,target,dir,cs,rs))continue;float delay=path[k].t-dur;if(delay<-0.28f)continue;float align=angle2(travel,dir);float score=delay*1000+conf*220-std::abs(align)*12;
        if(a_.objective==Objective::Fast)score+=cs*0.20f;else if(a_.objective==Objective::Control)score-=std::abs(rs-f("tc_control_target"))*0.18f;else if(a_.objective==Objective::Score){Vector goal{0,(float)a_.scenario.goalSign*5120,320};score+=(norm2(goal-path[k].pos).X*dir.X+norm2(goal-path[k].pos).Y*dir.Y)*350;}
        if(score>bestScore){bestScore=score;best.valid=true;best.jumpDelay=delay;best.idealAbs=t+delay;best.contactDelay=path[k].t;best.aerialDuration=dur;best.confidence=conf;best.alignDeg=align;best.predCarSpeed=cs;best.predRelSpeed=rs;best.contactBall=path[k].pos;best.contactCar=target;best.requiredDir=dir;}}
    return best;
}
bool TakeoffCoach::estimate(const CarWrapper& car,const BallSlice& sl,Objective objective,float& duration,float& confidence,Vector& targetCar,Vector& requiredDir,float& carSpeed,float& relSpeed)const{
    Vector cp=car.GetLocation(),cv=car.GetVelocity(),out;
    if(objective==Objective::Score)out=norm(Vector{0,(float)a_.scenario.goalSign*5120,320}-sl.pos);else if(objective==Objective::Control)out=len2(sl.vel)>80?norm2(sl.vel):norm2(sl.pos-cp);else out=norm(sl.pos-cp);
    targetCar=sl.pos-out*(objective==Objective::Control?125.f:150.f)-Vector{0,0,objective==Objective::Fast?55.f:35.f};Vector d=targetCar-cp;requiredDir=norm2(d);float h=len2(d),z=std::max(0.f,d.Z),speed=len2(cv),tol=f("tc_pos_tolerance"),count=0,first=0;bool found=false;
    for(float x=.34f;x<=std::min(2.f,sl.t+.25f);x+=1.f/120.f){float hr=speed*x+.5f*BOOST*f("tc_horiz_cal")*x*x+tol;float sj=std::max(0.f,x-.14f);float vr=JUMP*x+JUMP*sj+.5f*(BOOST*f("tc_vert_cal")+G)*x*x+tol;bool ok=hr>=h&&vr>=z;if(ok){if(!found){found=true;first=x;}count++;if(count>=1+f("tc_strictness")*5)break;}else if(found)break;}
    if(!found)return false;duration=first;confidence=clamp(count/6.f,0,1);carSpeed=std::min(2300.f,speed+BOOST*f("tc_horiz_cal")*first);relSpeed=std::abs(carSpeed-len2(sl.vel));return true;
}

void TakeoffCoach::draw(CanvasWrapper c){
    if(!b("tc_show_hud")||!gameWrapper->IsInFreeplay())return;int pos=i("tc_hud_position"),alpha=(int)(255*f("tc_hud_opacity"));Vector2 size=c.GetSize(),origin=pos==1?Vector2{24,150}:pos==2?Vector2{size.X-430,150}:Vector2{size.X/2-430,28};col(c,0,0,0,alpha);c.SetPosition(origin);c.FillBox({860,135});col(c,255,255,255,255);c.SetPosition({origin.X+15,origin.Y+10});c.DrawString(objectiveName(a_.objective)+" | "+a_.status,1.35f*f("tc_hud_scale"),1.35f*f("tc_hud_scale"),true);
    if(a_.phase==Phase::Feedback){c.SetPosition({origin.X+15,origin.Y+47});c.DrawString(a_.result.detail,.95f,.95f,true);c.SetPosition({origin.X+15,origin.Y+77});c.DrawString(a_.result.correction,1,1,true);}else{int idx=0;if(b("tc_show_timing")){float v=a_.solution.valid?clamp((-a_.solution.jumpDelay*1000.f)/std::max(f("tc_yellow_early_ms"),f("tc_yellow_late_ms")),-1,1):0;drawGauge(c,"TIMING",a_.solution.valid?n0(-a_.solution.jumpDelay*1000)+" ms":"none",v,idx++,true);}if(b("tc_show_alignment")){float er=a_.solution.valid?a_.solution.alignDeg:99;drawGauge(c,"ALIGN",n1(er)+" deg",clamp(er/f("tc_yellow_align"),-1,1),idx++,true);}if(b("tc_show_reach")){float q=a_.solution.valid?a_.solution.confidence:0;drawGauge(c,"REACH",n0(q*100)+"%",q,idx++,false);}}drawMarkers(c);col(c,255,255,255,255);
}
void TakeoffCoach::drawGauge(CanvasWrapper& c,const std::string& label,const std::string& value,float v,int idx,bool centered){
    int pos=i("tc_hud_position");Vector2 sz=c.GetSize(),base=pos==1?Vector2{39,205+idx*62}:pos==2?Vector2{sz.X-415,205+idx*62}:Vector2{sz.X/2-415+idx*275,80};int h=18;
    if(centered){col(c,200,55,55,230);c.SetPosition(base);c.FillBox({50,h});col(c,235,175,45,230);c.SetPosition({base.X+50,base.Y});c.FillBox({55,h});col(c,55,195,95,240);c.SetPosition({base.X+105,base.Y});c.FillBox({40,h});col(c,235,175,45,230);c.SetPosition({base.X+145,base.Y});c.FillBox({55,h});col(c,200,55,55,230);c.SetPosition({base.X+200,base.Y});c.FillBox({50,h});}
    else{col(c,200,55,55,230);c.SetPosition(base);c.FillBox({83,h});col(c,235,175,45,230);c.SetPosition({base.X+83,base.Y});c.FillBox({84,h});col(c,55,195,95,240);c.SetPosition({base.X+167,base.Y});c.FillBox({83,h});}
    float m=centered?(v+1)*.5f:v;m=clamp(m,0,1);int x=base.X+(int)(250*m);col(c,255,255,255,255);c.FillTriangle({x,base.Y-7},{x-6,base.Y-1},{x+6,base.Y-1});c.SetPosition({base.X,base.Y+21});c.DrawString(label+(b("tc_show_numbers")?"  "+value:""),.88f,.88f,true);
}
void TakeoffCoach::drawMarkers(CanvasWrapper& c){if(!a_.solution.valid||a_.phase==Phase::Feedback)return;if(b("tc_show_contact")){Vector2 p=c.Project(a_.solution.contactBall);col(c,255,255,255,230);c.DrawRect({p.X-9,p.Y-9},{p.X+9,p.Y+9});}if(b("tc_show_direction")){auto car=gameWrapper->GetLocalCar();if(!car.IsNull()){Vector s=car.GetLocation()+Vector{0,0,60},e=s+a_.solution.requiredDir*650;col(c,255,255,255,220);c.DrawLine(c.Project(s),c.Project(e),3);}}}
void TakeoffCoach::RenderSettings(){if(ImGui::BeginTabBar("tc_tabs")){if(ImGui::BeginTabItem("Drill")){drillTab();ImGui::EndTabItem();}if(ImGui::BeginTabItem("Setup")){setupTab();ImGui::EndTabItem();}if(ImGui::BeginTabItem("Solver")){solverTab();ImGui::EndTabItem();}if(ImGui::BeginTabItem("Feedback")){feedbackTab();ImGui::EndTabItem();}ImGui::EndTabBar();}}
void TakeoffCoach::drillTab(){
    ImGui::TextWrapped("Searches for a late robust takeoff. Negative timing is early; positive timing is late.");int o=i("tc_objective");const char* names[]={"Fast Touch","Control Touch","Random Call","Score"};if(ImGui::Combo("Objective",&o,names,4))set("tc_objective",o);
    if(ImGui::Button("Start / New Scenario"))cvarManager->executeCommand("tc_new");ImGui::SameLine();if(ImGui::Button("Stop"))cvarManager->executeCommand("tc_stop");bool ar=b("tc_auto_reset");if(ImGui::Checkbox("Auto next attempt",&ar))set("tc_auto_reset",ar);
    float ob=f("tc_observation");if(ImGui::SliderFloat("Observation delay",&ob,0,3,"%.2f s"))set("tc_observation",ob);float ft=f("tc_feedback_time");if(ImGui::SliderFloat("Result duration",&ft,.5f,10,"%.1f s"))set("tc_feedback_time",ft);
    int gs=i("tc_goal_sign");if(ImGui::RadioButton("Score toward +Y",gs>=0))set("tc_goal_sign",1);ImGui::SameLine();if(ImGui::RadioButton("Score toward -Y",gs<0))set("tc_goal_sign",-1);
}
void TakeoffCoach::setupTab(){
    ImGui::TextWrapped("Each pair is a random range. Equal handles make a fixed value. Ball direction: 0 away, +/-90 crossing, +/-180 toward.");
    if(ImGui::BeginTable("setup",2,ImGuiTableFlags_BordersInnerV)){ImGui::TableNextColumn();ImGui::Text("POSITION");rangeUi("Approach distance","tc_distance_lo","tc_distance_hi",1500,6000,"%.0f uu");rangeUi("Lateral offset","tc_lateral_lo","tc_lateral_hi",-2200,2200,"%.0f uu");rangeUi("Ball height","tc_height_lo","tc_height_hi",120,1500,"%.0f uu");rangeUi("Setup rotation","tc_rotation_lo","tc_rotation_hi",-180,180,"%.0f deg");rangeUi("Car facing offset","tc_facing_lo","tc_facing_hi",-90,90,"%.0f deg");
        ImGui::TableNextColumn();ImGui::Text("MOTION");rangeUi("Car speed","tc_car_speed_lo","tc_car_speed_hi",0,2300,"%.0f uu/s");rangeUi("Car velocity angle","tc_car_angle_lo","tc_car_angle_hi",-120,120,"%.0f deg");rangeUi("Ball horizontal speed","tc_ball_speed_lo","tc_ball_speed_hi",0,2200,"%.0f uu/s");rangeUi("Ball vertical speed","tc_ball_up_lo","tc_ball_up_hi",-500,1600,"%.0f uu/s");rangeUi("Ball travel direction","tc_ball_dir_lo","tc_ball_dir_hi",-180,180,"%.0f deg");ImGui::EndTable();}
}
void TakeoffCoach::solverTab(){float x=f("tc_contact_lo");if(ImGui::SliderFloat("Minimum contact horizon",&x,.35f,2,"%.2f s"))set("tc_contact_lo",x);x=f("tc_contact_hi");if(ImGui::SliderFloat("Maximum contact horizon",&x,.8f,4,"%.2f s"))set("tc_contact_hi",x);x=f("tc_strictness");if(ImGui::SliderFloat("Timing strictness",&x,0,1,"%.2f"))set("tc_strictness",x);x=f("tc_pos_tolerance");if(ImGui::SliderFloat("Position tolerance",&x,80,300,"%.0f uu"))set("tc_pos_tolerance",x);x=f("tc_solver_hz");if(ImGui::SliderFloat("Solver refresh",&x,5,60,"%.0f Hz"))set("tc_solver_hz",x);if(ImGui::CollapsingHeader("Advanced calibration")){x=f("tc_horiz_cal");if(ImGui::SliderFloat("Horizontal reach",&x,.4f,1.2f,"%.2f"))set("tc_horiz_cal",x);x=f("tc_vert_cal");if(ImGui::SliderFloat("Vertical reach",&x,.4f,1.2f,"%.2f"))set("tc_vert_cal",x);x=f("tc_control_target");if(ImGui::SliderFloat("Control relative-speed target",&x,200,1600,"%.0f uu/s"))set("tc_control_target",x);}ImGui::TextWrapped("Pre-bounce only: scenarios that hit a floor, wall, corner, or ceiling before the intercept are rejected.");}
void TakeoffCoach::feedbackTab(){
    auto ck=[this](const char* label,const char* name){bool v=b(name);if(ImGui::Checkbox(label,&v))set(name,v);};ck("Show HUD","tc_show_hud");ck("Timing gauge","tc_show_timing");ck("Alignment gauge","tc_show_alignment");ck("Reachability gauge","tc_show_reach");ck("Show numbers","tc_show_numbers");ck("Contact marker","tc_show_contact");ck("Ground direction line","tc_show_direction");
    int p=i("tc_hud_position");const char* ps[]={"Top","Left","Right"};if(ImGui::Combo("HUD placement",&p,ps,3))set("tc_hud_position",p);float x=f("tc_hud_scale");if(ImGui::SliderFloat("HUD scale",&x,.65f,1.6f,"%.2f"))set("tc_hud_scale",x);x=f("tc_hud_opacity");if(ImGui::SliderFloat("Panel opacity",&x,.25f,1,"%.2f"))set("tc_hud_opacity",x);ImGui::Separator();x=f("tc_green_ms");if(ImGui::SliderFloat("Green timing window",&x,20,160,"%.0f ms"))set("tc_green_ms",x);x=f("tc_yellow_early_ms");if(ImGui::SliderFloat("Yellow early limit",&x,70,500,"%.0f ms"))set("tc_yellow_early_ms",x);x=f("tc_yellow_late_ms");if(ImGui::SliderFloat("Yellow late limit",&x,50,400,"%.0f ms"))set("tc_yellow_late_ms",x);x=f("tc_green_align");if(ImGui::SliderFloat("Green alignment",&x,1,15,"%.1f deg"))set("tc_green_align",x);x=f("tc_yellow_align");if(ImGui::SliderFloat("Yellow alignment",&x,4,30,"%.1f deg"))set("tc_yellow_align",x);
}
bool TakeoffCoach::rangeUi(const char* label,const std::string& lo,const std::string& hi,float mn,float mx,const char* fmt){float a=f(lo),d=f(hi);bool ch=ImGui::DragFloatRange2(label,&a,&d,(mx-mn)/500.f,mn,mx,fmt,fmt);if(ch){if(a>d)std::swap(a,d);set(lo,a);set(hi,d);}return ch;}

std::string TakeoffCoach::GetPluginName(){return "Takeoff Coach";}void TakeoffCoach::SetImGuiContext(uintptr_t c){ImGui::SetCurrentContext((ImGuiContext*)c);}float TakeoffCoach::now()const{using C=std::chrono::steady_clock;static auto z=C::now();return std::chrono::duration<float>(C::now()-z).count();}
float TakeoffCoach::f(const std::string& n)const{return cvarManager->getCvar(n).getFloatValue();}int TakeoffCoach::i(const std::string& n)const{return cvarManager->getCvar(n).getIntValue();}bool TakeoffCoach::b(const std::string& n)const{return cvarManager->getCvar(n).getBoolValue();}void TakeoffCoach::set(const std::string& n,float v){cvarManager->getCvar(n).setValue(v);}void TakeoffCoach::set(const std::string& n,int v){cvarManager->getCvar(n).setValue(v);}void TakeoffCoach::set(const std::string& n,bool v){cvarManager->getCvar(n).setValue(v?1:0);}TakeoffCoach::RangeF TakeoffCoach::range(const std::string& lo,const std::string& hi)const{float a=f(lo),d=f(hi);if(a>d)std::swap(a,d);return {a,d};}float TakeoffCoach::sample(RangeF r){if(std::abs(r.hi-r.lo)<.0001f)return r.lo;return std::uniform_real_distribution<float>(r.lo,r.hi)(rng_);}TakeoffCoach::Objective TakeoffCoach::chosenObjective(){Objective o=(Objective)i("tc_objective");if(o==Objective::Random)return std::uniform_int_distribution<int>(0,1)(rng_)?Objective::Fast:Objective::Control;return o;}std::string TakeoffCoach::objectiveName(Objective o){if(o==Objective::Fast)return "FAST TOUCH";if(o==Objective::Control)return "CONTROL TOUCH";if(o==Objective::Score)return "SCORE";return "RANDOM CALL";}
float TakeoffCoach::clamp(float v,float lo,float hi){return std::max(lo,std::min(v,hi));}float TakeoffCoach::len(const Vector& v){return std::sqrt(v.X*v.X+v.Y*v.Y+v.Z*v.Z);}float TakeoffCoach::len2(const Vector& v){return std::sqrt(v.X*v.X+v.Y*v.Y);}Vector TakeoffCoach::norm(const Vector& v){float l=len(v);return l<.001f?Vector{1,0,0}:v*(1/l);}Vector TakeoffCoach::norm2(const Vector& v){float l=len2(v);return l<.001f?Vector{1,0,0}:Vector{v.X/l,v.Y/l,0};}Vector TakeoffCoach::rot2(const Vector& v,float r){float c=std::cos(r),s=std::sin(r);return {c*v.X-s*v.Y,s*v.X+c*v.Y,v.Z};}float TakeoffCoach::angle2(const Vector& a,const Vector& b){Vector x=norm2(a),y=norm2(b);return std::atan2(x.X*y.Y-x.Y*y.X,clamp(x.X*y.X+x.Y*y.Y,-1,1))*R2D;}Vector TakeoffCoach::forward(const Rotator& r){float p=r.Pitch*PI/32768.f,y=r.Yaw*PI/32768.f,cp=std::cos(p);return {cp*std::cos(y),cp*std::sin(y),std::sin(p)};}std::string TakeoffCoach::n0(float v){std::ostringstream o;o<<std::fixed<<std::setprecision(0)<<v;return o.str();}std::string TakeoffCoach::n1(float v){std::ostringstream o;o<<std::fixed<<std::setprecision(1)<<v;return o.str();}
