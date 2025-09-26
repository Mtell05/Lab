#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <Adafruit_GC9A01A.h>
#include <Adafruit_GFX.h>
#include <driver/i2s.h>
#include <cmath>

#include "secrets.h"

// ---------------- Pin Map ----------------
#define PIN_LCD_MOSI 11
#define PIN_LCD_SCLK 12
#define PIN_LCD_CS 10
#define PIN_LCD_DC 9
#define PIN_LCD_RST 13
#define PIN_LCD_BL 21

#define PIN_I2S_BCLK 16
#define PIN_I2S_LRCLK 17
#define PIN_I2S_DIN 15
#define PIN_AMP_SD 18

// ---------------- Display -----------------
static SPIClass lcdSpi(FSPI);
static Adafruit_GC9A01A display(&lcdSpi, PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_RST);

static uint16_t colorBackgroundOK;
static uint16_t colorBackgroundFail;
static uint16_t colorFace;
static uint16_t colorEye;
static uint16_t colorEyeClosed;
static uint16_t colorMouthOK;
static uint16_t colorMouthFail;

// ---------------- Audio -------------------
static constexpr i2s_port_t AUDIO_PORT = I2S_NUM_0;
static constexpr uint32_t AUDIO_SAMPLE_RATE = 16000;
static constexpr float STARTUP_TONE_FREQ = 440.0F;
static constexpr size_t AUDIO_CHUNK_SAMPLES = 256;

static bool wifiConnected = false;
// --------------- Helpers ------------------
static bool initDisplay();
static void renderAvatarBase();
static void drawEyes(bool open);
static void drawMouth(bool happy);
static void updateAvatarState(bool wifiOk, bool eyesOpen);
static bool connectWiFi(uint32_t timeoutMs = 10000);
static bool initI2S();
static void playStartupTone();

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n[Avatar] Booting...");

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, LOW);

  pinMode(PIN_AMP_SD, OUTPUT);
  digitalWrite(PIN_AMP_SD, LOW);  // keep amplifier muted until configured

  if (!initDisplay()) {
    Serial.println("[Avatar] Display init failed.");
  }

  wifiConnected = connectWiFi();

  renderAvatarBase();
  drawEyes(true);
  drawMouth(wifiConnected);

  if (initI2S()) {
    digitalWrite(PIN_AMP_SD, HIGH);  // unmute
    playStartupTone();
  } else {
    Serial.println("[Avatar] I2S init failed.");
  }
}

void loop() {
  static uint32_t lastWifiCheck = 0;
  static bool eyesOpen = true;
  static uint32_t lastBlinkChange = 0;
  const uint32_t now = millis();

  // Periodically refresh Wi-Fi state
  if (now - lastWifiCheck >= 2000) {
    lastWifiCheck = now;
    const bool currentWifi = WiFi.status() == WL_CONNECTED;
    if (currentWifi != wifiConnected) {
      wifiConnected = currentWifi;
      updateAvatarState(wifiConnected, eyesOpen);
      Serial.printf("[Avatar] Wi-Fi %s\n", wifiConnected ? "reconnected" : "lost");
    }
  }

  // Blink eyes every ~4 seconds
  if (eyesOpen && now - lastBlinkChange > 4000) {
    eyesOpen = false;
    lastBlinkChange = now;
    drawEyes(false);
  } else if (!eyesOpen && now - lastBlinkChange > 200) {
    eyesOpen = true;
    lastBlinkChange = now;
    drawEyes(true);
  }

  delay(10);
}

static bool initDisplay() {
  lcdSpi.begin(PIN_LCD_SCLK, -1, PIN_LCD_MOSI, PIN_LCD_CS);
  if (!display.begin()) {
    return false;
  }
  display.setRotation(0);
  display.fillScreen(0x0000);
  colorBackgroundOK = display.color565(12, 30, 60);
  colorBackgroundFail = display.color565(120, 0, 0);
  colorFace = display.color565(255, 214, 160);
  colorEye = display.color565(20, 20, 20);
  colorEyeClosed = display.color565(100, 60, 20);
  colorMouthOK = display.color565(200, 80, 120);
  colorMouthFail = display.color565(255, 220, 0);
  digitalWrite(PIN_LCD_BL, HIGH);
  return true;
}

static bool connectWiFi(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[Avatar] Connecting to %s", WIFI_SSID);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    Serial.print('.');
    delay(250);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[Avatar] Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println("[Avatar] Wi-Fi connection failed.");
  return false;
}

static void renderAvatarBase() {
  const uint16_t bg = wifiConnected ? colorBackgroundOK : colorBackgroundFail;
  display.fillScreen(bg);

  const int16_t cx = display.width() / 2;
  const int16_t cy = display.height() / 2;
  const int16_t faceRadius = min(cx, cy) - 8;

  display.fillCircle(cx, cy, faceRadius, colorFace);

  // cheeks
  display.fillCircle(cx - faceRadius / 3, cy + faceRadius / 5, faceRadius / 6, display.color565(255, 160, 170));
  display.fillCircle(cx + faceRadius / 3, cy + faceRadius / 5, faceRadius / 6, display.color565(255, 160, 170));
}

static void drawEyes(bool open) {
  const int16_t cx = display.width() / 2;
  const int16_t cy = display.height() / 2;
  const int16_t spacing = display.width() / 5;
  const int16_t eyeRadius = display.width() / 12;
  const int16_t eyeY = cy - display.height() / 10;

  if (open) {
    display.fillCircle(cx - spacing, eyeY, eyeRadius, 0xFFFF);
    display.fillCircle(cx + spacing, eyeY, eyeRadius, 0xFFFF);
    display.fillCircle(cx - spacing, eyeY, eyeRadius / 2, colorEye);
    display.fillCircle(cx + spacing, eyeY, eyeRadius / 2, colorEye);
  } else {
    const int16_t width = eyeRadius * 2;
    const int16_t height = eyeRadius / 3;
    display.fillRoundRect(cx - spacing - width / 2, eyeY - height / 2, width, height, height / 2, colorEyeClosed);
    display.fillRoundRect(cx + spacing - width / 2, eyeY - height / 2, width, height, height / 2, colorEyeClosed);
  }
}

static void drawMouth(bool happy) {
  const int16_t cx = display.width() / 2;
  const int16_t cy = display.height() / 2 + display.height() / 6;
  const int16_t mouthRadius = display.width() / 6;
  const uint16_t color = happy ? colorMouthOK : colorMouthFail;

  display.fillCircle(cx, cy, mouthRadius, color);
  if (happy) {
    display.fillCircle(cx, cy - mouthRadius / 2, mouthRadius, colorFace);
  } else {
    display.fillCircle(cx, cy + mouthRadius / 2, mouthRadius, colorFace);
  }
}

static void updateAvatarState(bool wifiOk, bool eyesOpen) {
  wifiConnected = wifiOk;
  renderAvatarBase();
  drawEyes(eyesOpen);
  drawMouth(wifiOk);
}

static bool initI2S() {
  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = static_cast<int>(AUDIO_SAMPLE_RATE),
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = static_cast<int>(AUDIO_CHUNK_SAMPLES),
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT};

  if (i2s_driver_install(AUDIO_PORT, &config, 0, nullptr) != ESP_OK) {
    return false;
  }

  const i2s_pin_config_t pins = {
      .bck_io_num = PIN_I2S_BCLK,
      .ws_io_num = PIN_I2S_LRCLK,
      .data_out_num = PIN_I2S_DIN,
      .data_in_num = I2S_PIN_NO_CHANGE};

  if (i2s_set_pin(AUDIO_PORT, &pins) != ESP_OK) {
    return false;
  }

  if (i2s_set_clk(AUDIO_PORT, AUDIO_SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO) != ESP_OK) {
    return false;
  }

  return true;
}

static void playStartupTone() {
  static int16_t sampleBuffer[AUDIO_CHUNK_SAMPLES * 2];
  float phase = 0.0F;
  const float phaseIncrement = 2.0F * PI * STARTUP_TONE_FREQ / static_cast<float>(AUDIO_SAMPLE_RATE);

  const uint32_t totalChunks = (AUDIO_SAMPLE_RATE / AUDIO_CHUNK_SAMPLES) / 2;  // ~0.5 s
  for (uint32_t chunk = 0; chunk < totalChunks; ++chunk) {
    for (size_t i = 0; i < AUDIO_CHUNK_SAMPLES; ++i) {
      const int16_t sample = static_cast<int16_t>(std::sin(phase) * 12000);
      sampleBuffer[2 * i] = sample;
      sampleBuffer[2 * i + 1] = sample;
      phase += phaseIncrement;
      if (phase >= 2.0F * PI) {
        phase -= 2.0F * PI;
      }
    }

    size_t bytesWritten = 0;
    const esp_err_t err = i2s_write(AUDIO_PORT, sampleBuffer, sizeof(sampleBuffer), &bytesWritten, portMAX_DELAY);
    if (err != ESP_OK || bytesWritten != sizeof(sampleBuffer)) {
      Serial.println("[Avatar] I2S write error.");
      break;
    }
  }
}
