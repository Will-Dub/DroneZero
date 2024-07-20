#ifndef CAMERA_H
#define CAMERA_H

#include <raspicam/raspicam.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

class Camera {
public:
    Camera();
    ~Camera();
    bool init();
    bool takePicture(const std::string &filename);
    std::vector<unsigned char> captureImage();
    bool IsConnected();

private:
    raspicam::RaspiCam camera;
    unsigned char *data;
    bool is_camera_connected;
};

#endif