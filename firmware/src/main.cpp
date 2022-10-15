#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO

#include "soundfont.h"

#include <Arduino.h>
#include <RingBuf.h>
#include <driver/dac.h>
#include <driver/i2s.h>
#include <tsf.h>

// I2S Connections
#define I2S_DOUT 27
#define I2S_BCLK 33
#define I2S_LRC 32

static const int SAMPLE_RATE = 11025;

static const i2s_port_t I2S_PORT = I2S_NUM_0;

// I2S configuration structures
static const i2s_config_t I2S_CONFIG = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX), // transfer, not receive
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample =
        I2S_BITS_PER_SAMPLE_16BIT, // could only get it to work with 32bits
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // based on the soundfont
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // interrupt level 1
    .dma_buf_count = 8,                       // number of buffers
    .dma_buf_len = 64,                        // 8 samples per buffer (minimum)
    .use_apll = false,                        // Use audio PLL
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0,
};

// I2S pin config
static const i2s_pin_config_t I2S_PIN_CONFIG = {
    .bck_io_num = I2S_BCLK,           // Serial Clock (SCK)
    .ws_io_num = I2S_LRC,             // Word Select (WS)
    .data_out_num = I2S_DOUT,         // Serial Data (SD)
    .data_in_num = I2S_PIN_NO_CHANGE, // not used (only for microphone)
};

// soundfont renderer
static tsf *TinySoundFont = tsf_load_memory(SoundFont, sizeof(SoundFont));

// The buffers to exchange data between soundfont renderer and I2S driver
const size_t bufferSize = 1000;
RingBuf<short, bufferSize> buffer;
short intermediateBuffer[bufferSize];

void setup() {
  Serial.begin(115200);

  if (!TinySoundFont) {
    Serial.println("ERROR: Unable to load SoundFont");
    while (true)
      ;
  }

  esp_err_t result;

  // The I2S config as per the example
  result = i2s_driver_install(I2S_PORT, &I2S_CONFIG, 0, NULL);
  if (result != ESP_OK) {
    Serial.printf("ERROR: Unable to install I2S driver: %d", result);
    while (true)
      ;
  }

  result = i2s_set_pin(I2S_PORT, &I2S_PIN_CONFIG);
  if (result != ESP_OK) {
    Serial.printf("ERROR: Unable to set I2S pins: %d\n", result);
    while (true)
      ;
  }

  // i2s_set_sample_rates();

  i2s_zero_dma_buffer(I2S_PORT);

  tsf_set_output(TinySoundFont, TSF_MONO, SAMPLE_RATE, 0);

  Serial.println("Setup completed");
}

void loop() {
  //  Serial.println("Sound Generator");
}
