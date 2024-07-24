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

    unsigned int imageSize = camera.getImageBufferSize();
    std::unique_ptr<uint8_t[]> imageData = std::make_unique<uint8_t[]>(imageSize);

    camera.retrieve(imageData.get(), raspicam::RASPICAM_FORMAT_RGB);

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

std::vector<uint8_t> Camera::captureImage() {
    if (!camera.isOpened()) {
        spdlog::critical("Camera is not opened");
        return {};
    }

    if (!camera.grab()) {
        spdlog::critical("Error capturing image");
        is_camera_connected = false;
        return {};
    }

    unsigned long bytes = camera.getImageBufferSize();
    spdlog::info("Image buffer size: {} bytes", bytes);

    std::vector<uint8_t> imageData(bytes);

    camera.retrieve(imageData.data(), raspicam::RASPICAM_FORMAT_RGB);

    if (std::all_of(imageData.begin(), imageData.end(), [](uint8_t byte) { return byte == 0; })) {
        spdlog::warn("Captured image data is filled with zeros");
    } else {
        spdlog::info("Image captured successfully");
    }

    return imageData;
}

int Camera::getWidth(){
    return 1280;
}

int Camera::getHeight(){
    return 960;
}

int Camera::getImageBufferSize(){
    return camera.getImageBufferSize();
}

std::vector<uint8_t> Camera::convertToBMP(const std::vector<uint8_t>& imageData, int width, int height) {
    const int fileSize = 54 + 3 * width * height; // 54 bytes for header, rest for pixel data
    std::vector<uint8_t> bmp(fileSize, 0);

    // BMP header (54 bytes)
    bmp[0] = 'B';
    bmp[1] = 'M';
    *(int*)&bmp[2] = fileSize; // File size
    *(int*)&bmp[10] = 54; // Offset to pixel data
    *(int*)&bmp[14] = 40; // Header size
    *(int*)&bmp[18] = width; // Image width
    *(int*)&bmp[22] = height; // Image height
    *(short*)&bmp[26] = 1; // Planes
    *(short*)&bmp[28] = 24; // Bits per pixel

    // Copy image data to BMP
    std::memcpy(&bmp[54], imageData.data(), imageData.size());

    return bmp;
}
