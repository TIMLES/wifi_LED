#include "WiFiConnect.h"
// StatusPrinter printer(5000); // 每2秒打印一次内存状态


// 创建全局对象（推荐放在setup之前）
WiFiConnect wifiConn(
    "WifiConnectMyDevice_ESP_AP", // AP热点名
    "wJZagHy0xz9EISm"           // AP热点密码（实际请改强密码）
);

void Tips_wifi() {
  Serial.println("用户提示：WiFi已断开！");
}


void setup() {
  Serial.begin(115200);
  // printer.begin(); // 启动后台自动内存打印
  // 尝试连接WiFi，如失败自动进入AP网页配网
  wifiConn.connect();

  wifiConn.startAutoReconnect(Tips_wifi);
}

void loop() {
  // 检查当前WiFi连接状态
  if (wifiConn.isConnected()) {
    Serial.print("设备已连接到WiFi，IP: ");
    Serial.println(wifiConn.getIP());
    // 你的主业务代码
  } else {
    // Serial.println("设备未连接WiFi，等待配网...");
    // 可显示提示或低功耗处理等
  }

  delay(10000);
}