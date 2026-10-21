#include <ScratchPadd/Audio/DeviceOutput.hpp>

#include <miniaudio/miniaudio.h>

namespace ScratchPadd::Audio {

struct DeviceOutput::Device {
  ma_device device; // set up by ma_device_init (see open)
  bool open = false;
  Renderer *renderer = nullptr;

  // miniaudio's audio thread: just hands the buffer to the mixer
  static void play(ma_device *device, void *out, const void *, ma_uint32 frames) {
    auto *self = static_cast<Device *>(device->pUserData);
    self->renderer->render(static_cast<float *>(out), frames);
  }
};

DeviceOutput::DeviceOutput(std::uint32_t sampleRate) : sampleRate_(sampleRate) {}

DeviceOutput::~DeviceOutput() { stop(); }

bool DeviceOutput::start(Renderer &renderer, std::string &error) {
  stop();
  device_ = std::make_unique<Device>();
  device_->renderer = &renderer;
  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 2;
  config.sampleRate = sampleRate_;
  config.dataCallback = &Device::play;
  config.pUserData = device_.get();
  // The default backends, then the null one, so a machine without sound
  // (a server, a container) still runs the engine
  ma_result result = ma_device_init(nullptr, &config, &device_->device);
  if (result != MA_SUCCESS) {
    const ma_backend null[] = {ma_backend_null};
    ma_context_config contextConfig = ma_context_config_init();
    static ma_context context; // lives as long as the program, once made
    static bool contextReady = ma_context_init(null, 1, &contextConfig, &context) == MA_SUCCESS;
    result = contextReady ? ma_device_init(&context, &config, &device_->device) : result;
  }
  if (result != MA_SUCCESS) {
    error = std::string("can't open a sound device: ") + ma_result_description(result);
    device_.reset();
    return false;
  }
  device_->open = true;
  result = ma_device_start(&device_->device);
  if (result != MA_SUCCESS) {
    error = std::string("can't start the sound device: ") + ma_result_description(result);
    stop();
    return false;
  }
  return true;
}

void DeviceOutput::stop() {
  if (device_ && device_->open) {
    ma_device_uninit(&device_->device); // stops it and waits for the audio thread
  }
  device_.reset();
}

std::string DeviceOutput::backendName() const {
  if (!device_ || !device_->open) {
    return "none";
  }
  return ma_get_backend_name(device_->device.pContext->backend);
}

} // namespace ScratchPadd::Audio
