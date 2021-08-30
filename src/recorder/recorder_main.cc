#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

#include "record_data.pb.h"
#include "opencv2/opencv.hpp"

using namespace std::chrono;

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

int main(int argc, char **argv)
{
    // open the first webcam plugged in the computer
    cv::VideoCapture camera(0);
    if (!camera.isOpened())
    {
        std::cerr << "ERROR: Could not open camera" << std::endl;
        return 1;
    }

    camera.set(cv::CAP_PROP_FPS, 30);

    cv::Mat frame;

    // display the frame until you press a key
    for (size_t i = 0; i < 10; ++i)
    {
        // capture the next frame from the webcam
        camera >> frame;
        const auto serialized_img = SerializeImage(frame);
        int64 timestamp = duration_cast<microseconds>(
                              high_resolution_clock::now().time_since_epoch())
                              .count();

        recorder::RecordData exps;
        exps.set_timestamp(timestamp);
        exps.set_image(serialized_img);

        std::ostringstream outfile;
        outfile << timestamp << ".binarypb";
        std::cout << "Saving to " << outfile.str() << std::endl;
        std::ofstream out(outfile.str());
        if (!out.is_open())
        {
            std::cout << "Open file error." << std::endl;
        }
        out << exps.SerializeAsString();
        out.close();
    }
    return 0;
}
