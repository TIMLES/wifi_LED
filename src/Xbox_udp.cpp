#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "WiFiConnect.h"
#include <U8g2lib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

WiFiConnect wifiConn(
    "WifiConnectMyDevice_ESP_AP",
    "wJZagHy0xz9EISm"
);

IPAddress local_IP(192, 168, 137, 51);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);

WiFiUDP udp;
unsigned int udpPort = 4210;
bool udpEnabled = false;

// 手柄数据结构体
struct ControllerData {
  int lx, ly, rx, ry, lt, rt;
  int dpad_up, dpad_down, dpad_left, dpad_right;
  int btn_a, btn_b, btn_x, btn_y;
  int btn_lb, btn_rb;
};
ControllerData g_controllerData;

SemaphoreHandle_t xDataMutex;    // 控制数据同步互斥
SemaphoreHandle_t xNewDataEvent; // 新数据到达信号量
SemaphoreHandle_t xUdpMutex;     // UDP互斥量

IPAddress lastRemoteIP;
unsigned int lastRemotePort;
unsigned int lastSendFps = 0;    // FPS统计

void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFi] STA已连接（未获得IP）");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[WiFi] 获得IP: ");
      Serial.println(WiFi.localIP());
      udp.begin(udpPort);
      Serial.println("UDP已初始化（WiFi获得IP）");
      udpEnabled = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[WiFi] STA断开，UDP不可用");
      udpEnabled = false;
      break;
    default:
      break;
  }
}

// UDP接收任务
void UDPReceiveTask(void* pvParameters) {
  uint8_t msg[16];
  ControllerData lastData = {0};
  unsigned long lastFpsTime = millis();
  unsigned int frameCount = 0;
  while (1) {
    if (udpEnabled) {
      xSemaphoreTake(xUdpMutex, portMAX_DELAY);  // 收包加锁
      int packetSize = udp.parsePacket();
      if (packetSize == 16) {
        int len = udp.read(msg, 16);
        if (len == 16) {
          // 记住发送方的IP/端口【重点】
          lastRemoteIP = udp.remoteIP();
          lastRemotePort = udp.remotePort();

          // 解析数据
          ControllerData data;
          data.lx = msg[0]; data.ly = msg[1];
          data.rx = msg[2]; data.ry = msg[3];
          data.lt = msg[4]; data.rt = msg[5];
          data.dpad_up    = msg[6];
          data.dpad_down  = msg[7];
          data.dpad_left  = msg[8]; data.dpad_right = msg[9];
          data.btn_a = msg[10]; data.btn_b = msg[11];
          data.btn_x = msg[12]; data.btn_y = msg[13];
          data.btn_lb = msg[14]; data.btn_rb = msg[15];
          // 比较前后数据
          bool changed = memcmp(&lastData, &data, sizeof(ControllerData)) != 0;
          if (changed) {
            xSemaphoreTake(xDataMutex, portMAX_DELAY);
            g_controllerData = data;
            xSemaphoreGive(xDataMutex);
            xSemaphoreGive(xNewDataEvent);
            lastData = data;
          }
          frameCount++;
          if (millis() - lastFpsTime >= 1000) {
            Serial.printf("UDP接收帧率: %d FPS [TASK]\n", frameCount);
            // 更新统计FPS（供发送任务用）
            xSemaphoreTake(xDataMutex, portMAX_DELAY);
            lastSendFps = frameCount;
            xSemaphoreGive(xDataMutex);

            frameCount = 0;
            lastFpsTime = millis();
            uint32_t freeHeap = ESP.getFreeHeap();
            uint32_t totalHeap = ESP.getHeapSize();
            float usage = (totalHeap - freeHeap) * 100.0f / totalHeap;
            Serial.printf("内存使用: %.2f%% (%u / %u bytes)\n", usage, totalHeap - freeHeap, totalHeap);
          }
        }
      }
      xSemaphoreGive(xUdpMutex); // 收包解锁
    }
    vTaskDelay(1/portTICK_PERIOD_MS);
  }
}

// 新增FPS发送任务
void UDPSendFpsTask(void* pvParameters) {
  IPAddress prevIP;
  unsigned int prevPort = 0;
  while (1) {
    // 取最新FPS数据和目标IP、端口
    unsigned int fpsCopy = 0;
    IPAddress remoteIP;
    unsigned int remotePort;

    // 保护全局数据读取
    xSemaphoreTake(xDataMutex, portMAX_DELAY);
    fpsCopy = lastSendFps;
    xSemaphoreGive(xDataMutex);

    remoteIP = lastRemoteIP;
    remotePort = lastRemotePort;

    // 如果 remoteIP 有效（兼容首次未收到包场景）
    if (remotePort != 0) {
      // 构造内容
      char buf[32];
      snprintf(buf, sizeof(buf), "ESP_FPS:%u", fpsCopy);

      xSemaphoreTake(xUdpMutex, portMAX_DELAY);
      udp.beginPacket(remoteIP, remotePort);  // 目标用电脑实际IP和端口
      udp.write((uint8_t*)buf, strlen(buf));
      udp.endPacket();
      xSemaphoreGive(xUdpMutex);
    }

    vTaskDelay(1000/portTICK_PERIOD_MS); // 每秒发一次
  }
} 

void setup() {
  Serial.begin(115200);
  WiFi.onEvent(onWiFiEvent);
  WiFi.config(local_IP, gateway, subnet);
  wifiConn.connect();
  wifiConn.startAutoReconnect();

  xDataMutex = xSemaphoreCreateMutex();
  xNewDataEvent = xSemaphoreCreateBinary();
  xUdpMutex = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(
    UDPReceiveTask,
    "UDPRecvTask",
    4096,
    NULL,
    1,
    NULL,
    1
  );
  xTaskCreatePinnedToCore(
    UDPSendFpsTask,
    "UDPSendFpsTask",
    2048,
    NULL,
    1,
    NULL,
    1
  );
}

void loop() {
  if (xSemaphoreTake(xNewDataEvent, 50 / portTICK_PERIOD_MS) == pdTRUE) {
    xSemaphoreTake(xDataMutex, portMAX_DELAY);
    ControllerData data = g_controllerData;
    xSemaphoreGive(xDataMutex);
    Serial.printf("LX:%d LY:%d RX:%d RY:%d BTN_A:%d\n",
      data.lx, data.ly, data.rx, data.ry, data.btn_a);
  }
  // 其它loop循环动作...
}