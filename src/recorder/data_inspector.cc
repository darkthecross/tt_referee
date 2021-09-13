#include <fstream>

#include <unistd.h>
#include "record_data.pb.h"
#include "opencv2/opencv.hpp"
#include "opencv2/core.hpp"
#include "gflags/gflags.h"
#include <boost/filesystem.hpp>
#include <glog/logging.h>
#include <thread>

#include "util.h"

DEFINE_string(data_path, "",
              "path to RecordData binary proto.");

cv::Mat DeserializeImage(const std::string &str)
{
    std::vector<uchar> bytes(str.begin(), str.end());
    return cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action,
                         int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
}

int main(int argc, char **argv)
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if (FLAGS_data_path.empty())
    {
        LOG(ERROR) << "Empty data_path!";
        return -1;
    }

    cv::Mat left_frame = cv::Mat::zeros(1280, 720, CV_8UC3);
    cv::Mat right_frame = cv::Mat::zeros(1280, 720, CV_8UC3);
    cv::Mat full_frame = cv::Mat::zeros(2560, 720, CV_8UC3);

    std::mutex frame_mutex, update_mutex, ts_mutex;
    bool need_update = false;

    // Initialize data_cache
    recorder::RecordData record_data;
    std::fstream input(FLAGS_data_path, std::ios::in | std::ios::binary);
    if (!record_data.ParseFromIstream(&input))
    {
        LOG(ERROR) << "Failed to parse " << FLAGS_data_path << " to proto.";
        return -1;
    }
    std::map<int64, std::string> cam0_frames, cam1_frames;
    for (const auto &f : record_data.frame_data())
    {
        if (f.camera_id() == 0)
        {
            cam0_frames[f.timestamp()] = f.image();
        }
        else if (f.camera_id() == 1)
        {
            cam1_frames[f.timestamp()] = f.image();
        }
    }

    int64 cur_timestamp = cam0_frames.begin()->first;
    bool running = true;

    auto img_thread = std::thread([&cur_timestamp, &ts_mutex, &need_update, &update_mutex, &cam0_frames, &cam1_frames, &left_frame, &right_frame, &full_frame, &frame_mutex, &running]()
                                  {
                                      int64 prev_timestamp = 0;
                                      while (running)
                                      {
                                          ts_mutex.lock();
                                          update_mutex.lock();
                                          need_update = (prev_timestamp != cur_timestamp);
                                          ts_mutex.unlock();
                                          if (need_update)
                                          {
                                              update_mutex.unlock();
                                              frame_mutex.lock();
                                              auto it_0 = cam0_frames.upper_bound(cur_timestamp);
                                              if (it_0 != cam0_frames.end())
                                              {
                                                  left_frame = DeserializeImage(it_0->second);
                                              }
                                              auto it_1 = cam1_frames.upper_bound(cur_timestamp);
                                              if (it_1 != cam1_frames.end())
                                              {
                                                  right_frame = DeserializeImage(it_1->second);
                                              }
                                              cv::hconcat(left_frame, right_frame, full_frame);
                                              frame_mutex.unlock();
                                              prev_timestamp = cur_timestamp;
                                          }
                                          else
                                          {
                                              update_mutex.unlock();
                                              usleep(1000);
                                          }
                                      }
                                  });

    auto step_thread = std::thread([&running, &cur_timestamp, &cam0_frames, &ts_mutex]()
                                   {
                                       while (running)
                                       {
                                           ts_mutex.lock();
                                           cur_timestamp += 1000;
                                           if(cur_timestamp > cam0_frames.begin()->first + 1000000) {
                                               cur_timestamp = cam0_frames.begin()->first;
                                           }
                                           ts_mutex.unlock();
                                           usleep(1000);
                                       }
                                   });

    if (!glfwInit())
    {
        LOG(ERROR) << "Error initializing glfw! ";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow *window = glfwCreateWindow(2560, 720, "Simple example", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        LOG(ERROR) << "Error initializing glfw window! ";
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetKeyCallback(window, key_callback);

    //  Initialise glew (must occur AFTER window creation or glew will error)
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        LOG(ERROR) << "GLEW initialisation error: " << glewGetErrorString(err);
        exit(-1);
    }
    LOG(INFO) << "GLEW okay - using version: " << glewGetString(GLEW_VERSION);

    util::InitOpenGL(2560, 720);

    while (!glfwWindowShouldClose(window)) // Application still alive?
    {
        util::DrawFrame(full_frame, 2560, 720);
        glfwSwapBuffers(window);
        glfwPollEvents();
        // Limit fps to 110.
        std::this_thread::sleep_for(std::chrono::milliseconds(9));
    }

    running = false;
    img_thread.join();
    step_thread.join();

    return 0;
}