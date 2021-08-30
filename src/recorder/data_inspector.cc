#include <fstream>

#include "record_data.pb.h"
#include "opencv2/opencv.hpp"
#include "gflags/gflags.h"

DEFINE_string(data_path, "",
              "path to RecordData proto.");

cv::Mat DeserializeImage(const std::string &str)
{
    std::vector<uchar> bytes(str.begin(), str.end());
    return cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
}

int main(int argc, char **argv)
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    recorder::RecordData exps;
    std::fstream input(FLAGS_data_path, std::ios::in | std::ios::binary);
    if (!exps.ParseFromIstream(&input))
    {
        std::cerr << "Failed to parse " << FLAGS_data_path << " to proto." << std::endl;
        return -1;
    }

    std::cout << "timestamp: " << exps.timestamp();

    cv::namedWindow("Webcam", cv::WINDOW_AUTOSIZE);

    // this will contain the image from the webcam
    cv::Mat frame = DeserializeImage(exps.image());

    cv::imshow("Webcam", frame);
    cv::waitKey(0);

    return 0;
}