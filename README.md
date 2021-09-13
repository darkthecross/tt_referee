# TT_REFEREE

This is a real time observation system for table tenniss.

## Dependencies

This implementation was originally developed on a Ubuntu machine, and deployed to a Nvidia Jetson Xavier NX. 

This implementation depends on the following libraries:

* OpenCV
* libprotobuf-dev
* glog
* gflags

## Setup guide for Nvidia Jetson Xavier NX

OpenCV comes with Jetpack.

```shell
sudo apt-get install protobuf-compiler libprotobuf-dev
sudo apt-get install libgflags-dev
sudo apt install libgoogle-glog-dev
```

```shell
# opengl, glew, glfw
sudo apt-get -y install libglu1-mesa-dev freeglut3-dev mesa-common-dev libglew-dev libglfw3-dev
```

Also, glog might not work with cmake out of the box. I added [Findglog.cmake](https://raw.githubusercontent.com/ceres-solver/ceres-solver/master/cmake/FindGlog.cmake) to `/usr/share/cmake-3.10/Modules`.

## Hardware

I'm using IMX219 camera module. May need to adjust code for video capturing for other types of sensors.