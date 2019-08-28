#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <FS.h>

#define DEBUG false

extern "C" {
  #include "pwm.h"
}

ESP8266WebServer webServer(80);

uint8_t power = 1;
uint8_t brightness = 0;
uint8_t colorValues[3] = { 0, 0, 0 };

#define PWM_CHANNELS 3
const uint32_t pwmPeriod = 5000; // * 200ns ^= 1 kHz
uint32 pwmPins[PWM_CHANNELS][3] = { // MUX, FUNC, PIN
	{PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14, 14}, // D5 = RED
	{PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12, 12}, // D6 = GREEN
	{PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13, 13}  // D7 = BLUE
};
uint32_t pwmDuty[PWM_CHANNELS] = { 0, 0, 0 };

void setPWM() {
  for (uint8_t i = 0; i < PWM_CHANNELS; i++) {
    if (power) {
      uint32_t newDuty = (((pwmPeriod * colorValues[i]) / 255) * brightness) / 255;
      pwmDuty[i] = newDuty > pwmPeriod ? pwmPeriod : newDuty < 0 ? 0 : newDuty;
      pwm_set_duty(pwmDuty[i], i);
    } else {
      pwm_set_duty(0, i);
    }
  }
  pwm_start();
}

void loadSettings() {
  brightness = EEPROM.read(0);

  colorValues[0] = EEPROM.read(1);
  colorValues[1] = EEPROM.read(2);
  colorValues[2] = EEPROM.read(3);

  power = EEPROM.read(4);

  setPWM();
}

void sendString(String value) { webServer.send(200, "text/plain", value); }

void sendInt(uint8_t value) { sendString(String(value)); }

void setPower(uint8_t value) {
  power = value == 0 ? 0 : 1;

  setPWM();

  EEPROM.write(4, power);
  EEPROM.commit();
}

void setBrightness(uint8_t value) {
  brightness = value > 255 ? 255 : value < 0 ? 0 : value;

  setPWM();

  EEPROM.write(0, brightness);
  EEPROM.commit();
}

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  colorValues[0] = r > 255 ? 255 : r < 0 ? 0 : r;
  colorValues[1] = g > 255 ? 255 : g < 0 ? 0 : g;
  colorValues[2] = b > 255 ? 255 : b < 0 ? 0 : b;

  setPWM();

  EEPROM.write(1, colorValues[0]);
  EEPROM.write(2, colorValues[1]);
  EEPROM.write(3, colorValues[2]);
  EEPROM.commit();
}

void setup() {
  if (DEBUG) Serial.begin(115200);
  delay(100);
  Serial.setDebugOutput(DEBUG);

  WiFi.hostname("LEDcontroller");
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(DEBUG);
  if (!wifiManager.autoConnect("LEDcontroller")) {
    delay(1000);
    ESP.restart();
    delay(1000);
  }

  pinMode(14, OUTPUT);
	pinMode(12, OUTPUT);
	pinMode(13, OUTPUT);
  digitalWrite(14, LOW);
	digitalWrite(12, LOW);
	digitalWrite(13, LOW);
  pwm_init(pwmPeriod, pwmDuty, PWM_CHANNELS, pwmPins);
  pwm_start();

  delay(2000);
  pwm_set_duty(2500, 0);
  pwm_start();
  delay(2000);
  pwm_set_duty(0, 0);
  pwm_start();
  delay(2000);
  pwm_set_duty(5000, 0);
  pwm_start();
  delay(2000);

  EEPROM.begin(512);

  loadSettings();

  SPIFFS.begin();

  webServer.on("/get", []() {
    String getVar = webServer.arg("var");
    if (getVar == "power") {
      sendInt(power);
    } else if (getVar == "brightness") {
      sendInt(brightness);
    } else if (getVar == "color") {
      sendString(String(colorValues[0]) + "," + String(colorValues[1]) + "," + String(colorValues[2]));
    } else {
      sendString("");
    }
  });

  webServer.on("/set", []() {
    sendString("");
    String setVar = webServer.arg("var");
    if (setVar == "power") {
      setPower(webServer.arg("val").toInt());
    } else if (setVar == "brightness") {
      setBrightness(webServer.arg("val").toInt());
    } else if (setVar == "color") {
      setColor(webServer.arg("r").toInt(), webServer.arg("g").toInt(), webServer.arg("b").toInt());
    }
  });

  webServer.serveStatic("/cache.manifest", SPIFFS, "/cache.manifest");
  webServer.serveStatic("/", SPIFFS, "/index.htm", "max-age=86400");

  webServer.begin();
  if (DEBUG) Serial.println("HTTP web server started");
}

void loop() {
  webServer.handleClient();
  yield();
}
