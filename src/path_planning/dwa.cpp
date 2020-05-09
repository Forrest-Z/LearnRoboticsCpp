#include "path_planning/dwa.hpp"



std::vector<std::pair<float, float>> pointToVector(float x, float y) {
    return { {x, y}};
}


std::vector<std::pair<float, float>> trajectorToVector(const Trajectory& traj) {
    std::vector<std::pair<float, float>> vec;
    vec.reserve(traj.size());
    for (const auto& pose : traj) 
        vec.emplace_back(pose.x, pose.y);
    return vec;
}

DynamicWindow calculateDynamicWindow(const RobotState& x, const Config& cfg) {
    // dw from robot specs
    DynamicWindow vs = {
        cfg.min_speed, 
        cfg.max_speed,
        -cfg.max_yawrate,
        cfg.max_yawrate
    };

    // dw from motion model
    DynamicWindow vd = {
        x.v - cfg.max_accel * cfg.dt,
        x.v + cfg.max_accel * cfg.dt,
        x.w - cfg.max_dyawrate * cfg.dt,
        x.w + cfg.max_dyawrate * cfg.dt
    };

    // final, clip min and max of v and yaw
    DynamicWindow dw = {
        std::max(vs.vmin, vd.vmin),
        std::min(vs.vmax, vd.vmax),
        std::max(vs.yaw_rate_min, vd.yaw_rate_min),
        std::min(vs.yaw_rate_max, vd.yaw_rate_max)
    };

    return dw;

}

RobotState motion(const RobotState& x, const Control& u, float dt) {
    RobotState x_new;
    // update theta first and use this to theta to update (x, y)
    x_new.theta = x.theta + u.w * dt;
    x_new.x = x.x + u.v * std::cos(x_new.theta) * dt;
    x_new.y = x.y + u.v * std::sin(x_new.theta) * dt;
    x_new.v = u.v;
    x_new.w = u.w;

    return x_new;
}




Trajectory predictTrajectory(const RobotState& x_init, const Control& u, const Config& cfg) {
    Trajectory traj = {x_init};
    float time = 0.f;

    RobotState x = x_init;
    while (time <= cfg.predict_time) {
        x = motion(x, u, cfg.dt);
        traj.push_back(x);
        time += cfg.dt;
    }

    return traj;
}



float calculateObstacleCost(const Trajectory& traj, const Obstacles& obs, 
    const Config& cfg) {

    // cost is inversely proportional to closest obs along path
    if (cfg.robot_type == RobotType::circle) {

        float min_r = std::numeric_limits<float>::max();
        for (const RobotState& pose : traj) {
            for (auto [x, y] : obs) {
                float dx = x - pose.x;
                float dy = y - pose.y;
                float r = std::hypot(dx, dy);

                if (r < cfg.robot_radius) // will collide, inf cost
                    return std::numeric_limits<float>::infinity();

                min_r = std::min(r, min_r);
            }
        }
        return 1.0f / min_r;
    } else if (cfg.robot_type == RobotType::rectangle) {
        //TODO support rectangles
    }


    return 0.0f;
}

float calculateToGoalCost(const Trajectory& traj, const Eigen::Vector2f& goal) {
    float dx = goal.x() - traj.back().x;
    float dy = goal.y() - traj.back().y;

    float error_angle = std::atan2(dy, dx);
    float cost_angle = error_angle - traj.back().theta;
    float cost = std::abs(std::atan2( std::sin(cost_angle), std::cos(cost_angle)     ));

    return cost;
}

std::pair<Control, Trajectory> calculateControlAndTrajectory(
    const RobotState& x, const DynamicWindow& dw, 
    const Config& cfg, const Eigen::Vector2f& goal, const Obstacles& obs) {
    
    float min_cost = std::numeric_limits<float>::max();
    Control best_u{0.0f, 0.0f};
    Trajectory best_traj;

    // try all v and w combinations
    for (float v = dw.vmin; v < dw.vmax; v += cfg.v_reso) {
        for (float w = dw.yaw_rate_min; w < dw.yaw_rate_max; w += cfg.yawrate_reso) {
            Control u {v, w};
            Trajectory traj = predictTrajectory(x, u, cfg);

            float to_goal_cost = cfg.to_goal_cost_gain * calculateToGoalCost(traj, goal);
            float speed_cost = cfg.speed_cost_gain * (cfg.max_speed - traj.back().v);
            float ob_cost = cfg.obstacle_cost_gain * calculateObstacleCost(traj, obs, cfg);

            float final_cost = to_goal_cost + speed_cost + ob_cost;

            if (final_cost < min_cost ){
                min_cost = final_cost;
                best_u  = u;
                best_traj = traj;
            }

        }
    }



    return {best_u, best_traj};

}


std::pair<Control, Trajectory> dwaControl(const RobotState& x, 
    const Config& cfg, const Eigen::Vector2f& goal, const Obstacles& obs) {
    
    DynamicWindow dw = calculateDynamicWindow(x, cfg);

    return calculateControlAndTrajectory(x, dw, cfg, goal, obs);
}




