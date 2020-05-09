#include "path_planning/grid_search.hpp"



// Base class
GridSearchBase::GridSearchBase (std::vector<int>& ox,  std::vector<int>& oy, 
            float grid_res,  float robot_radius) : 
        grid_res_(grid_res), robot_radius_(robot_radius),
        ox_(ox), oy_(oy) {
    calculateObstacleMap(ox, oy);
}


std::pair<std::vector<int>, std::vector<int>> GridSearchBase::getExpandedNodes() {
    return {expanded_x_, expanded_y_};
}

void GridSearchBase::calculateObstacleMap( std::vector<int>& ox,  std::vector<int>& oy) {
    auto [min_x, max_x] = std::minmax_element(ox.begin(), ox.end());
    min_x_ = std::round(*min_x);
    max_x_ = std::round(*max_x);

    auto [min_y, max_y] = std::minmax_element(oy.begin(), oy.end());
    min_y_ = std::round(*min_y);
    max_y_ = std::round(*max_y);

    x_width_ = std::round( (max_x_ - min_x_ ) /   grid_res_     );
    y_width_ = std::round( (max_y_ - min_y_ ) /   grid_res_     );

    obstacle_map_ = std::vector<std::vector<bool>>(x_width_, std::vector<bool>(y_width_, false));
    // apply inflation
    // at each grid cell iterate through all obstacles and check if within radius
    int iox, ioy; // obs grid cell
    int x, y; // grid cell
    float d;
    for (int ix = 0; ix < x_width_; ++ix) {
        x = calculatePosition(ix, min_x_);
        for (int iy = 0; iy < y_width_; ++iy) {
            y = calculatePosition(iy, min_y_);

            for (size_t obs_num = 0; obs_num < ox.size(); ++obs_num) {
                iox = ox[obs_num], ioy = oy[obs_num];
                d = std::hypot(iox - x, ioy - y);
                if (d <= robot_radius_) {
                    obstacle_map_[ix][iy] = true;
                    break;
                }
            }
        }
    }
}

std::pair<std::vector<int>, std::vector<int>> GridSearchBase::calculateFinalPath(Node goal_node, 
                                        std::unordered_map<int, Node>& closed_set) {
        // for plotting   
    std::vector<int> rx;
    std::vector<int> ry;  

    rx.push_back(calculatePosition(goal_node.x, min_x_)); 
    ry.push_back(calculatePosition(goal_node.y, min_y_));
    int parent = goal_node.parent;

    while (parent != -1) {
        Node n = closed_set[parent];
        rx.push_back(calculatePosition(n.x, min_x_));
        ry.push_back(calculatePosition(n.y, min_y_));
        parent = n.parent;
    }

    return std::make_pair(rx, ry);                                        
}


int GridSearchBase::calculateXYIndex(int pos, int min_pos) {
    return std::round((pos - min_pos) / grid_res_);
}


int GridSearchBase::calculateIndex(Node& node) {
    return (node.y - min_y_) * x_width_ + (node.x - min_x_);
}

float GridSearchBase::calculatePosition(int index, float minp) {
    float pos = index * grid_res_ + minp;
    return pos;
}

bool GridSearchBase::verifyNode(Node& node) {
    float px = calculatePosition(node.x, min_x_);
    float py = calculatePosition(node.y, min_y_);

    if (px < min_x_)
        return false;
    if (py < min_y_)
        return false;
    if (px >= max_x_)
        return false;
    if (py >= max_y_)
        return false;

    if (obstacle_map_[node.x][node.y])
        return false;

    return true;
}



// Dijstra
Dijkstra::Dijkstra(std::vector<int>& ox,  std::vector<int>& oy, 
            float grid_res,  float robot_radius) : 
            GridSearchBase(ox, oy, grid_res, robot_radius) {}

std::pair<std::vector<int>, std::vector<int>> 
    Dijkstra::plan(float sx, float sy, float gx, float gy) {

    Node start_node(calculateXYIndex(sx, min_x_), 
                    calculateXYIndex(sy, min_y_), 0.0f, -1);
    Node goal_node(calculateXYIndex(gx, min_x_), 
                    calculateXYIndex(gy, min_y_), 0.0f, -1);
    
    std::unordered_map<int, Node> open_set;
    std::unordered_map<int, Node> closed_set;

    open_set[calculateIndex(start_node)] = start_node;


    while (true) {
        auto next_node = std::min_element(open_set.begin(), open_set.end(), 
        [&](const auto& p1, const auto& p2){
            return p1.second.cost < p2.second.cost;
        });

        int c_id = (*next_node).first;
        Node current = (*next_node).second;

        
        float wx = calculatePosition(current.x, min_x_);
        float wy = calculatePosition(current.y, min_y_);
        expanded_x_.push_back(wx);
        expanded_y_.push_back(wy);

        // check goal
        if (current.x == goal_node.x && current.y == goal_node.y) {
            std::cout << "Goal found!" << std::endl;
            goal_node.parent = current.parent;
            goal_node.cost = current.cost;
            break;
        }

        // remove from open set and add to closed set
        open_set.erase(c_id);
        closed_set[c_id] = current;

        // visit neighbours
        for (auto [dx, dy, cost] : motion_model_) {
            Node node(current.x + dx, current.y + dy, current.cost + cost, c_id);
            int n_id = calculateIndex(node);

            if (closed_set.find(n_id) != closed_set.end()) 
                continue;

            if (verifyNode(node) == false)
                continue;

            // add to open set if not there, update open set if better cost found
            if (open_set.find(n_id) == open_set.end() || open_set[n_id].cost >= node.cost) {
                open_set[n_id] = node;
            } 
        }
    }

    auto [rx, ry] = calculateFinalPath(goal_node, closed_set);

    return {rx, ry};

}




// Astar
Astar::Astar(std::vector<int>& ox,  std::vector<int>& oy, 
            float grid_res,  float robot_radius) : 
            GridSearchBase(ox, oy, grid_res, robot_radius) {}

 std::pair<std::vector<int>, std::vector<int>> 
    Astar::plan(float sx, float sy, float gx, float gy)  {
    
    Node start_node(calculateXYIndex(sx, min_x_), 
                    calculateXYIndex(sy, min_y_), 0.0f, -1);
    Node goal_node(calculateXYIndex(gx, min_x_), 
                    calculateXYIndex(gy, min_y_), 0.0f, -1);
    
    std::unordered_map<int, Node> open_set;
    std::unordered_map<int, Node> closed_set;

    open_set[calculateIndex(start_node)] = start_node;


    while (true) {
        auto next_node = std::min_element(open_set.begin(), open_set.end(), 
        [&](const auto& p1, const auto& p2){
            return p1.second.cost + calculateHeuristic(p1.second, goal_node) < 
                p2.second.cost + calculateHeuristic(p2.second, goal_node);
        });

        int c_id = (*next_node).first;
        Node current = (*next_node).second;

        
        float wx = calculatePosition(current.x, min_x_);
        float wy = calculatePosition(current.y, min_y_);
        expanded_x_.push_back(wx);
        expanded_y_.push_back(wy);


        // check goal
        if (current.x == goal_node.x && current.y == goal_node.y) {
            std::cout << "Goal found!" << std::endl;
            goal_node.parent = current.parent;
            goal_node.cost = current.cost;
            break;
        }

        // remove from open set and add to closed set
        open_set.erase(c_id);
        closed_set[c_id] = current;

        // visit neighbours
        for (auto [dx, dy, cost] : motion_model_) {
            Node node(current.x + dx, current.y + dy, current.cost + cost, c_id);
            int n_id = calculateIndex(node);

            if (closed_set.find(n_id) != closed_set.end()) 
                continue;

            if (verifyNode(node) == false)
                continue;

            // add to open set if not there, update open set if better cost found
            if (open_set.find(n_id) == open_set.end() || open_set[n_id].cost >= node.cost) {
                open_set[n_id] = node;
            } 
        }
    }

    auto [rx, ry] = calculateFinalPath(goal_node, closed_set);

    return {rx, ry};

}


float Astar::calculateHeuristic(const Node& n1, const Node& n2) {
    return std::hypot(n1.x - n2.x, n1.y - n2.y);
}



