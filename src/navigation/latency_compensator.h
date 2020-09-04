#include <list>
#include "ros/ros.h"
#include "eigen3/Eigen/Dense"
#include "geometry_msgs/Pose2D.h"

class LatencyCompensator {
public:
  LatencyCompensator(float actuation_delay, float observation_delay, float delta_t);
  void recordNewInput(float x_dot, float y_dot, float omega);
  void recordObservation();
  geometry_msgs::Pose2D predictedState(Eigen::Vector2f Loc, float theta);

private:
  std::list< std::array<float,4> > recordedInputs_; // Record of past inputs in the form (curvature, speed, timestamp)
  float actuation_delay_;                           // Robot actuation delay (seconds)
  float observation_delay_;                         // Sensor observation delay (seconds)
  float system_delay_;                              // Total system delay (ignoring contoller delay) (seconds)
  float last_observation_time_;                     // Timestamp for when the last sensor state came in 
  float delta_t_;                                   // Duration of a control loop for the system being compensated
  float wheelbase_;                                 // Used in forward kinematics
};