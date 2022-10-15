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

static const int SAMPLE_RATE = 44100;

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
const static unsigned char MinimalSoundFont[] = {
#define TEN0 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    'R', 'I',  'F',  'F',  220,  1,    0,    0,    's',  'f',  'b',  'k',  'L',
    'I', 'S',  'T',  88,   1,    0,    0,    'p',  'd',  't',  'a',  'p',  'h',
    'd', 'r',  76,   TEN0, TEN0, TEN0, TEN0, 0,    0,    0,    0,    TEN0, 0,
    0,   0,    0,    0,    0,    0,    255,  0,    255,  0,    1,    TEN0, 0,
    0,   0,    'p',  'b',  'a',  'g',  8,    0,    0,    0,    0,    0,    0,
    0,   1,    0,    0,    0,    'p',  'm',  'o',  'd',  10,   TEN0, 0,    0,
    0,   'p',  'g',  'e',  'n',  8,    0,    0,    0,    41,   0,    0,    0,
    0,   0,    0,    0,    'i',  'n',  's',  't',  44,   TEN0, TEN0, 0,    0,
    0,   0,    0,    0,    0,    0,    TEN0, 0,    0,    0,    0,    0,    0,
    0,   1,    0,    'i',  'b',  'a',  'g',  8,    0,    0,    0,    0,    0,
    0,   0,    2,    0,    0,    0,    'i',  'm',  'o',  'd',  10,   TEN0, 0,
    0,   0,    'i',  'g',  'e',  'n',  12,   0,    0,    0,    54,   0,    1,
    0,   53,   0,    0,    0,    0,    0,    0,    0,    's',  'h',  'd',  'r',
    92,  TEN0, TEN0, 0,    0,    0,    0,    0,    0,    0,    50,   0,    0,
    0,   0,    0,    0,    0,    49,   0,    0,    0,    34,   86,   0,    0,
    60,  0,    0,    0,    1,    TEN0, TEN0, TEN0, TEN0, 0,    0,    0,    0,
    0,   0,    0,    'L',  'I',  'S',  'T',  112,  0,    0,    0,    's',  'd',
    't', 'a',  's',  'm',  'p',  'l',  100,  0,    0,    0,    86,   0,    119,
    3,   31,   7,    147,  10,   43,   14,   169,  17,   58,   21,   189,  24,
    73,  28,   204,  31,   73,   35,   249,  38,   46,   42,   71,   46,   250,
    48,  150,  53,   242,  55,   126,  60,   151,  63,   108,  66,   126,  72,
    207, 70,   86,   83,   100,  72,   74,   100,  163,  39,   241,  163,  59,
    175, 59,   179,  9,    179,  134,  187,  6,    186,  2,    194,  5,    194,
    15,  200,  6,    202,  96,   206,  159,  209,  35,   213,  213,  216,  45,
    220, 221,  223,  76,   227,  221,  230,  91,   234,  242,  237,  105,  241,
    8,   245,  118,  248,  32,   252};

static tsf *TinySoundFont =
    tsf_load_memory(MinimalSoundFont, sizeof(MinimalSoundFont));

// The buffers to exchange data between soundfont renderer and I2S driver
const size_t bufferSize = 1000;
RingBuf<short, bufferSize> buffer;
short intermediateBuffer[bufferSize];

esp_err_t result;

void setup() {
  Serial.begin(115200);

  if (!TinySoundFont) {
    Serial.println("ERROR: Unable to load SoundFont");
    while (true)
      ;
  }

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
  // 1. tsf_load
  // 2. tsf_set_output
  // 3. tsf_note_on
  // 4. tsf_render_short
  Serial.println("start loop");

  tsf_note_on(TinySoundFont, 0, 48, 1.0f); // C2

  Serial.println("tsf_note_on");

  short HalfSecond[22050];
  tsf_render_short(TinySoundFont, HalfSecond, 22050, 0);

  Serial.println("tsf_render_short");

  size_t i2s_bytes_written;
  result = i2s_write(I2S_PORT, &HalfSecond, sizeof(HalfSecond),
                     &i2s_bytes_written, 0);
  if (result != ESP_OK) {
    Serial.printf("ERROR: Unable to write I2S data: %d", result);
    while (true)
      ;
  }

  // delay(1000);

  // tsf_note_on(TinySoundFont, 0, 52, 1.0f); // E2
}
