// Shows a live camera feed in an SDL2 window.
//
//   bazel run //projects/examples/camera:exampleCamera [-- <camera index>]
//
// Keys: M toggles mirroring, Q or Escape quits. The window can be resized;
// the image keeps its aspect ratio.
//
// OpenCV picks the platform's camera API (AVFoundation on macOS, Media
// Foundation on Windows, V4L2 on Linux) and SDL2 provides the window, so the
// same code runs on all three.
#include <SDL2/SDL.h>
#include <fmt/core.h>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {

void printCameraAccessHint() {
#if defined(__APPLE__)
  fmt::println(stderr, "On macOS, allow camera access for your terminal app in System Settings > "
                       "Privacy & Security > Camera, then run again.");
#elif defined(_WIN32)
  fmt::println(stderr, "On Windows, check that Settings > Privacy & security > Camera allows "
                       "desktop apps to use the camera.");
#else
  fmt::println(stderr, "On Linux, check that a /dev/video* device exists and that your user can "
                       "read it (usually by being in the 'video' group).");
#endif
}

// Some cameras return a few empty frames while they start up.
bool readFirstFrame(cv::VideoCapture &camera, cv::Mat &frame) {
  for (int attempt = 0; attempt < 100; ++attempt) {
    if (camera.read(frame) && !frame.empty())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return false;
}

SDL_Texture *createFrameTexture(SDL_Renderer *renderer, const cv::Mat &frame) {
  // OpenCV frames are 8-bit BGR, which SDL can upload directly.
  return SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR24, SDL_TEXTUREACCESS_STREAMING, frame.cols,
                           frame.rows);
}

} // namespace

int main(int argc, char **argv) {
  // The camera to open: the first argument, or 0. Anything that isn't a
  // whole number is an error (atoi would have turned it into 0).
  int cameraIndex = 0;
  if (argc > 1) {
    const char *arg = argv[1];
    const char *end = arg + std::strlen(arg);
    auto [last, error] = std::from_chars(arg, end, cameraIndex);
    if (error != std::errc() || last != end || cameraIndex < 0) {
      fmt::println(stderr, "Usage: {} [camera number], e.g. 0", argv[0]);
      return 1;
    }
  }

  cv::VideoCapture camera(cameraIndex);
  if (!camera.isOpened()) {
    fmt::println(stderr, "Could not open camera {}.", cameraIndex);
    printCameraAccessHint();
    return 1;
  }

  cv::Mat frame;
  if (!readFirstFrame(camera, frame)) {
    fmt::println(stderr, "Camera {} opened but sent no frames.", cameraIndex);
    printCameraAccessHint();
    return 1;
  }
  if (frame.type() != CV_8UC3) {
    fmt::println(stderr, "Unsupported frame format from camera {} (expected 8-bit BGR).",
                 cameraIndex);
    return 1;
  }
  fmt::println("Camera {}: {}x{}. M toggles mirroring, Q or Escape quits.", cameraIndex, frame.cols,
               frame.rows);

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fmt::println(stderr, "Failed to initialize SDL: {}", SDL_GetError());
    return 1;
  }
  SDL_Window *window =
      SDL_CreateWindow("Camera", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, frame.cols,
                       frame.rows, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) {
    fmt::println(stderr, "Failed to create window: {}", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    fmt::println(stderr, "Failed to create renderer: {}", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  // Letterbox instead of stretching when the window is resized.
  SDL_RenderSetLogicalSize(renderer, frame.cols, frame.rows);
  SDL_Texture *texture = createFrameTexture(renderer, frame);

  bool mirror = true; // like a mirror, which is what people expect from a webcam
  bool done = false;
  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        done = true;
      if (event.type == SDL_KEYDOWN) {
        const SDL_Keycode key = event.key.keysym.sym;
        if (key == SDLK_ESCAPE || key == SDLK_q)
          done = true;
        if (key == SDLK_m)
          mirror = !mirror;
      }
    }

    // Blocks until the camera has a new frame, which paces the loop.
    if (!camera.read(frame) || frame.empty()) {
      fmt::println(stderr, "Camera {} stopped sending frames.", cameraIndex);
      break;
    }

    int textureWidth = 0, textureHeight = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);
    if (frame.cols != textureWidth || frame.rows != textureHeight) {
      SDL_DestroyTexture(texture);
      texture = createFrameTexture(renderer, frame);
      SDL_RenderSetLogicalSize(renderer, frame.cols, frame.rows);
    }

    if (mirror)
      cv::flip(frame, frame, 1); // around the vertical axis
    SDL_UpdateTexture(texture, nullptr, frame.data, static_cast<int>(frame.step));

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
