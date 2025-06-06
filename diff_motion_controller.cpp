#include <algorithm>
#include <iostream>
#include <cassert>
#include <signal.h>
#include <memory>
#include <iostream>
#include <yaml-cpp/yaml.h>

#include <lcm/lcm-cpp.hpp>
#include <mbot_lcm_msgs/pose2D_t.hpp>
#include <mbot_lcm_msgs/path2D_t.hpp>
#include <mbot_lcm_msgs/timestamp_t.hpp>
#include <mbot_lcm_msgs/mbot_message_received_t.hpp>
#include <mbot_lcm_msgs/mbot_slam_reset_t.hpp>
#include <utils/timestamp.h>
#include <utils/geometric/angle_functions.hpp>
#include <utils/geometric/pose_trace.hpp>
#include <utils/lcm_config.h>
#include <mbot/mbot_channels.h>
#include <slam/slam_channels.h>

#include "diff_maneuver_controller.h"

/////////////////////// TODO: /////////////////////////////
/**
 * Code below is a little more than a template. You will need
 * to update the maneuver controllers to function more effectively
 * and/or add different controllers. 
 * You will at least want to:
 *  - Add a form of PID to control the speed at which your
 *      robot reaches its target pose.
 *  - Add a rotation element to the StratingManeuverController
 *      to maintian a avoid deviating from the intended path.
 *  - Limit (min max) the speeds that your robot is commanded
 *      to avoid commands to slow for your bots or ones too high
 */
///////////////////////////////////////////////////////////

class StraightManeuverController : public ManeuverControllerBase
{

private:
    float fwd_pid[3] = {1.0, 0, 0};
    float fwd_sum_error = 0;
    float fwd_last_error = 0;
    float turn_pid[3] = {3.0, 0, 0};
    float turn_sum_error = 0;
    float turn_last_error = 0;
public:
    StraightManeuverController() = default;   
    virtual mbot_lcm_msgs::twist2D_t get_command(const mbot_lcm_msgs::pose2D_t& pose, const mbot_lcm_msgs::pose2D_t& target) override
    {
        float dx = target.x - pose.x;
        float dy = target.y - pose.y;
        float d_fwd = sqrt(pow(dx,2) + pow(dy,2));
        float d_theta = angle_diff(atan2(dy,dx), pose.theta);

        // PID separately for the fwd and the angular velocity output given the fwd and angular error
        fwd_sum_error += d_fwd;
        float fwd_der = 0;
        if (fwd_last_error > 0)
            fwd_der = (d_fwd - fwd_last_error) / 0.05;
        
        float fwd_vel = fwd_pid[0] * d_fwd + fwd_pid[1] * fwd_sum_error + fwd_pid[2] * fwd_der;
        // fprintf(stdout,"Fwd error: %f\tFwd vel: %f\n", d_fwd, fwd_vel);

        turn_sum_error += d_theta;
        float turn_der = 0;
        if (turn_last_error > 0)
            turn_der = angle_diff(d_theta, turn_last_error) / 0.05;
        
        float turn_vel = turn_pid[0] * d_theta + turn_pid[1] * turn_sum_error + turn_pid[2] * turn_der;
        // fprintf(stdout,"Turn error: %f\tTurn vel: %f\n", d_theta, turn_vel);

        return {0, fwd_vel, 0, turn_vel};
    }

    virtual bool target_reached(const mbot_lcm_msgs::pose2D_t& pose, const mbot_lcm_msgs::pose2D_t& target, bool is_end_pose)  override
    {
        return ((fabs(pose.x - target.x) < 0.02) && (fabs(pose.y - target.y)  < 0.02));
    }
};

class TurnManeuverController : public ManeuverControllerBase
{
private:
    float turn_pid[3] = {3.0, 0, 0};
    float turn_sum_error = 0;
    float turn_last_error = 0;
public:
    TurnManeuverController() = default;   
    virtual mbot_lcm_msgs::twist2D_t get_command(const mbot_lcm_msgs::pose2D_t& pose, const mbot_lcm_msgs::pose2D_t& target) override
    {
        float dx = target.x - pose.x;
        float dy = target.y - pose.y;
        float d_theta = angle_diff(atan2(dy,dx), pose.theta);
        // fprintf(stdout,"dx: %f\tdy: %f\td_theta: %f\n", dx, dy, d_theta);

        // PID for the angular velocity given the delta theta
        turn_sum_error += d_theta;
        float turn_der = 0.0;
        if (turn_last_error > 0)
            turn_der = (d_theta - turn_last_error) / 0.05;
        
        float turn_vel = turn_pid[0] * d_theta + turn_pid[1] * turn_sum_error + turn_pid[2] * turn_der;
        // fprintf(stdout,"Turn error: %f\tTurn vel: %f\tPose theta: %f\n", d_theta, turn_vel, pose.theta);

        return {0, 0, 0, turn_vel};
    }
    mbot_lcm_msgs::twist2D_t get_command_final_turn(const mbot_lcm_msgs::pose2D_t& pose, const mbot_lcm_msgs::pose2D_t& target)
    {
        float d_theta = angle_diff(target.theta, pose.theta);
        // fprintf(stdout,"dx: %f\tdy: %f\td_theta: %f\n", dx, dy, d_theta);

        // PID for the angular velocity given the delta theta
        turn_sum_error += d_theta;
        float turn_der = 0;
        if (turn_last_error > 0)
            turn_der = (d_theta - turn_last_error) / 0.05;
        
        float turn_vel = turn_pid[0] * d_theta + turn_pid[1] * turn_sum_error + turn_pid[2] * turn_der;
        // fprintf(stdout,"Turn error: %f\tTurn vel: %f\tPose theta: %f\n", d_theta, turn_vel, pose.theta);

        return {0, 0, 0, turn_vel};
    }

    virtual bool target_reached(const mbot_lcm_msgs::pose2D_t& pose, const mbot_lcm_msgs::pose2D_t& target, bool is_end_pose)  override
    {
        float dx = target.x - pose.x;
        float dy = target.y - pose.y;
        float target_heading = atan2(dy, dx);
        // Handle the case when the target is on the same x,y but on a different theta
        return (fabs(angle_diff(pose.theta, target_heading)) < 0.05);
    }
    bool target_reached_final_turn(const mbot_lcm_msgs::pose2D_t& pose, const mbot_lcm_msgs::pose2D_t& target)
    {
        float dx = target.x - pose.x;
        float dy = target.y - pose.y;
        float target_heading = atan2(dy, dx);
        // Handle the case when the target is on the same x,y but on a different theta
        return (fabs(angle_diff(target.theta, pose.theta)) < 0.05);
    }
};


class SmartManeuverController : public ManeuverControllerBase
{

private:
    float pid[3] = {1.0, 2.5, 0.0}; //kp, ka, kb
    float d_end_crit = 0.02;
    float d_end_midsteps = 0.08;
    float angle_end_crit = 0.2;
public:
    SmartManeuverController() = default;   
    virtual mbot_lcm_msgs::twist2D_t get_command(const mbot_lcm_msgs::pose2D_t& pose, const mbot_lcm_msgs::pose2D_t& target) override
    {
        float vel_sign = 1;
        float dx = target.x - pose.x;
        float dy = target.y - pose.y;
        float d_fwd = sqrt(dx * dx + dy * dy);
        float alpha = angle_diff(atan2(dy,dx), pose.theta);
        // printf("alpha: %f\n", alpha);

        // // To avoid weird behaviour at alpha=pi/2, because it is a common case
        // float margin = 2 * M_PI / 180;
        // if (fabs(alpha) > M_PI_2 + margin)
        // {
        //     alpha = wrap_to_pi(alpha - M_PI);
        //     vel_sign = -1;
        // }
        float beta = wrap_to_pi(target.theta -(alpha + pose.theta));
        float fwd_vel = vel_sign *  pid[0] * d_fwd;
        float turn_vel = pid[1] * alpha + pid[2] * beta;

        // If alpha is more than 45 degrees, turn in place and then go
        if (fabs(alpha) > M_PI_4)
        {
            fwd_vel = 0;
        }

        // printf("%f,%f\n", fwd_vel, turn_vel);
        return {0, fwd_vel, 0, turn_vel};
    }

    virtual bool target_reached(const mbot_lcm_msgs::pose2D_t& pose, const mbot_lcm_msgs::pose2D_t& target, bool is_end_pose)  override
    {
        float distance = d_end_midsteps;
        if (is_end_pose)
            distance = d_end_crit;
        return ((fabs(pose.x - target.x) < distance) && (fabs(pose.y - target.y)  < distance));
    }
};

class CarrotFollowingController : public ManeuverControllerBase
{
// modification of StraightManeuverController for Carrot Following implementation
private:
    YAML::Node params;
    float carrot_dist = 0.3; // <- this will need to be tuned
    float target_velocity = 0.2; // m/s
    float target_turn = M_PI/4; // rad/s
    float angle_gain = 0;
    float scale = 1;
    float turn_tol = 0.4;
    bool waypoint_reached = false;

public:
    CarrotFollowingController(){
        try{
            params = YAML::LoadFile("../params/motion_controller.yaml");
        }
        catch(YAML::BadFile e){
            std::cerr << "Error Opening YAML file" << std::endl;
        }
        catch(std::exception e){
            std::cerr << "Error parsing YAML file: " << e.what() << std::endl;
        }

        carrot_dist = params["Carrot"]["Carrot_Dist"].as<float>();
        target_velocity = params["Carrot"]["Target_Vel"].as<float>(); // m/s
        scale = params["Carrot"]["Target_Turn_Ratio"].as<float>();
        angle_gain = params["Carrot"]["Angle_Gain"].as<float>();
        turn_tol = params["Carrot"]["Turn_Tol"].as<float>();

        target_turn = M_PI/scale; // rad/s

        printf("Carrot Dist: %f m   Target Velocity: %f m/s    Target Turn Vel = pi/%f rad/s\n", carrot_dist, target_velocity, scale);
    }   
    virtual mbot_lcm_msgs::twist2D_t get_command(const mbot_lcm_msgs::pose2D_t& pose, const mbot_lcm_msgs::pose2D_t& target) override
    {   
        float dx = target.x - pose.x;
        float dy = target.y - pose.y;
        
        float target_dist = sqrt(dx*dx+ dy*dy);
        float target_ang = atan2(dy,dx);
        float cur_pose = wrap_to_pi(pose.theta);


        float cx, cy;
        if(target_dist > carrot_dist){
            cx = pose.x + carrot_dist * cos(target_ang);
            cy = pose.y + carrot_dist * sin(target_ang);
        }
        else{
            cx = target.x;
            cy = target.y;
        }
        
        dx = cx - pose.x;
        dy = cy - pose.y;

        float d_fwd = sqrt(pow(dx,2) + pow(dy,2));
        float d_theta = wrap_to_pi(cur_pose - atan2(dy,dx));
        
        float vel = target_velocity * (d_fwd / carrot_dist);
        float fwd_vel = std::min(vel, target_velocity);

        float vel_a = d_theta * angle_gain;
        int sign = vel_a < 0 ? -1 : 1;
        float turn_vel = std::min(float(fabs(vel_a)), target_turn) * sign;
        // float turn_vel = target_turn * sign;


        if ((fabs(d_theta) > turn_tol)){
            fwd_vel = 0; 
            turn_vel = target_turn * sign;
        }

        std::cout << fwd_vel << " " << turn_vel <<" "<< d_theta  << " " << vel_a  <<" " << target_ang << " " <<target.x << " "<< target.y << std::endl;
        return {0, fwd_vel, 0, turn_vel};
    }

    virtual bool target_reached(const mbot_lcm_msgs::pose2D_t& pose, const mbot_lcm_msgs::pose2D_t& target, bool is_end_pose)  override
    {
        return ((fabs(pose.x - target.x) < 0.02) && (fabs(pose.y - target.y)  < 0.02));
    }
// error in PID tf must be greater than tf/2 for stability

};


class MotionController
{ 
public: 
    
    /**
    * Constructor for MotionController.
    */
    MotionController(lcm::LCM * instance)
    :
        lcmInstance(instance),
        odomToGlobalFrame_{0, 0, 0, 0}
    {
        subscribeToLcm();

	    time_offset = 0;
	    timesync_initialized_ = false;

        // straight_controller = controllers[mp].get();
    } 
    
    /**
    * \brief updateCommand calculates the new motor command to send to the Mbot. This method is called after each call to
    * lcm.handle. You need to check if you have sufficient data to calculate a new command, or if the previous command
    * should just be used again until for feedback becomes available.
    * 
    * \return   The motor command to send to the mbot_driver.
    */
    mbot_lcm_msgs::twist2D_t updateCommand(void) 
    {
        mbot_lcm_msgs::twist2D_t cmd {now(), 0.0, 0.0, 0.0};
        if(!targets_.empty() && !odomTrace_.empty()) 
        {   
            mbot_lcm_msgs::pose2D_t target = targets_.back();
            bool is_last_target = targets_.size() == 1;
            mbot_lcm_msgs::pose2D_t pose = currentPose();

            if (state_ == SMART) 
            {
                if (smart_controller.target_reached(pose, target, is_last_target))
                {
                    if (is_last_target)
                        state_ = FINAL_TURN;
                    else if(!assignNextTarget())
                        printf("Target reached! (%f,%f,%f)\n", target.x, target.y, target.theta);
                }
                else cmd = smart_controller.get_command(pose, target);
            }

            ///////  TODO: Add different states when adding maneuver controls /////// 
             if (state_ == CARROT) 
            {
                if (carrot_controller.target_reached(pose, target, is_last_target))
                {
                    if (is_last_target)
                        state_ = FINAL_TURN;
                    else if(!assignNextTarget())
                        printf("Target reached! (%f,%f,%f)\n", target.x, target.y, target.theta);
                }
                else cmd = carrot_controller.get_command(pose, target);
            }

            if(state_ == INITIAL_TURN)
            { 
                if(turn_controller.target_reached(pose, target, is_last_target))
                {
		            state_ = DRIVE;
                } 
                else
                {
                    cmd = turn_controller.get_command(pose, target);
                }
            }
            else if(state_ == DRIVE) 
            {
                if(straight_controller.target_reached(pose, target, is_last_target))
                {
                    state_ = FINAL_TURN;
                    // if(!assignNextTarget())
                    // {
                    //     // std::cout << "\rTarget Reached!\n";
                    //     printf("Target reached! (%f,%f,%f)\n", target.x, target.y, target.theta);
                    // }
                }
                else
                { 
                    cmd = straight_controller.get_command(pose, target);
                }
		    }
            else if(state_ == FINAL_TURN)
            { 
                if(turn_controller.target_reached_final_turn(pose, target))
                {
		            if(!assignNextTarget())
                    {
                        // std::cout << "\rTarget Reached!\n";
                        printf("Target reached! (%f,%f,%f)\n", target.x, target.y, target.theta);
                    }
                } 
                else
                {
                    cmd = turn_controller.get_command_final_turn(pose, target);
                }
            }
            // else
            // {
            //     std::cerr << "ERROR: MotionController: Entered unknown state: " << state_ << '\n';
            // }
		} 
        return cmd; 
    }

    bool timesync_initialized(){ return timesync_initialized_; }

    void handleTimesync(const lcm::ReceiveBuffer* buf, const std::string& channel, const mbot_lcm_msgs::timestamp_t* timesync)
    {
	    timesync_initialized_ = true;
	    time_offset = timesync->utime-utime_now();
    }
    
    void handlePath(const lcm::ReceiveBuffer* buf, const std::string& channel, const mbot_lcm_msgs::path2D_t* path)
    {
        targets_ = path->path;
        std::reverse(targets_.begin(), targets_.end()); // store first at back to allow for easy pop_back()

    	std::cout << "received new path at time: " << path->utime << "\n"; 
    	for(auto pose : targets_)
        {
    		std::cout << "(" << pose.x << "," << pose.y << "," << pose.theta << "); ";
    	}
        std::cout << std::endl;

        assignNextTarget();

        //confirm that the path was received
        mbot_lcm_msgs::mbot_message_received_t confirm {now(), path->utime, channel};
        lcmInstance->publish(MESSAGE_CONFIRMATION_CHANNEL, &confirm);
    }
    
    void handleOdometry(const lcm::ReceiveBuffer* buf, const std::string& channel, const mbot_lcm_msgs::pose2D_t* odometry)
    {
        mbot_lcm_msgs::pose2D_t pose {odometry->utime, odometry->x, odometry->y, odometry->theta};
        odomTrace_.addPose(pose);
    }
    
    void handlePose(const lcm::ReceiveBuffer* buf, const std::string& channel, const mbot_lcm_msgs::pose2D_t* pose)
    {
        computeOdometryOffset(*pose);
    }
    
private:
    
    enum State
    {
        INITIAL_TURN,
        DRIVE,
        FINAL_TURN, // to get to the pose heading
        SMART,
        CARROT
    };
    // motion planner type

    
    mbot_lcm_msgs::pose2D_t odomToGlobalFrame_;      // transform to convert odometry into the global/map coordinates for navigating in a map
    PoseTrace  odomTrace_;              // trace of odometry for maintaining the offset estimate
    std::vector<mbot_lcm_msgs::pose2D_t> targets_;

    State state_;

    int64_t time_offset;
    bool timesync_initialized_;

    lcm::LCM * lcmInstance;
 
    TurnManeuverController turn_controller;
    StraightManeuverController straight_controller;
    SmartManeuverController smart_controller;
    CarrotFollowingController carrot_controller; //<- replaces straight manuever controller

    // std::vector<std::unique_ptr<ManeuverControllerBase>> controllers = {
    //     std::make_unique<StraightManeuverController>(),
    //     std::make_unique<CarrotFollowingController>()
    // };

    // ManeuverControllerBase* straight_controller = nullptr;

    int64_t now()
    {
	    return utime_now() + time_offset;
    }
    
    bool assignNextTarget(void)
    {
        if(!targets_.empty()) { targets_.pop_back(); }
        state_ = CARROT; 
        return !targets_.empty();
    }
    
    void computeOdometryOffset(const mbot_lcm_msgs::pose2D_t& globalPose)
    {
        mbot_lcm_msgs::pose2D_t odomAtTime = odomTrace_.poseAt(globalPose.utime);
        double deltaTheta = globalPose.theta - odomAtTime.theta;
        double xOdomRotated = (odomAtTime.x * std::cos(deltaTheta)) - (odomAtTime.y * std::sin(deltaTheta));
        double yOdomRotated = (odomAtTime.x * std::sin(deltaTheta)) + (odomAtTime.y * std::cos(deltaTheta));
         
        odomToGlobalFrame_.x = globalPose.x - xOdomRotated;
        odomToGlobalFrame_.y = globalPose.y - yOdomRotated; 
        odomToGlobalFrame_.theta = deltaTheta;
    }
    
    mbot_lcm_msgs::pose2D_t currentPose(void)
    {
        assert(!odomTrace_.empty());
        
        mbot_lcm_msgs::pose2D_t odomPose = odomTrace_.back();
        mbot_lcm_msgs::pose2D_t pose;
        pose.x = (odomPose.x * std::cos(odomToGlobalFrame_.theta)) - (odomPose.y * std::sin(odomToGlobalFrame_.theta)) 
            + odomToGlobalFrame_.x;
        pose.y = (odomPose.x * std::sin(odomToGlobalFrame_.theta)) + (odomPose.y * std::cos(odomToGlobalFrame_.theta))
            + odomToGlobalFrame_.y;
        pose.theta = angle_sum(odomPose.theta, odomToGlobalFrame_.theta);
        
        return pose;
    }

    void subscribeToLcm()
    {
        lcmInstance->subscribe(ODOMETRY_CHANNEL, &MotionController::handleOdometry, this);
        lcmInstance->subscribe(SLAM_POSE_CHANNEL, &MotionController::handlePose, this);
        lcmInstance->subscribe(CONTROLLER_PATH_CHANNEL, &MotionController::handlePath, this);
        lcmInstance->subscribe(MBOT_TIMESYNC_CHANNEL, &MotionController::handleTimesync, this);
    }
};

int main(int argc, char** argv)
{
    lcm::LCM lcmInstance(MULTICAST_URL);
    MotionController controller(&lcmInstance);

    signal(SIGINT, exit);
    
    while(true)
    {
        lcmInstance.handleTimeout(50);  // update at 20Hz minimum

    	if(controller.timesync_initialized()){
            mbot_lcm_msgs::twist2D_t cmd = controller.updateCommand();
            // std::cout << cmd.utime << " " << cmd.vx << " " <<cmd.vy << " " << cmd.wz << std::endl;
            // Limit command values
            // Fwd vel
            // if (cmd.vx > 0.3) cmd.vx = 0.3;
            // else if (cmd.vx < -0.3) cmd.vx = -0.3;

            // Angular vel
            float max_ang_vel = M_PI * 2.0 / 3.0;
            if (cmd.wz > max_ang_vel) cmd.wz = max_ang_vel;
            else if (cmd.wz < -max_ang_vel) cmd.wz = -max_ang_vel;

            // printf("%f\t%f\n", cmd.vx, cmd.wz);
            
            lcmInstance.publish(MBOT_MOTOR_COMMAND_CHANNEL, &cmd);
    	}
    }
    
    return 0;
}