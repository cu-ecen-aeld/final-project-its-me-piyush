#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstring>
#include <sys/mman.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <jpeglib.h>
#include <signal.h>
#include <libcamera/libcamera.h>

using namespace libcamera;

static std::mutex frame_mutex;
static std::condition_variable frame_cv;
static std::vector<uint8_t> jpeg_frame;
static bool new_frame = false;
static Camera* g_camera = nullptr;

std::vector<uint8_t> encode_jpeg_yuv420(const uint8_t* data, int width, int height) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    uint8_t* out_buf = nullptr;
    unsigned long out_size = 0;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &out_buf, &out_size);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_YCbCr;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 80, TRUE);

    // Set up YUV420 subsampling
    cinfo.raw_data_in = TRUE;
    cinfo.comp_info[0].h_samp_factor = 2;
    cinfo.comp_info[0].v_samp_factor = 2;
    cinfo.comp_info[1].h_samp_factor = 1;
    cinfo.comp_info[1].v_samp_factor = 1;
    cinfo.comp_info[2].h_samp_factor = 1;
    cinfo.comp_info[2].v_samp_factor = 1;

    jpeg_start_compress(&cinfo, TRUE);

    const uint8_t* y_plane = data;
    const uint8_t* u_plane = data + width * height;
    const uint8_t* v_plane = u_plane + (width * height) / 4;

    JSAMPROW y_rows[16], u_rows[8], v_rows[8];
    JSAMPARRAY planes[3] = { y_rows, u_rows, v_rows };

    while (cinfo.next_scanline < (JDIMENSION)height) {
        int y_start = cinfo.next_scanline;
        for (int i = 0; i < 16 && (y_start + i) < height; i++)
            y_rows[i] = (JSAMPROW)(y_plane + (y_start + i) * width);
        for (int i = 0; i < 8 && (y_start / 2 + i) < height / 2; i++) {
            u_rows[i] = (JSAMPROW)(u_plane + (y_start / 2 + i) * (width / 2));
            v_rows[i] = (JSAMPROW)(v_plane + (y_start / 2 + i) * (width / 2));
        }
        jpeg_write_raw_data(&cinfo, planes, 16);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    std::vector<uint8_t> result(out_buf, out_buf + out_size);
    free(out_buf);
    return result;
}

void request_completed(Request* request) {
    if (request->status() == Request::RequestCancelled) return;

    auto* buffer = request->buffers().begin()->second;
    size_t total_size = 0;
    for (auto& plane : buffer->planes())
        total_size += plane.length;

    void* data = mmap(nullptr, total_size, PROT_READ, MAP_SHARED,
                      buffer->planes()[0].fd.get(), 0);
    if (data != MAP_FAILED) {
        auto jpeg = encode_jpeg_yuv420((uint8_t*)data, 1280, 720);
        munmap(data, total_size);

        std::lock_guard<std::mutex> lock(frame_mutex);
        jpeg_frame = std::move(jpeg);
        new_frame = true;
        frame_cv.notify_all();
    }

    request->reuse(Request::ReuseBuffers);
    g_camera->queueRequest(request);
}

void handle_client(int client_fd) {
    const char* header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n";
    send(client_fd, header, strlen(header), 0);

    while (true) {
        std::vector<uint8_t> frame;
        {
            std::unique_lock<std::mutex> lock(frame_mutex);
            frame_cv.wait(lock, [] { return new_frame; });
            frame = jpeg_frame;
            new_frame = false;
        }

        char part_header[256];
        snprintf(part_header, sizeof(part_header),
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n",
            frame.size());

        if (send(client_fd, part_header, strlen(part_header), 0) < 0) break;
        if (send(client_fd, frame.data(), frame.size(), 0) < 0) break;
        if (send(client_fd, "\r\n", 2, 0) < 0) break;
    }
    close(client_fd);
}

void run_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);
    std::cout << "Streaming at http://0.0.0.0:" << port << "/\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0)
            std::thread(handle_client, client_fd).detach();
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    std::thread(run_server, 8080).detach();

    auto cm = std::make_unique<CameraManager>();
    cm->start();

    if (cm->cameras().empty()) {
        std::cerr << "No cameras found!\n";
        return 1;
    }

    auto camera = cm->cameras()[0];
    g_camera = camera.get();
    camera->acquire();

    auto config = camera->generateConfiguration({StreamRole::Viewfinder});
    auto& stream_config = config->at(0);
    stream_config.pixelFormat = formats::YUV420;
    stream_config.size = {1280, 720};
    config->validate();
    camera->configure(config.get());

    auto allocator = std::make_unique<FrameBufferAllocator>(camera);
    Stream* stream = stream_config.stream();
    allocator->allocate(stream);

    std::vector<std::unique_ptr<Request>> requests;
    for (auto& buffer : allocator->buffers(stream)) {
        auto request = camera->createRequest();
        request->addBuffer(stream, buffer.get());
        requests.push_back(std::move(request));
    }

    camera->requestCompleted.connect(request_completed);

    camera->start();
    for (auto& req : requests)
        camera->queueRequest(req.get());

    std::this_thread::sleep_for(std::chrono::hours(24 * 365));

    camera->stop();
    camera->release();
    cm->stop();
    return 0;
}
