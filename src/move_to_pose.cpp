

#include "move_to_pose.hpp"

MoveToPoseController::MoveToPoseController(float Kp_rho, float Kp_alpha, 
    float Kp_beta) :  Kp_rho_(Kp_alpha), 
    Kp_alpha_(Kp_alpha), Kp_beta_(Kp_beta) {}

Control MoveToPoseController::moveToPose(const Pose& curr)  {
    Control u {0.f, 0.f};

    float dx = goal_.x - curr.x;
    float dy = goal_.y - curr.y;

    // clip angles to [-pi, pi]
    float rho = std::hypot(dx, dy);
    float alpha = fmod(std::atan2(dy, dx) - curr.theta + M_PI, 2 * M_PI) - M_PI;
    float beta  = fmod(goal_.theta - curr.theta - alpha + M_PI, 2 * M_PI) - M_PI;

    float angle_diff = fmod(goal_.theta - curr.theta + M_PI, 2 * M_PI) - M_PI;

    if (rho < 0.1 && std::abs(angle_diff) < 5.0f * M_PI/180.f ) {
        arrived_ = true;
        return u;
    }

    u.v = Kp_rho_ * rho;
    u.w = Kp_alpha_ * alpha + Kp_beta_ * beta;

    if (alpha > M_PI/2.f || alpha < -M_PI/2.f )
        u.v = -u.v;

    return u;
}




int main(){

    Gnuplot gp;
    gp << "set xrange [-1:21]\nset yrange [-1:21]\n";


    // params
    float Kp_rho = 9.0f;
    float Kp_alpha = 15.0f;
    float Kp_beta = -3.0f;
    float dt = 0.01f;

    // random starting location
    std::random_device random_device{};
    std::mt19937 generator{random_device()};
    std::uniform_real_distribution<float> unif_dist(0.0f, 1.0f);

    // generate start and goal location
    Pose start {
        20.0f * unif_dist(generator),
        20.0f * unif_dist(generator),
        2.0f * static_cast<float>(M_PI) * unif_dist(generator) - static_cast<float>(M_PI)
    };

    Pose goal {
        20.0f * unif_dist(generator),
        20.0f * unif_dist(generator),
        2.0f * static_cast<float>(M_PI) * unif_dist(generator) - static_cast<float>(M_PI)
    };

    // initialize controller and set goal
    MoveToPoseController controller(Kp_rho, Kp_alpha, Kp_beta);
    controller.setGoal(goal);

    Path path;
    gp << "set size ratio 1.0\n";
    gp << "set term gif animate\n";
    gp << "set output '../animations/move_to_pose.gif'\n";


    Pose curr = start;
    while (controller.hasArrived() == false) {
        Control u = controller.moveToPose(curr);
        curr.theta = curr.theta + u.w * dt;
        curr.x = curr.x + u.v * std::cos(curr.theta) * dt;
        curr.y = curr.y + u.v * std::sin(curr.theta) * dt;

        path.emplace_back(curr.x, curr.y);

        gp << "plot '-' with vectors title 'goal' lw 2,"
                   "'-' with vectors title 'start' lw 2,"
                   "'-' with vectors title 'pose' lw 3,"
                   "'-' title 'path' with lines \n";
        gp.send1d(poseToArrow(goal, 0.5));
        gp.send1d(poseToArrow(start, 0.5));
        gp.send1d(poseToArrow(curr, 0.5));
        gp.send1d(path);
        // sleep(1);
    }
    
    gp << "set output\n";


}