#include "Camera.h"

Camera::Camera() : is_camera_connected(false) {}

Camera::~Camera() {
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

std::vector<uint8_t> Camera::captureImage() {
    if (!camera.isOpened()) {
        spdlog::critical("Camera is not opened");
        is_camera_connected = false;
        return {};
    }

    if (!camera.grab()) {
        spdlog::critical("Error capturing image");
        is_camera_connected = false;
        return {};
    }

    unsigned long bytes = camera.getImageBufferSize();

    std::vector<uint8_t> imageData(bytes);

    camera.retrieve(imageData.data());

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

std::vector<uint8_t> Camera::convertToJpeg(const std::vector<uint8_t>& imageData, int width, int height, int quality) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    // Initialize the JPEG compression object with default error handling.
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // Use a memory buffer to store the JPEG data.
    unsigned char* jpegBuffer = nullptr;
    unsigned long jpegSize = 0;
    jpeg_mem_dest(&cinfo, &jpegBuffer, &jpegSize);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3; // RGB
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE); // Set quality (0-100)

    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = const_cast<uint8_t*>(&imageData[cinfo.next_scanline * width * 3]);
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

    // Copy the JPEG data to a std::vector<uint8_t>
    std::vector<uint8_t> jpegData(jpegBuffer, jpegBuffer + jpegSize);

    // Clean up
    jpeg_destroy_compress(&cinfo);
    free(jpegBuffer);

    return jpegData;
}
