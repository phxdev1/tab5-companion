#include "hal/hal.h"
#include "bsp/m5stack_tab5.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>
#include <cstdlib>

static const char *TAG = "hal:audio";

static esp_codec_dev_handle_t spk_dev_ = nullptr;
static bool audio_ready_ = false;

void hal::audio_init()
{
    i2s_std_config_t i2s_cfg = {};
    i2s_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
    i2s_cfg.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);

    esp_err_t ret = bsp_audio_init(&i2s_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bsp_audio_init failed: %d", ret);
        return;
    }

    spk_dev_ = bsp_audio_codec_speaker_init();
    if (!spk_dev_) {
        ESP_LOGE(TAG, "Speaker init failed");
        return;
    }

    audio_ready_ = true;
    ESP_LOGI(TAG, "Audio initialized");
}

void hal::audio_set_volume(int pct)
{
    if (!audio_ready_) return;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    esp_codec_dev_set_out_vol(spk_dev_, pct);
    ESP_LOGI(TAG, "Volume: %d%%", pct);
}

void hal::audio_tone(int freq_hz, int duration_ms)
{
    if (!audio_ready_) {
        ESP_LOGW(TAG, "Audio not initialized");
        return;
    }

    if (freq_hz < 20) freq_hz = 20;
    if (freq_hz > 20000) freq_hz = 20000;
    if (duration_ms < 1) duration_ms = 1;
    if (duration_ms > 10000) duration_ms = 10000;

    const int sample_rate = 16000;
    const int num_samples = (sample_rate * duration_ms) / 1000;
    const int buf_size = num_samples * sizeof(int16_t);

    int16_t *buf = (int16_t *)malloc(buf_size);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate tone buffer");
        return;
    }

    // Generate sine wave
    for (int i = 0; i < num_samples; i++) {
        float t = (float)i / sample_rate;
        buf[i] = (int16_t)(16000.0f * sinf(2.0f * M_PI * freq_hz * t));
    }

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .sample_rate = (uint32_t)sample_rate,
    };

    esp_codec_dev_set_out_vol(spk_dev_, 70);
    esp_codec_dev_open(spk_dev_, &fs);
    esp_codec_dev_write(spk_dev_, buf, buf_size);
    esp_codec_dev_close(spk_dev_);

    free(buf);
    ESP_LOGI(TAG, "Tone: %d Hz, %d ms", freq_hz, duration_ms);
}
