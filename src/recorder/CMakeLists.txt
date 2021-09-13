cmake_minimum_required(VERSION 3.10)

# set the project name
project(Recorder LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# specify the C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

find_package(OpenCV REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(glfw3 3.2 REQUIRED)
find_package(OpenGL 3.0 REQUIRED)
find_package(GLEW REQUIRED)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS record_data.proto)


add_library(util STATIC util.h util.cc)
target_link_libraries(util glfw OpenGL::OpenGL ${OpenCV_LIBRARIES} ${GLEW_LIBRARIES})

add_executable(recorder_main recorder_main.cc ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(recorder_main ${OpenCV_LIBRARIES} ${Protobuf_LIBRARIES} ${Boost_LIBRARIES} glog gflags)

add_executable(data_inspector data_inspector.cc ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(data_inspector glfw OpenGL::OpenGL ${OpenCV_LIBRARIES} ${GLEW_LIBRARIES} ${Protobuf_LIBRARIES} ${Boost_LIBRARIES} gflags glog util)
