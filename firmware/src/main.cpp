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

const i2s_port_t I2S_PORT = I2S_NUM_0;

const int sampleRate = 11025;

// The soundfont renderer
tsf *TinySoundFont = tsf_load_memory(SoundFont, sizeof(SoundFont));

// The buffers to exchange data between soundfont renderer and I2S driver
const size_t bufferSize = 1000;
RingBuf<short, bufferSize> buffer;
short intermediateBuffer[bufferSize];

void setup() {
  Serial.begin(115200);

  esp_err_t result;

  // The I2S config as per the example
  const i2s_config_t i2s_config = {
      .mode =
          i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX), // transfer, not receive
      .sample_rate = sampleRate,                     // 16KHz
      .bits_per_sample =
          I2S_BITS_PER_SAMPLE_16BIT, // could only get it to work with 32bits
      .channel_format = I2S_CHANNEL_FMT_ALL_RIGHT, // based on the soundfont
      .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // interrupt level 1
      .dma_buf_count = 8,                       // number of buffers
      .dma_buf_len = 64, // 8 samples per buffer (minimum)
      .use_apll = false, // Use audio PLL

  };
  result = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (result != ESP_OK) {
    Serial.printf("Failed installing I2S driver: %d\n", result);
    while (true)
      ;
  }

  // The pin config as per the setup
  const i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_BCLK,           // Serial Clock (SCK)
      .ws_io_num = I2S_LRC,             // Word Select (WS)
      .data_out_num = I2S_DOUT,         // Serial Data (SD)
      .data_in_num = I2S_PIN_NO_CHANGE, // not used (only for microphone)
  };
  result = i2s_set_pin(I2S_PORT, &pin_config);
  if (result != ESP_OK) {
    Serial.printf("Failed setting I2S pin: %d\n", result);
    while (true)
      ;
  }

  i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);

  // i2s_zero_dma_buffer(I2S_PORT);

  tsf_set_output(TinySoundFont, TSF_MONO, sampleRate, 0);

  Serial.println("I2S driver installed.");
}

void loop() {
  Serial.println("Sound Generator.");

  unsigned long t = millis();

  size_t written = 100;
  short value;
  // fill the I2S buffer from our ringbuffer; the I2S API doesn't tell
  // us how much space it has in its internal buffers, so we must send
  // one short at a time, without blocking, until it fails (which
  // means the buffers are full)
  while (written > 0) {
    // we don't want to drop an element from the buffer if we then
    // can't write it! so we peek at the buffer, try to write, and pop
    // if successful
    value =
        buffer[0] + 32768; // tsf produces *signed* shorts, i2s wants *unsigned*
    i2s_write(I2S_PORT, &value, sizeof(value), &written, 0);
    if (written > 0)
      buffer.pop(value);
  }

  // Fill the ringbuffer from the renderer
  int toFill = buffer.maxSize() - buffer.size();
  if (toFill > 100) {
    // The buffer tells us how much space it has, but it can't give us
    // a contiguous segment of RAM to write into; we need an
    // intermediate buffer
    tsf_render_short(TinySoundFont, intermediateBuffer, toFill, 0);
    for (int i = 0; i < toFill; ++i) {
      buffer.push(intermediateBuffer[i]);
    }
  }

  // while (millis() == t) {
  //   delayMicroseconds(100);
  // }

  tsf_note_on(TinySoundFont, 0, 36, 0.1f);
}
