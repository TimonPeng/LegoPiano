#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorAAC.h"
#include "AudioOutputI2S.h"
#include "sampleaac.h"

#include <Arduino.h>
#include <driver/i2s.h>

AudioFileSourcePROGMEM *in;
AudioGeneratorAAC *aac;
AudioOutputI2S *out;

// I2S Connections
#define I2S_DOUT 13
#define I2S_BCLK 15
#define I2S_LRC 2

const i2s_port_t I2S_PORT = I2S_NUM_0;

void setup() {
  Serial.begin(115200);

  in = new AudioFileSourcePROGMEM(sampleaac, sizeof(sampleaac));
  aac = new AudioGeneratorAAC();
  out = new AudioOutputI2S();
  out->SetGain(0.125);
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  aac->begin(in, out);
}

void loop() {
  if (aac->isRunning()) {
    aac->loop();
  } else {
    aac->stop();
    Serial.printf("Sound Generator\n");
    delay(1000);
  }
}
