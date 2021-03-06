# Catkin User Guide: http://www.ros.org/doc/groovy/api/catkin/html/user_guide/user_guide.html
# Catkin CMake Standard: http://www.ros.org/doc/groovy/api/catkin/html/user_guide/standards.html
cmake_minimum_required(VERSION 2.8.3)
project(cs393r_starter)
# Load catkin and all dependencies required for this package
# TODO: remove all from COMPONENTS that are not catkin packages.
find_package(catkin REQUIRED COMPONENTS std_msgs nav_msgs geometry_msgs visualization_msgs sensor_msgs roscpp rosbag tf ut_automata tf2_ros)

include_directories(include ${Boost_INCLUDE_DIR} ${catkin_INCLUDE_DIRS})
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

MESSAGE(STATUS "Compiling Reference Car Localization + Navigation")
MESSAGE(STATUS "Using compiler: ${CMAKE_CXX_COMPILER}")
MESSAGE(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
MESSAGE(STATUS "Arch: ${CMAKE_SYSTEM_PROCESSOR}")

SET(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Werror")

IF(${CMAKE_BUILD_TYPE} MATCHES "Release")
  MESSAGE(STATUS "Additional Flags for Release mode")
  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fopenmp -O2 -DNDEBUG")
ELSEIF(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  MESSAGE(STATUS "Additional Flags for Debug mode")
  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g")
ENDIF()

# INCLUDE($ENV{ROS_ROOT}/core/rosbuild/rosbuild.cmake)
# ROSBUILD_INIT()
# SET(ROS_BUILD_STATIC_LIBS true)
# SET(ROS_BUILD_SHARED_LIBS false)

MESSAGE(STATUS "ROS-Overrride Build Type: ${CMAKE_BUILD_TYPE}")
MESSAGE(STATUS "CXX Flags: ${CMAKE_CXX_FLAGS}")

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

SET(libs ${catkin_LIBRARIES} glog gflags amrl-shared-lib boost_system lua5.1
    pthread)

ADD_LIBRARY(shared_library
            src/visualization/visualization.cc
            src/vector_map/vector_map.cc)

ADD_SUBDIRECTORY(src/shared)
INCLUDE_DIRECTORIES(src/shared)
INCLUDE_DIRECTORIES(src)

add_executable(slam
                        src/slam/slam_main.cc
                        src/slam/slam.cc
                        src/slam/CellGrid.cpp)
TARGET_LINK_LIBRARIES(slam shared_library ${libs})


add_executable(particle_filter
                        src/particle_filter/particle_filter_main.cc
                        src/particle_filter/particle_filter.cc)
TARGET_LINK_LIBRARIES(particle_filter shared_library ${libs})

add_executable(navigation
                        src/navigation/navigation_main.cc
                        src/navigation/navigation.cc
                        src/navigation/local_planner.cc
                        src/navigation/global_planner.cc
                        src/navigation/latency_compensator.cc
                        src/navigation/human.cc)
TARGET_LINK_LIBRARIES(navigation shared_library ${libs})

add_executable(measure_latency
                        src/navigation/measureLatency.cpp)
TARGET_LINK_LIBRARIES(measure_latency shared_library ${libs})

add_executable(odometry_broadcaster
                        src/tf/odometry_broadcaster.cpp)
TARGET_LINK_LIBRARIES(odometry_broadcaster shared_library ${libs})

ADD_EXECUTABLE(pq_tutorial
               src/navigation/pq_tutorial.cc)
ADD_EXECUTABLE(eigen_tutorial
               src/eigen_tutorial.cc)## Generate added messages and services with any dependencies listed here
#generate_messages(
#    #TODO DEPENDENCIES geometry_msgs std_msgs
#)

# catkin_package parameters: http://ros.org/doc/groovy/api/catkin/html/dev_guide/generated_cmake_api.html#catkin-package
# TODO: fill in what other packages will need to use this package
catkin_package(
    DEPENDS std_msgs nav_msgs geometry_msgs visualization_msgs sensor_msgs roscpp rosbag tf ut_automata tf2_ros
    CATKIN_DEPENDS # TODO
    INCLUDE_DIRS # TODO include
    LIBRARIES # TODO
)