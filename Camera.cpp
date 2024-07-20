#include "Camera.h"

Camera::Camera() : data(nullptr), is_camera_connected(false) {}

Camera::~Camera() {
    if (data) {
        delete[] data;
    }
    camera.release();
}

bool Camera::init() {
    camera.setFormat(raspicam::RASPICAM_FORMAT_RGB);
    camera.setCaptureSize(1280, 960);

    if (!camera.open()) {
        spdlog::critical("Error opening camera");
        is_camera_connected = false;
        return false;
    }

    is_camera_connected = true;
    return true;
}

bool Camera::IsConnected(){
    return is_camera_connected;
}

bool Camera::takePicture(const std::string &filename) {
    if (!camera.grab()) {
        spdlog::critical("Error capturing image");
        is_camera_connected = false;
        return false;
    }

    if (!data) {
        data = new unsigned char[camera.getImageBufferSize()];
    }

    camera.retrieve(data, raspicam::RASPICAM_FORMAT_RGB);

    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        spdlog::warn("Error opening output file");
        return false;
    }

    outFile << "P6\n" << camera.getWidth() << " " << camera.getHeight() << " 255\n";
    outFile.write(reinterpret_cast<char*>(data), camera.getImageBufferSize());
    outFile.close();

    return true;
}

std::vector<unsigned char> Camera::captureImage() {
    if (!camera.grab()) {
        spdlog::critical("Error capturing image");
        is_camera_connected = false;
        return std::vector<unsigned char>();
    }

    if (!data) {
        data = new unsigned char[camera.getImageBufferSize()];
    }

    camera.retrieve(data, raspicam::RASPICAM_FORMAT_RGB);

    return std::vector<unsigned char>(data, data + camera.getImageBufferSize());
}