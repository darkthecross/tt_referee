#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

#include "opencv2/opencv.hpp"
#include "record_data.pb.h"
#include <boost/filesystem.hpp>
#include <glog/logging.h>
#include "gflags/gflags.h"

using namespace std::chrono;

constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr int kFrameRate = 60;
constexpr char kDataDir[] = "/media/ssd/tt_referee_data/";

DEFINE_int64(record_duration_seconds, 30,
             "Max num of seconds to record.");

DEFINE_int64(record_blob_seconds, 1,
             "Duration to group data and save.");

int64 timestamp_micros()
{
    return duration_cast<microseconds>(
               high_resolution_clock::now().time_since_epoch())
        .count();
}

std::string SerializeImage(const cv::Mat &m)
{
    std::vector<uchar> bytes;
    bool encode_ok = cv::imencode(".jpg", m, bytes);
    if (!encode_ok)
    {
        std::cout << "Encode image failed!" << std::endl;
    }
    std::string res(bytes.begin(), bytes.end());
    return res;
}

// FML. see
// https://github.com/JetsonHacksNano/CSI-Camera/blob/master/simple_camera.cpp
std::string gstreamer_pipeline(int camera_id, int capture_width,
                               int capture_height, int display_width,
                               int display_height, int framerate,
                               int flip_method = 0)
{
    return "nvarguscamerasrc sensor_id=" + std::to_string(camera_id) +
           " ! video/x-raw(memory:NVMM), width=(int)" +
           std::to_string(capture_width) + ", height=(int)" +
           std::to_string(capture_height) +
           ", format=(string)NV12, framerate=(fraction)" +
           std::to_string(framerate) +
           "/1 ! nvvidconv flip-method=" + std::to_string(flip_method) +
           " ! video/x-raw, width=(int)" + std::to_string(display_width) +
           ", height=(int)" + std::to_string(display_height) +
           ", format=(string)BGRx ! videoconvert ! video/x-raw, "
           "format=(string)BGR ! appsink";
}

int main(int argc, char **argv)
{
    google::InitGoogleLogging(argv[0]);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    std::string pipeline_0 = gstreamer_pipeline(/*camera_id=*/0, kWidth, kHeight,
                                                kWidth, kHeight, kFrameRate);
    std::string pipeline_1 = gstreamer_pipeline(/*camera_id=*/1, kWidth, kHeight,
                                                kWidth, kHeight, kFrameRate);
    LOG(INFO) << "Using pipeline for camera 0: \n"
              << pipeline_0;
    LOG(INFO) << "Using pipeline for camera 1: \n"
              << pipeline_1;

    // open the first webcam plugged in the computer
    cv::VideoCapture camera_0(pipeline_0, cv::CAP_GSTREAMER);
    cv::VideoCapture camera_1(pipeline_1, cv::CAP_GSTREAMER);
    if (!camera_0.isOpened() || !camera_1.isOpened())
    {
        LOG(ERROR) << "ERROR: Could not open camera.";
        return 1;
    }

    cv::Mat camera_0_frame, camera_1_frame;

    std::ostringstream dir_path_stream;
    int64 entry_time = timestamp_micros();
    dir_path_stream << kDataDir << std::to_string(entry_time);
    const auto dir_path = dir_path_stream.str();
    LOG(INFO) << "Creating top level directory for data: " << dir_path;
    boost::filesystem::create_directories(dir_path);

    std::mutex queue_mutex;

    // Combine data for 10 seconds into a single file.
    recorder::RecordData record_data;
    bool running = true;

    std::queue<recorder::FrameData> frame_data_queue;

    auto camera_0_thread = std::thread([&camera_0, &camera_0_frame, &queue_mutex, &frame_data_queue, &running]()
                                       {
                                           while (running)
                                           {
                                               camera_0 >> camera_0_frame;
                                               recorder::FrameData frame_data;
                                               frame_data.set_timestamp(timestamp_micros());
                                               frame_data.set_camera_id(0);
                                               frame_data.set_image(SerializeImage(camera_0_frame));
                                               queue_mutex.lock();
                                               frame_data_queue.push(frame_data);
                                               queue_mutex.unlock();
                                           }
                                       });

    auto camera_1_thread = std::thread([&camera_1, &camera_1_frame, &queue_mutex, &frame_data_queue, &running]()
                                       {
                                           while (running)
                                           {
                                               camera_1 >> camera_1_frame;
                                               recorder::FrameData frame_data;
                                               frame_data.set_timestamp(timestamp_micros());
                                               frame_data.set_camera_id(1);
                                               frame_data.set_image(SerializeImage(camera_1_frame));
                                               queue_mutex.lock();
                                               frame_data_queue.push(frame_data);
                                               queue_mutex.unlock();
                                           }
                                       });

    auto timing_thread = std::thread([&running, entry_time]()
                                     {
                                         // Record data for at most 30 seconds.
                                         while (timestamp_micros() - entry_time < FLAGS_record_duration_seconds * 1e6)
                                         {
                                             usleep(1000);
                                         }
                                         LOG(INFO) << "Shutting down...";
                                         running = false;
                                     });

    auto data_thread = std::thread([&frame_data_queue, &queue_mutex, &running, &dir_path]()
                                   {
                                       recorder::RecordData record_data;
                                       while (running)
                                       {
                                           while (record_data.frame_data_size() < kFrameRate * 2 * FLAGS_record_blob_seconds && running)
                                           {
                                               queue_mutex.lock();
                                               while (!frame_data_queue.empty())
                                               {
                                                   *(record_data.add_frame_data()) = frame_data_queue.front();
                                                   frame_data_queue.pop();
                                               }
                                               queue_mutex.unlock();
                                           }
                                           std::ostringstream outfile;
                                           outfile << dir_path << "/" << std::to_string(timestamp_micros()) << ".binarypb";
                                           LOG(INFO) << "Saving to " << outfile.str();
                                           std::ofstream out(outfile.str());
                                           if (!out.is_open())
                                           {
                                               LOG(ERROR) << "Open file error.";
                                           }
                                           out << record_data.SerializeAsString();
                                           out.close();
                                           record_data.clear_frame_data();
                                       }
                                   });

    camera_0_thread.join();
    camera_1_thread.join();
    timing_thread.join();
    data_thread.join();

    camera_0.release();
    camera_1.release();

    return 0;
}
