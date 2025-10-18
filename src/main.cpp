#include <WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>

#define NEOPIXEL_PIN   1
#define NUM_LEDS       60

const char* ssid     = "RAY";
const char* password = "12345678";
IPAddress local_IP(192, 168, 137, 50);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiUDP udp;
unsigned int udpPort = 8888;

Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastUpdate = 0;
const unsigned long timeout = 1000;

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
  lastUpdate = millis();
}

void setup() {
  Serial.begin(115200);
  strip.begin();
  strip.show();
  setColor(0,0,0);

  WiFi.config(local_IP, gateway, subnet);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tries++;
    if (tries > 60) {
      Serial.println("\nWiFi连接失败！");
      while(1) delay(500);
    }
  }
  Serial.print("\nConnected. IP: ");
  Serial.println(WiFi.localIP());

  udp.begin(udpPort);
  Serial.printf("UDP监听端口: %d\n", udpPort);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize == 3) {
    uint8_t colors[3];
    int len = udp.read(colors, 3);
    if (len == 3) {
      setColor(colors[0], colors[1], colors[2]);
      // Serial.printf("收到RGB二进制UDP: %d,%d,%d\n", colors[0], colors[1], colors[2]);
    }
  }
  if (millis() - lastUpdate > timeout) {
    setColor(0, 0, 0);
    lastUpdate = millis();
  }
}