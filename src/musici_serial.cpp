#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#define NEOPIXEL_PIN   44        // WS2812B的数据线接在哪个引脚
#define NUM_LEDS       8        // 灯的个数，根据实际填
unsigned long lastUpdate = 0;
const unsigned long timeout = 1000;
Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
void setColor(uint8_t r, uint8_t g, uint8_t b) {
  strip.setPixelColor(0, strip.Color(r, g, b)); // 只设第一个灯
  strip.show(); // 显示更改
  lastUpdate = millis();
}
void setup() {
  Serial.begin(115200);
  strip.begin();            // 初始化灯带
  strip.show();             // 全部设置为灭
  setColor(0,0,0);          // 初始为灭
  while(1){
    if (Serial.available()) {
      String msg = Serial.readStringUntil('\n');
      if (msg == "HELLO-ESP32") {
        Serial.println("HELLO-PC");
        break;
      }
    }
  }
}
String inputString = "";
void loop() {
  while (Serial.available()) {
    char inChar = Serial.read();
    if (inChar == '\n') {
      int comma1 = inputString.indexOf(',');
      int comma2 = inputString.lastIndexOf(',');
      if (comma1 > 0 && comma2 > comma1) {
        int r = inputString.substring(0, comma1).toInt();
        int g = inputString.substring(comma1+1, comma2).toInt();
        int b = inputString.substring(comma2+1).toInt();
        setColor(r, g, b);
      }
      inputString = "";
    } else {
      if (inputString.length() < 16) inputString += inChar;
      else inputString = "";
    }
  }
  // 超时熄灯
  if (millis() - lastUpdate > timeout) {
    setColor(0, 0, 0);
    lastUpdate = millis();
  }
}