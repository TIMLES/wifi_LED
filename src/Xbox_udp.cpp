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

void UDPReceiveTask(void* pvParameters) {
  uint8_t msg[16];
  ControllerData lastData = {0};  // 初始化全为0
  unsigned long lastFpsTime = millis();
  unsigned int frameCount = 0;

  while (1) {
    if (udpEnabled) {
      int packetSize = udp.parsePacket();
      if (packetSize == 16) {
        int len = udp.read(msg, 16);
        if (len == 16) {
          // 解析数据
          ControllerData data;
          data.lx = msg[0]; data.ly = msg[1];
          data.rx = msg[2]; data.ry = msg[3];
          data.lt = msg[4]; data.rt = msg[5];
          data.dpad_up    = msg[6];
          data.dpad_down  = msg[7];
          data.dpad_left  = msg[8];
          data.dpad_right = msg[9];
          data.btn_a = msg[10]; data.btn_b = msg[11];
          data.btn_x = msg[12]; data.btn_y = msg[13];
          data.btn_lb = msg[14]; data.btn_rb = msg[15];

          // 比较前后数据是否有变化
          bool changed = memcmp(&lastData, &data, sizeof(ControllerData)) != 0;
          if (changed) {
            // 数据有变化，写入并通知
            xSemaphoreTake(xDataMutex, portMAX_DELAY);
            g_controllerData = data;
            xSemaphoreGive(xDataMutex);

            xSemaphoreGive(xNewDataEvent);

            // 更新lastData
            lastData = data;
          }

          frameCount++;
          if (millis() - lastFpsTime >= 1000) {
            Serial.printf("UDP接收帧率: %d FPS [TASK]\n", frameCount);
            frameCount = 0;
            lastFpsTime = millis();

                // TODO: 输出当前内存占用百分比
            uint32_t freeHeap = ESP.getFreeHeap();
            uint32_t totalHeap = ESP.getHeapSize();
            float usage = (totalHeap - freeHeap) * 100.0f / totalHeap;
            Serial.printf("内存使用: %.2f%% (%u / %u bytes)\n", usage, totalHeap - freeHeap, totalHeap);
          }
        }
      }
    }
    vTaskDelay(1/portTICK_PERIOD_MS); // 轻微让出CPU
  }
}


void setup() {
  Serial.begin(115200);
  WiFi.onEvent(onWiFiEvent);
  WiFi.config(local_IP, gateway, subnet);
  wifiConn.connect();
  wifiConn.startAutoReconnect();

  // 创建同步机制
  xDataMutex = xSemaphoreCreateMutex();
  xNewDataEvent = xSemaphoreCreateBinary();

  // 启动UDP接收任务
  xTaskCreatePinnedToCore(
    UDPReceiveTask,
    "UDPRecvTask",
    4096,
    NULL,
    1,
    NULL,
    1 // 用core 1
  );
}

void loop() {
  // 阻塞等待新数据到来
  if (xSemaphoreTake(xNewDataEvent, 50 / portTICK_PERIOD_MS) == pdTRUE) {
    // 获取数据并操作（如刷新显示、控制等）
    xSemaphoreTake(xDataMutex, portMAX_DELAY);
    ControllerData data = g_controllerData; // 本地快照
    xSemaphoreGive(xDataMutex);

    // 示例：串口输出所有
    Serial.printf("LX:%d LY:%d RX:%d RY:%d BTN_A:%d\n",
      data.lx, data.ly, data.rx, data.ry, data.btn_a);


  }
  // 其它loop循环动作...
}