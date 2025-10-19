#include "WiFiConnect.h"

const byte DNS_PORT = 53;

WiFiConnect* WiFiConnect::instance_ = nullptr;

// 类的析构函数
WiFiConnect::WiFiConnect(const char* apSsid, const char* apPassword, int apPort)
    : ap_ssid_(apSsid),
      ap_password_(apPassword),
      ap_port_(apPort),
      server_(apPort),  // 初始化直接调用构造
      connectWiFiTask_(NULL),
      DnsServiceTask_(NULL),
      checkWiFiTask_(NULL),
      wifiReconnectTask_(NULL),
      wifi_connect_result_(-1) 
{
  if (instance_ != nullptr) {
    Serial.println("Error: Only one WiFiConnect instance allowed!");
    abort();
  }
  instance_ = this;
}
// 在普通成员函数里，你可以直接用成员变量名（或this->），不需要obj->，obj是静态函数或全局函数访问成员时用的
/*=======================public===================================*/
// 连接函数
void WiFiConnect::connect() {
  prefs_.begin("wifi", true);
  String ssid = prefs_.getString("ssid", "");
  String password = prefs_.getString("password", "");
  prefs_.end();

  bool wifi_connected = false;

  if (ssid.length() > 0 && password.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    int timeout = 5;  // 最大约5秒
    while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
      delay(500);
      Serial.print(".");
    }
    wifi_connected = (WiFi.status() == WL_CONNECTED);
    Serial.println();
  }
  if (wifi_connected) {
    Serial.println("WiFi连接成功！");
    Serial.print("设备IP地址: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("未能连接WiFi或未配置，启动AP配网模式...");
    setupAP();
  }
}

// 是否已连接
bool WiFiConnect::isConnected() const { return WiFi.status() == WL_CONNECTED; }

// 获取当前IP地址
IPAddress WiFiConnect::getIP() const { return WiFi.localIP(); }

/**
 * 开启自动重连
*/
void WiFiConnect::startAutoReconnect(WiFiTipCallback cb, uint32_t tipInterval) {
  tipCallback_ = cb;
  tipInterval_ = tipInterval; // 保存到成员变量,提示间隔
  if (checkWiFiTask_ == NULL) {
    xTaskCreate(checkWiFiTaskFunc, "checkWiFiTaskFunc", 4096, NULL, 2,
                &checkWiFiTask_);
  }
}

// 释放资源
void WiFiConnect::end() {
  if (DnsServiceTask_) {
    vTaskDelete(DnsServiceTask_);
    DnsServiceTask_ = NULL;
  }
  if (connectWiFiTask_) {
    vTaskDelete(connectWiFiTask_);
    connectWiFiTask_ = NULL;
  }
  if (checkWiFiTask_) {
    vTaskDelete(checkWiFiTask_);
    checkWiFiTask_ = NULL;
  }
  if (wifiReconnectTask_) {
    vTaskDelete(wifiReconnectTask_);
    wifiReconnectTask_ = NULL;
  }
  server_.stop();
  dnsServer_.stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  prefs_.end();
  instance_ = nullptr;
}
/*=======================private========================================================*/

// 连接页面和路由
void WiFiConnect::setupAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid_.c_str(), ap_password_.c_str());
  IPAddress apIP = WiFi.softAPIP();

  Serial.println("AP模式启动, IP: " + apIP.toString());
  dnsServer_.start(DNS_PORT, "*", apIP);
  Serial.println(apIP);
  // 启动后台DNS服务任务
  if (DnsServiceTask_ == NULL) {
    xTaskCreate(DnsServiceTaskFunc, "DnsServiceTask", 4096, NULL, 3,
                &DnsServiceTask_);
  }
  // 主页
  server_.on("/", HTTP_GET, [this]() {
    server_.send(200, "text/html; charset=utf-8", homepage_html);
  });

  // 响应各大系统的探测路由
  const char* routes[] = {"/generate_204",         "/hotspot-detect.html",
                          "/favicon.ico",          "/captive-portal/index.html",
                          "/apple-touch-icon.png", "/android.ico"};
  for (auto r : routes) {
    server_.on(r, HTTP_GET, [this]() {
      server_.send(200, "text/html; charset=utf-8", homepage_html);
    });
  }
  // 全部404（未定义）统一返回主页
  server_.onNotFound([this]() {
    server_.send(200, "text/html; charset=utf-8", homepage_html);
  });

  // ----- 用户提交WiFi信息 -----
  server_.on("/connect", HTTP_POST, [this]() {
    pending_ssid_ = server_.arg("ssid");
    pending_password_ = server_.arg("password");
    wifi_connect_result_ = -1;
    // 立即响应网页，告诉前端“正在连接...”
    server_.send(200, "text/html; charset=utf-8", html_connect);
    delay(100);  // 确保网页发送完毕
    if (connectWiFiTask_ == NULL)
      // 启动连接WiFi任务
      xTaskCreate(connectWiFiTaskFunc, "ConnectWiFi", 4096, NULL, 2,
                  &connectWiFiTask_);
  
  });

  // 状态路由
  server_.on("/status", HTTP_GET, [this]() {
    String out = "{";
    out += "\"status\":" + String(wifi_connect_result_);
    if (wifi_connect_result_ == 1) {
      out += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
    }
    out += "}";
    server_.send(200, "application/json", out);
  });
  server_.begin();

  //     // 由DnsServiceTaskFunc任务高频路由服务，轮询查找处理DNS与HTTP请求s
  // if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
  //     obj->dnsServer_.processNextRequest();
  //     obj->server_.handleClient();
  // }
}

// wifi连接任务 TASK
void WiFiConnect::connectWiFiTaskFunc(void* param) {
  WiFiConnect* obj = WiFiConnect::instance_;
  Serial.println("开始WiFi连接Task...");
    if (WiFi.getMode() != WIFI_AP_STA)
      WiFi.mode(WIFI_AP_STA);
  WiFi.begin(obj->pending_ssid_.c_str(), obj->pending_password_.c_str());
  int timeout = 10;
  while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  if (WiFi.status() == WL_CONNECTED) {
    obj->wifi_connect_result_ = 1;     // 成功连接标志
    obj->prefs_.begin("wifi", false);  // false表示非只读
    obj->prefs_.putString("ssid", obj->pending_ssid_);
    obj->prefs_.putString("password", obj->pending_password_);
    obj->prefs_.end();
    vTaskDelay(5000 / portTICK_PERIOD_MS);  // 网页有时间获取状态
    WiFi.softAPdisconnect(true);            // 现在关闭AP
    if (obj->DnsServiceTask_ != NULL) {
      // 释放后台资源
      vTaskDelete(obj->DnsServiceTask_);
      obj->DnsServiceTask_ = NULL;
    }
  } else {
    obj->wifi_connect_result_ = 0;
    WiFi.disconnect();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }

  obj->connectWiFiTask_ = NULL;
  vTaskDelete(NULL);
  // ESP.restart();
}

// 后台路由  Task
void WiFiConnect::DnsServiceTaskFunc(void* param) {
  WiFiConnect* obj = WiFiConnect::instance_;
  while (true) {
    // 高频路由服务，仅AP或AP_STA模式下运行
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
      obj->dnsServer_.processNextRequest();
      obj->server_.handleClient();
    }
    // 高频频率 xTaskDelay 10ms
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// 断网检测 Task 断网重连
void WiFiConnect::checkWiFiTaskFunc(void* param) {
    WiFiConnect* obj = WiFiConnect::instance_;
    bool lastConnected = (WiFi.status() == WL_CONNECTED);

    while (true) {
        bool nowConnected = (WiFi.status() == WL_CONNECTED);

        // 1. 断网时，创建断网提示任务（只创建一次）
        if (lastConnected && !nowConnected) {
            if (obj->wifiTipTask_ == NULL && obj->tipCallback_) {
                xTaskCreate(tipTaskFunc, "WiFiTipTask", 2048, obj, 2,
                            &(obj->wifiTipTask_));
                Serial.println("[RTOS] 断网提示Task启动");
            }
            if (obj->wifiReconnectTask_ == NULL &&
                (WiFi.getMode() == WIFI_STA)) {
                xTaskCreate(
                    [](void*) {
                        WiFiConnect::instance_->connect();
                        WiFiConnect::instance_->wifiReconnectTask_ = NULL;
                        vTaskDelete(NULL);
                    },
                    "WiFiReconnect", 4096, NULL, 2, &(obj->wifiReconnectTask_));
                Serial.println("[RTOS] WiFi断开，准备重连");
            }
        }

        // 2. 联网时，清除断网提示任务
        if (!lastConnected && nowConnected && obj->wifiTipTask_ != NULL) {
            vTaskDelete(obj->wifiTipTask_);
            obj->wifiTipTask_ = NULL;
            Serial.println("[RTOS] WiFi恢复，断网提示Task销毁");
        }

        lastConnected = nowConnected;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//断网提示：
void WiFiConnect::tipTaskFunc(void* param) {
    WiFiConnect* obj = (WiFiConnect*)param;
    while (WiFi.status() != WL_CONNECTED) {
        if (obj->tipCallback_) obj->tipCallback_(); // 执行用户回调
        vTaskDelay(obj->tipInterval_ / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL); // 联网后自动销毁
}