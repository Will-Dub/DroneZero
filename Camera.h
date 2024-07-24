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
#include <memory>

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};
#pragma pack(pop)

class Camera {
public:
    Camera();
    ~Camera();
    bool init();
    bool takePicture(const std::string &filename);
    std::vector<unsigned char> captureImage();
    bool IsConnected();
    void captureImage(std::vector<uint8_t>& imageData, size_t& imageSize);
    std::vector<uint8_t> convertToBMP(const std::vector<uint8_t>& imageData, int width, int height);
    int getWidth();
    int getHeight();
    int getImageBufferSize();

private:
    raspicam::RaspiCam camera;
    unsigned char *data;
    bool is_camera_connected;
};

#endif