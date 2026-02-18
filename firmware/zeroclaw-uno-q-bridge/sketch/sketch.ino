// ZeroClaw Bridge — full MCU peripheral control for Arduino UNO Q
// SPDX-License-Identifier: MPL-2.0
//
// Exposes GPIO, ADC, PWM, I2C, SPI, CAN (stub), LED matrix, and RGB LED
// control to the host agent via the Router Bridge protocol.

#include "Arduino_RouterBridge.h"
#include <Wire.h>
#include <SPI.h>

// ── Pin / hardware constants (UNO Q datasheet ABX00162) ─────────

// ADC: 12-bit, channels A0-A5 map to pins 14-19, VREF+ = 3.3V
static const int ADC_FIRST_PIN = 14;
static const int ADC_LAST_PIN  = 19;

// PWM-capable digital pins
static const int PWM_PINS[]    = {3, 5, 6, 9, 10, 11};
static const int PWM_PIN_COUNT = sizeof(PWM_PINS) / sizeof(PWM_PINS[0]);

// 8x13 LED matrix — 104 blue pixels
static const int LED_MATRIX_BYTES = 13;

// MCU RGB LEDs 3-4 — active-low, pins PH10-PH15
#ifndef PIN_RGB_LED3_R
  #define PIN_RGB_LED3_R 22
  #define PIN_RGB_LED3_G 23
  #define PIN_RGB_LED3_B 24
  #define PIN_RGB_LED4_R 25
  #define PIN_RGB_LED4_G 26
  #define PIN_RGB_LED4_B 27
#endif

static const int RGB_LED_PINS[][3] = {
  {PIN_RGB_LED3_R, PIN_RGB_LED3_G, PIN_RGB_LED3_B},
  {PIN_RGB_LED4_R, PIN_RGB_LED4_G, PIN_RGB_LED4_B},
};
static const int RGB_LED_COUNT = sizeof(RGB_LED_PINS) / sizeof(RGB_LED_PINS[0]);

// ── Hex helpers ─────────────────────────────────────────────────

static uint8_t hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return 0;
}

static int hex_decode(const char *hex, uint8_t *buf, int max_len) {
  int len = 0;
  while (hex[0] && hex[1] && len < max_len) {
    buf[len++] = (hex_nibble(hex[0]) << 4) | hex_nibble(hex[1]);
    hex += 2;
  }
  return len;
}

static void hex_encode(const uint8_t *data, int len, char *out) {
  static const char hexchars[] = "0123456789abcdef";
  for (int i = 0; i < len; i++) {
    out[i * 2]     = hexchars[(data[i] >> 4) & 0x0F];
    out[i * 2 + 1] = hexchars[data[i] & 0x0F];
  }
  out[len * 2] = '\0';
}

static bool is_pwm_pin(int pin) {
  for (int i = 0; i < PWM_PIN_COUNT; i++) {
    if (PWM_PINS[i] == pin) return true;
  }
  return false;
}

// ── GPIO (original, unchanged) ──────────────────────────────────

void gpio_write(int pin, int value) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, value ? HIGH : LOW);
}

int gpio_read(int pin) {
  pinMode(pin, INPUT);
  return digitalRead(pin);
}

// ── ADC (12-bit, A0-A5) ────────────────────────────────────────

int adc_read(int channel) {
  int pin = ADC_FIRST_PIN + channel;
  if (pin < ADC_FIRST_PIN || pin > ADC_LAST_PIN) return -1;
  analogReadResolution(12);
  return analogRead(pin);
}

// ── PWM (D3, D5, D6, D9, D10, D11) ─────────────────────────────

int pwm_write(int pin, int duty) {
  if (!is_pwm_pin(pin)) return -1;
  if (duty < 0)   duty = 0;
  if (duty > 255) duty = 255;
  pinMode(pin, OUTPUT);
  analogWrite(pin, duty);
  return 0;
}

// ── I2C scan ────────────────────────────────────────────────────

String i2c_scan() {
  Wire.begin();
  String result = "";
  bool first = true;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      if (!first) result += ",";
      result += String(addr);
      first = false;
    }
  }
  return result.length() > 0 ? result : "none";
}

// ── I2C transfer ────────────────────────────────────────────────

String i2c_transfer(int addr, const char *hex_data, int rx_len) {
  if (addr < 1 || addr > 127) return "err:addr";
  if (rx_len < 0 || rx_len > 32) return "err:rxlen";

  uint8_t tx_buf[32];
  int tx_len = hex_decode(hex_data, tx_buf, sizeof(tx_buf));

  Wire.begin();
  if (tx_len > 0) {
    Wire.beginTransmission((uint8_t)addr);
    Wire.write(tx_buf, tx_len);
    uint8_t err = Wire.endTransmission(rx_len == 0);
    if (err != 0) return "err:tx:" + String(err);
  }

  if (rx_len > 0) {
    Wire.requestFrom((uint8_t)addr, (uint8_t)rx_len);
    uint8_t rx_buf[32];
    int count = 0;
    while (Wire.available() && count < rx_len) {
      rx_buf[count++] = Wire.read();
    }
    char hex_out[65];
    hex_encode(rx_buf, count, hex_out);
    return String(hex_out);
  }
  return "ok";
}

// ── SPI transfer ────────────────────────────────────────────────

String spi_transfer(const char *hex_data) {
  uint8_t buf[32];
  int len = hex_decode(hex_data, buf, sizeof(buf));
  if (len == 0) return "err:empty";

  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  uint8_t rx_buf[32];
  for (int i = 0; i < len; i++) {
    rx_buf[i] = SPI.transfer(buf[i]);
  }
  SPI.endTransaction();

  char hex_out[65];
  hex_encode(rx_buf, len, hex_out);
  return String(hex_out);
}

// ── CAN (stub — needs Zephyr FDCAN driver) ──────────────────────

int can_send(int id, const char *hex_data) {
  (void)id;
  (void)hex_data;
  return -2;  // not yet available
}

// ── LED matrix (8x13, 13-byte bitmap) ───────────────────────────

int led_matrix(const char *hex_bitmap) {
  uint8_t bitmap[LED_MATRIX_BYTES];
  int len = hex_decode(hex_bitmap, bitmap, LED_MATRIX_BYTES);
  if (len != LED_MATRIX_BYTES) return -1;
  // Matrix rendering depends on board LED matrix driver availability.
  // Bitmap accepted; actual display requires Arduino_LED_Matrix library.
  (void)bitmap;
  return 0;
}

// ── RGB LED (MCU LEDs 3-4, active-low) ──────────────────────────

int rgb_led(int id, int r, int g, int b) {
  if (id < 0 || id >= RGB_LED_COUNT) return -1;
  r = constrain(r, 0, 255);
  g = constrain(g, 0, 255);
  b = constrain(b, 0, 255);
  pinMode(RGB_LED_PINS[id][0], OUTPUT);
  pinMode(RGB_LED_PINS[id][1], OUTPUT);
  pinMode(RGB_LED_PINS[id][2], OUTPUT);
  analogWrite(RGB_LED_PINS[id][0], 255 - r);
  analogWrite(RGB_LED_PINS[id][1], 255 - g);
  analogWrite(RGB_LED_PINS[id][2], 255 - b);
  return 0;
}

// ── Capabilities ────────────────────────────────────────────────

String get_capabilities() {
  return "gpio,adc,pwm,i2c,spi,can,led_matrix,rgb_led";
}

// ── Bridge setup ────────────────────────────────────────────────

void setup() {
  Bridge.begin();
  Bridge.provide("digitalWrite", gpio_write);
  Bridge.provide("digitalRead",  gpio_read);
  Bridge.provide("analogRead",   adc_read);
  Bridge.provide("analogWrite",  pwm_write);
  Bridge.provide("i2cScan",      i2c_scan);
  Bridge.provide("i2cTransfer",  i2c_transfer);
  Bridge.provide("spiTransfer",  spi_transfer);
  Bridge.provide("canSend",      can_send);
  Bridge.provide("ledMatrix",    led_matrix);
  Bridge.provide("rgbLed",       rgb_led);
  Bridge.provide("capabilities", get_capabilities);
}

void loop() {
  Bridge.update();
}
