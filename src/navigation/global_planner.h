#ifndef GLOBAL_PLANNER_CS393R_HH
#define GLOBAL_PLANNER_CS393R_HH

#include "eigen3/Eigen/Dense"
#include "amrl_msgs/VisualizationMsg.h"
#include "glog/logging.h"
#include "ros/ros.h"
#include "stdio.h"

#include "shared/math/geometry.h"
#include "shared/math/line2d.h"
#include "shared/math/math_util.h"
#include "shared/util/timer.h"
#include "visualization/visualization.h"
#include "vector_map/vector_map.h"
#include "navigation/simple_queue.h"

struct Neighbor{
  Eigen::Vector2i node_index;
  std::string key;
  float path_length;
  int neighbor_index;
};

struct Node{
  Eigen::Vector2f loc;                  // Location of node
  Eigen::Vector2i index;                // Index of node
  float cost;                           // Total path cost up to this node (NOTE: not edge cost)
  std::string parent;                   // Parent of the node on the optimal path                
  std::vector<Neighbor> neighbors;      // List of all valid adjacent nodes
  std::string key;                      // Unique identifier
};

class GlobalPlanner{

public:
	// Default Constructor
	GlobalPlanner(){}
	// Set the map resolution
	void setResolution(float resolution);
	// Initialize the navigation map at the start point and update the planner resolution
	void initializeMap(Eigen::Vector2f start_loc);
	// Instantiate a new node as a child of another node
	Node newNode(const Node &old_node, int neighbor_index);
	// Check if travel from Node A to Node B is valid
	bool isValidNeighbor(const Node &node, const Neighbor &neighbor);
	// Find the travel cost bewteen two nodes
	float edgeCost(const Node &node_A,const Node &node_B);
	// Update valid neighbors and edge costs
	void visitNode(Node &node);
	// Get the best sequence of node keys to the nav_goal_ point
	std::vector<std::string> getGlobalPath(Eigen::Vector2f nav_goal_loc);

	// Visualization
	void plotNodeNeighbors(const Node &node, amrl_msgs::VisualizationMsg &msg);
	void visualizeMap();

private:

	// Helper Functions
	std::string getNewID(int xi, int yi);
  	std::vector<Neighbor> getNeighbors(const Node &node);
  	std::array<geometry::line2f,2> getCushionLines(geometry::line2f edge, float offset);

	// Navigation map (key, Node)
	std::map<std::string, Node> nav_map_;
	// Horizontal/vertical distance between two adjacent nodes
	float map_resolution_;
	// Priority Queue (key, priority)
	SimpleQueue<std::string, float> frontier_;
	// Blueprint map of the environment
	vector_map::VectorMap map_;
};

#endif
