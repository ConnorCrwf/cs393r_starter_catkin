//========================================================================
//  This software is free: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License Version 3,
//  as published by the Free Software Foundation.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  Version 3 in the file COPYING that came with this distribution.
//  If not, see <http://www.gnu.org/licenses/>.
//========================================================================
/*!
\file    particle-filter.cc
\brief   Particle Filter Starter Code
\author  Joydeep Biswas, (C) 2019
*/
//========================================================================

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include "eigen3/Eigen/Dense"
#include "eigen3/Eigen/Geometry"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "shared/math/geometry.h"
#include "shared/math/line2d.h"
#include "shared/math/math_util.h"
#include "shared/util/timer.h"

#include "config_reader/config_reader.h"
#include "particle_filter.h"

#include "vector_map/vector_map.h"

using math_util::DegToRad;
using math_util::RadToDeg;
using math_util::AngleDiff;
using geometry::line2f;
using std::cout;
using std::endl;
using std::string;
using std::swap;
using std::vector;
using Eigen::Vector2f;
using Eigen::Vector2i;
using vector_map::VectorMap;

DEFINE_double(num_particles, 50, "Number of particles");

namespace {
  int updates_since_last_resample_ = 0;
  Vector2f last_update_loc_(0,0);
} // namespace

namespace particle_filter {

config_reader::ConfigReader config_reader_({"config/particle_filter.lua"});

ParticleFilter::ParticleFilter() :
    prev_odom_loc_(0, 0),
    prev_odom_angle_(0),
    odom_initialized_(false),
    var_obs_(1.0), // variance of lidar, to be tuned
    d_short_(0.5), // was 0.1
    d_long_(0.5){} // was 0.3

void ParticleFilter::GetParticles(vector<Particle>* particles) const {
  *particles = particles_;
}

// Return intersection points in a known map with a given pose
void ParticleFilter::GetPredictedPointCloud(const Vector2f& loc,
                                            const float angle,
                                            int num_ranges,
                                            float range_min,
                                            float range_max,
                                            float angle_min,
                                            float angle_max,
                                            vector<Vector2f>* scan_ptr) {
  vector<Vector2f>& scan = *scan_ptr;

  // Note: The returned values must be set using the `scan` variable:
  scan.resize(num_ranges/10);

  Vector2f lidar_loc = loc + 0.2*Vector2f( cos(angle), sin(angle) );
  
  // Sweeps through angles of virtual Lidar and returns closest point
  for (size_t i_scan = 0; i_scan < scan.size(); i_scan++)
  {
    // Initialize scan, to be updated later
    scan[i_scan] = Vector2f(0, 0);
    // Get the visual "ray" vector for this particular scan
    line2f ray_line(1,2,3,4); // Line segment from (1,2) to (3,4)
    float ray_angle = angle + 10.0*i_scan/num_ranges*(angle_max-angle_min) + angle_min;
    ray_line.p0.x() = lidar_loc.x() + range_min*cos(ray_angle);
    ray_line.p0.y() = lidar_loc.y() + range_min*sin(ray_angle);
    ray_line.p1.x() = lidar_loc.x() + range_max*cos(ray_angle);
    ray_line.p1.y() = lidar_loc.y() + range_max*sin(ray_angle);
    
    // Initialize variables for next loop
    Vector2f intersection_min;
    intersection_min.x() = lidar_loc.x() + range_max*cos(ray_angle);
    intersection_min.y() = lidar_loc.y() + range_max*sin(ray_angle);
    float dist_to_intersection_min = range_max;
    // Sweeps through lines in map to get closest intersection with ray
    for (size_t i_line = 0; i_line < map_.lines.size(); ++i_line)
    {
      const line2f map_line = map_.lines[i_line];
      Vector2f intersection_point;
      bool intersects = map_line.Intersection(ray_line, &intersection_point);
      // If there is an intersection, examine this point
      if (intersects)
      {
        float dist_to_this_intersection = (intersection_point-loc).norm();
        // If it's the closest point yet, record it
        if (dist_to_this_intersection < dist_to_intersection_min)
        {
          dist_to_intersection_min = dist_to_this_intersection;
          intersection_min = intersection_point;
        }
      }
    }
    // Return closest point for this particular scan (map frame)
    scan[i_scan] = intersection_min;

    // Optional: If you just want the range
    // scan[i_scan] = dist_to_intersection_min;
    
    // Optional: In base_link frame (untested):
    // scan[i_scan] = Map2BaseLink(intersection_min, loc, angle);
  }
}

// Update weight of a given particle based on how well it fits map
void ParticleFilter::Update(const vector<float>& ranges,
                            float range_min,
                            float range_max,
                            float angle_min,
                            float angle_max,
                            Particle* p_ptr) {
  Particle& particle = *p_ptr;

  if (not odom_initialized_) return;

  // Get predicted point cloud
  vector<Vector2f> predicted_cloud;
  GetPredictedPointCloud(particle.loc, particle.angle,
                         ranges.size(),
                         range_min, range_max,
                         angle_min, angle_max,
                         &predicted_cloud);

  // Resize ranges to match predicted size
  int ratio = ranges.size() / predicted_cloud.size();
  vector<float> trimmed_ranges(predicted_cloud.size());
  for (size_t i = 0; i < predicted_cloud.size(); i++)
  {
    trimmed_ranges[i] = ranges[ratio*i];
  }

  // Calculate Particle Weight (pure Gaussian to start off)
  float log_error_sum = 0;
  for (size_t i = 0; i < predicted_cloud.size(); i++)
  {
    Vector2f predicted_point = predicted_cloud[i];
    Vector2f particle_lidar_loc = particle.loc + 0.2*Vector2f( cos(particle.angle), sin(particle.angle) );
    float predicted_range = (predicted_point-particle_lidar_loc).norm();

    // Discount any erronious readings at or exceeding the limits of the lidar range
    if (predicted_range > range_max       or predicted_range < range_min
           or ranges[i] > 0.95*range_max  or ranges[i] <  1.05*range_min) continue;

    // New implementation of piecewise function of d_short and d_long
    float range_diff = trimmed_ranges[i] - predicted_range;
    range_diff = std::min(range_diff, d_long_);
    range_diff = std::max(range_diff,-d_short_);

    log_error_sum += -Sq(range_diff) / var_obs_;
  }

  particle.log_weight += log_error_sum; //gamma is 1
}

// Resample particles to duplicate good ones and get rid of bad ones
void ParticleFilter::Resample() {
  // Check whether particles have been initialized
  if (particles_.empty() or not odom_initialized_) return;

  // Initialize Local Variables (static for speed in exchange for memory)
  vector<Particle> new_particles;                                         // temp variable to house new particles
 // static vector<float> normalized_log_weights(FLAGS_num_particles);     // vector of log(w/w_max) = log(w) - log(w_max)
  static vector<float> absolute_weight_breakpoints(FLAGS_num_particles);  // vector of cumulative absolute normalized weights
  float normalized_sum = 0;                                               // sum of normalized (but NOT log) weights: used for resampling

  // Normalize each of the log weights
  for (size_t i=0; i < FLAGS_num_particles; i++){
    particles_[i].log_weight -= max_log_particle_weight_;
    normalized_sum += exp(particles_[i].log_weight);
    absolute_weight_breakpoints[i] = normalized_sum;
  }

  // Resample based on the absolute weights
  float division_size = normalized_sum / FLAGS_num_particles;
  float sample_point = rng_.UniformRandom(0,division_size);

  if (division_size == 0) return;

  for (size_t i=0; i < FLAGS_num_particles; i++){
    while (absolute_weight_breakpoints[i] > sample_point){
      new_particles.push_back(particles_[i]);
      sample_point += division_size;
    }
  }

  max_log_particle_weight_ = 0;
  particles_ = new_particles;
}

// Done by Alex
// Called by LaserCallback in particle_filter_main
void ParticleFilter::ObserveLaser(const vector<float>& ranges,
                                  float range_min,
                                  float range_max,
                                  float angle_min,
                                  float angle_max) {
  // A new laser scan observation is available (in the laser frame)
  // Since the range of weights is (-inf,0] we have to initialize max at -inf
  max_log_particle_weight_ = -std::numeric_limits<float>::infinity();

  float dist_since_last_update = (prev_odom_loc_ - last_update_loc_).norm();
  // If we've moved at least 0.1m but haven't moved over 1m (filters out new initialization and timing errors)
  if (dist_since_last_update > 0.1 and dist_since_last_update < 1.0)
  {
    // Update all particle weights and find the maximum weight
    for (auto &particle : particles_)
    {
      Update(ranges, range_min, range_max, angle_min, angle_max, &particle);
      if (particle.log_weight > max_log_particle_weight_) max_log_particle_weight_ = particle.log_weight;
    }
    last_update_loc_ = prev_odom_loc_;
    // cout << "UPDATE" << endl;

    // Resample every n updates
    if (updates_since_last_resample_ > 5){
      Resample();
      // cout << "RESAMPLE" << endl;
      updates_since_last_resample_ = 0;
    }
    updates_since_last_resample_ ++;
  }
}

// 
void ParticleFilter::ObserveOdometry(const Vector2f& odom_loc,
                                     const float odom_angle) {
  // A new odometry value is available (in the odom frame)
  // Implement the motion model predict step here, to propagate the particles
  // forward based on odometry.

  Vector2f odom_trans_diff = odom_loc - prev_odom_loc_;

  // Only executes if odom is initialized and a realistic value
  if (odom_initialized_ and odom_trans_diff.norm() < 1.0)
  {
    float angle_diff = AngleDiff(odom_angle, prev_odom_angle_);
    if (std::abs(angle_diff) > M_2PI)
    {
      // Should never happen, but just in case:
      cout << "Error: reported change in angle exceeds 2pi" << endl;
    }
    for (auto &particle : particles_)
    {
      // Find the transformation between the map and odom frame for this particle
      Eigen::Rotation2Df R_Odom2Map(AngleDiff(particle.angle, prev_odom_angle_));
      Vector2f map_trans_diff = R_Odom2Map * odom_trans_diff;
      // Apply noise to pose of particle
      UpdateParticleLocation(map_trans_diff, angle_diff, &particle);
    }
    prev_odom_loc_ = odom_loc;
    prev_odom_angle_ = odom_angle;
  }
  else
  {
    ResetOdomVariables(odom_loc, odom_angle);
    odom_initialized_ = true;
    cout << "Odom reset due to initialization or large movement." << endl;
  }
}

// Update a given particle with random noise based on recent movement
void ParticleFilter::UpdateParticleLocation(Vector2f odom_trans_diff, float dtheta_odom, Particle* p_ptr)
{
  // Use the motion model to update each particle's location
  // This function will probably be called in the ObserveOdometry callback
  // You can update the particle location directly by modifying the particle variable
  // defined above since it was passed by reference (using the "&" symbol).
  // this particle passed by reference comes from ObserveLaser for loop
  // and is modified by Update function similar to how it is being modified here
  // but this occurs at every timestep

  // Noise constants to tune
  const float k1 = 0.50;  // translation error per unit translation (suggested: 0.1-0.2)
  const float k2 = 0.25;  // translation error per unit rotation (suggested: 0.01)
  const float k3 = 0.50;  // angular error per unit translation (suggested: 0.02-0.1)
  const float k4 = 0.75;  // angular error per unit rotation (suggested: 0.05-0.2)
  
  Particle& particle = *p_ptr;
  const float angle_diff = dtheta_odom;

  // TODO: add noise to x and y based on movement in that dimension
  const float translation_noise_x = rng_.Gaussian(0.0, k1*odom_trans_diff.norm() + k2*abs(angle_diff));
  const float translation_noise_y = rng_.Gaussian(0.0, k1*odom_trans_diff.norm() + k2*abs(angle_diff));
  const float rotation_noise = rng_.Gaussian(0.0, k3*odom_trans_diff.norm() + k4*abs(angle_diff));
  particle.loc += odom_trans_diff + Vector2f(translation_noise_x, translation_noise_y);
  particle.angle += dtheta_odom + rotation_noise;
}

// Called by InitCallback in particle_filter_main
void ParticleFilter::Initialize(const string& map_file,
                                const Vector2f& loc,
                                const float angle) {
  // The "set_pose" button on the GUI was clicked, or an initialization message
  // was received from the log. Initialize the particles accordingly, e.g. with
  // some distribution around the provided location and angle.
  particles_.clear(); // Need to get rid of particles from previous inits
  map_.Load("maps/" + map_file + ".txt");
  odom_initialized_ = false;
  ResetOdomVariables(loc, angle);

  // Make initial guesses (particles) based on a Gaussian distribution about initial placement
  for (size_t i = 0; i < FLAGS_num_particles; i++){
    Particle particle_init;
    particle_init.loc.x() = rng_.Gaussian(loc.x(), 0.25);  // std_dev of 0.25m, to be tuned
    particle_init.loc.y() = rng_.Gaussian(loc.y(), 0.25);  // std_dev of 0.25m, to be tuned
    particle_init.angle   = rng_.Gaussian(angle, M_PI/6);  // std_dev of 30deg, to be tuned
    particle_init.log_weight = 0;
    particles_.push_back(particle_init);
  }  
}

// Called when new pose is set or robot is moved substantially ("kidnapped")
void ParticleFilter::ResetOdomVariables(const Vector2f loc, const float angle) {
  init_offset_angle_ = angle - prev_odom_angle_;
  last_update_loc_ = loc;
  prev_odom_loc_ = loc;
  prev_odom_angle_ = angle;
  updates_since_last_resample_ = 0;
}

// Called by OdometryCallback in particle_filter_main
void ParticleFilter::GetLocation(Eigen::Vector2f* loc_ptr, 
                                 float* angle_ptr) const {
  Vector2f& loc = *loc_ptr;
  float& angle = *angle_ptr;
  // Compute the best estimate of the robot's location based on the current set
  // of particles. The computed values must be set to the `loc` and `angle`
  // variables to return them.

  // Just do weighted average of loc and angle
  Vector2f weighted_loc_sum(0.0, 0.0);
  float weighted_angle_sum = 0.0;
  float weight_sum = 0.0;
  for (auto &particle : particles_)
  {
    // Convert from log weight to normalized weight
    float normalized_log_weight = particle.log_weight - max_log_particle_weight_;
    float normalized_weight = exp(normalized_log_weight);
    weighted_loc_sum += particle.loc * normalized_weight;
    weighted_angle_sum += particle.angle * normalized_weight;
    weight_sum += normalized_weight;
  }
  loc = weighted_loc_sum / weight_sum;
  angle = weighted_angle_sum / weight_sum;
}

// Helper function to convert from map to base_link, untested
Vector2f ParticleFilter::Map2BaseLink(const Vector2f& point, const Vector2f& loc, const float angle){
  Eigen::Rotation2Df R_inv(-angle); // negative of angle should be the same as transpose or inverse, right?
  Vector2f lidar_reading = R_inv*(point-loc); // transformation to lidar frame
  Vector2f lidar_offset(0.2, 0);
  return lidar_reading - lidar_offset; // transformation to base_link frame
}

Vector2f ParticleFilter::OdomVec2Map(const Vector2f odom_vec){
  Eigen::Rotation2Df R_MB(init_offset_angle_);
  return R_MB * odom_vec;
}

}  // namespace particle_filter
