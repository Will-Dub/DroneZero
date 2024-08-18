#ifndef CAMERA_H
#define CAMERA_H

#include <raspicam/raspicam.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <vector>
#include <spdlog/spdlog.h>
#include <memory>
#include <jpeglib.h>

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
    std::vector<unsigned char> captureImage();
    std::vector<uint8_t> convertToBMP(const std::vector<uint8_t>& imageData, int width, int height);
    std::vector<uint8_t> convertToJpeg(const std::vector<uint8_t>& imageData, int width, int height, int quality);
    int getWidth();
    int getHeight();
    int getImageBufferSize();
    bool init();
    bool IsConnected();

private:
    raspicam::RaspiCam camera;
    bool isCameraConnected;
};

#endif