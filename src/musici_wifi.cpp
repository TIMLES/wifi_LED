#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include "WiFiConnect.h"



#define NEOPIXEL_PIN   44
#define NUM_LEDS       8

const char* ssid     = "RAY";
const char* password = "12345678";
IPAddress local_IP(192, 168, 137, 50);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiUDP udp;
unsigned int udpPort = 8888;

unsigned long lastUpdate = 0;
const unsigned long timeout = 1000;


bool udpEnabled = false;
void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFi] STA已连接（未获得IP）");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[WiFi] 获得IP: ");
      Serial.println(WiFi.localIP());
      // 此处初始化UDP
      udp.begin(udpPort);
      Serial.println("UDP已初始化（WiFi获得IP）");
      udpEnabled = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[WiFi] STA断开，UDP不可用");
      udpEnabled = false;
      // 可选：这里可以重新释放UDP资源、标志位等
      break;
    default:
      break;
  }
}

Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
void setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t val) {
  int num=int(val*NUM_LEDS/255);
  for (int i = 0; i < NUM_LEDS; i++) {
    if (i<=num){
      strip.setPixelColor(i, strip.Color(r, g, b));
      } else  {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  strip.show();
  lastUpdate = millis();
}



// 创建全局对象
WiFiConnect wifiConn(
    "WifiConnectMyDevice_ESP_AP", // AP热点名
    "wJZagHy0xz9EISm"           // AP热点密码（实际请改强密码）
);

void Tips_wifi() {
  Serial.println("用户提示：WiFi已断开！");

  // 断网时红色闪3次
  for (int i = 0; i < 3; i++) {
    setColor(255, 0, 0, 255);
    delay(200);
    setColor(0, 0, 0, 0);
    delay(200);
  }

}



void setup() {
  Serial.begin(115200);
  // 注册WiFi事件回调
  WiFi.onEvent(onWiFiEvent);

  strip.begin();
  strip.show();
  setColor(0,0,0,0);

  WiFi.config(local_IP, gateway, subnet);
  wifiConn.connect();
  wifiConn.startAutoReconnect(Tips_wifi, 5000); 

}

unsigned long lastFpsTime = 0;
unsigned int frameCount = 0;

void loop() {

    if (udpEnabled) {
  int packetSize = udp.parsePacket();
  if (packetSize == 4) {
    uint8_t colors[4];
    int len = udp.read(colors, 4);
    if (len == 4) {
      setColor(colors[0], colors[1], colors[2], colors[3]);
      // Serial.printf("RGB_UDP: %d,%d,%d\n", colors[0], colors[1], colors[2]);
      // 在收到包后
      frameCount++;

      if (millis() - lastFpsTime >= 3000) {
        Serial.printf("UDP接收帧率: %.2f FPS\n", frameCount / 3.0);
        frameCount = 0;
        lastFpsTime = millis();
        }
    }
  }
  if (millis() - lastUpdate > timeout) {
    setColor(0, 0, 0,0);
    lastUpdate = millis();
  }}else {
    // Serial.println("UDP不可用，等待WiFi连接...");
    setColor(0,0,0,0);
  }
}



#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include "WiFiConnect.h"

// 创建全局对象（推荐放在setup之前）
WiFiConnect wifiConn(
    "WifiConnectMyDevice_ESP_AP", // AP热点名
    "wJZagHy0xz9EISm"           // AP热点密码（实际请改强密码）
);



#define NEOPIXEL_PIN   44
#define NUM_LEDS       8


IPAddress local_IP(192, 168, 137, 50);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);


WiFiUDP udp;
unsigned int udpPort = 8888;

unsigned long lastUpdate = 0;
const unsigned long timeout = 1000;


bool udpEnabled = false;
void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFi] STA已连接（未获得IP）");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[WiFi] 获得IP: ");
      Serial.println(WiFi.localIP());
      // 此处初始化UDP
      udp.begin(udpPort);
      Serial.println("UDP已初始化（WiFi获得IP）");
      udpEnabled = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[WiFi] STA断开，UDP不可用");
      udpEnabled = false;
      // 可选：这里可以重新释放UDP资源、标志位等
      break;
    default:
      break;
  }
}

Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
void setColor(uint8_t r, uint8_t g, uint8_t b, uint8_t val) {
  int num=int(val*NUM_LEDS/255);
  for (int i = 0; i < NUM_LEDS; i++) {
    if (i<=num){
      strip.setPixelColor(i, strip.Color(r, g, b));
      } else  {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  strip.show();
  lastUpdate = millis();
}




void Tips_wifi() {
  Serial.println("用户提示：WiFi已断开！");

  // 断网时红色闪3次
  for (int i = 0; i < 3; i++) {
    setColor(255, 0, 0, 255);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    setColor(0, 0, 0, 0);
    vTaskDelay(300 / portTICK_PERIOD_MS);
  }

}



void setup() {
  Serial.begin(115200);
  // 注册WiFi事件回调
  WiFi.onEvent(onWiFiEvent);

  strip.begin();
  strip.show();
  setColor(0,0,0,0);

  WiFi.config(local_IP, gateway, subnet);
  wifiConn.connect();
  wifiConn.startAutoReconnect(Tips_wifi, 5000); 

}


// unsigned long lastFpsTime = 0;
// unsigned int frameCount = 0;
void loop() {

    if (udpEnabled) {
  int packetSize = udp.parsePacket();
  if (packetSize == 4) {
    uint8_t colors[4];
    int len = udp.read(colors, 4);
    if (len == 4) {
      // setColor(colors[0], colors[1], colors[2], colors[3]);
      Serial.printf("RGB_UDP: %d,%d,%d\n", colors[0], colors[1], colors[2]);
      
      // frameCount++;
      // if (millis() - lastFpsTime >= 1000) {
      //   Serial.printf("UDP接收帧率: %d FPS\n", frameCount);
      //   frameCount = 0;
      //   lastFpsTime = millis();
      //   }
    }
  }
  if (millis() - lastUpdate > timeout) {
    // setColor(0, 0, 0,0);
    lastUpdate = millis();
  }}else {
    // Serial.println("UDP不可用，等待WiFi连接...");
    // setColor(0,0,0,0);
  }

}