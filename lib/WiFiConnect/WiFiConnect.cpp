#include "WiFiConnect.h"

const byte DNS_PORT = 53;
WiFiConnect* WiFiConnect::instance_ = nullptr;

// 类的析构函数
WiFiConnect::WiFiConnect(const char* apSsid, const char* apPassword, int apPort)
    : ap_ssid_(apSsid),
      ap_password_(apPassword),
      ap_port_(apPort),
      server_(apPort),  // 初始化直接调用构造
      wifiManagerTask_(NULL),
      DnsServiceTask_(NULL),
      checkWiFiTask_(NULL),
      disconnect_wifiTipTask_(NULL),
      wifi_connect_result_(-1) 
{
  if (instance_ != nullptr) {
    Serial.println("Error: Only one WiFiConnect instance allowed!");
    abort();
  }
  instance_ = this;

      wifiQueue_ = xQueueCreate(8, sizeof(WiFiMessage)); // 系统WiFi事务队列
    if (!wifiQueue_) {
        Serial.println("WiFiConnect队列创建失败!");
        abort();
    }
    xTaskCreate(
        WiFiManagerTaskFunc, "WiFiManagerTask", 4096, nullptr, 2, &wifiManagerTask_ // 放APP或0核
    );
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
  // 投递连接请求到队列（自动由管理Task串行处理）
  if (ssid.length() > 0 && password.length() > 0) {
      WiFiMessage msg;
      msg.type = WIFI_OP_CONNECT;//连接已有密码
      msg.ssid = ssid;
      msg.password = password;
      wifi_connect_result_ = -1;
      xQueueSend(wifiQueue_, &msg, pdMS_TO_TICKS(100));
  } else {
      WiFiMessage msg;
      msg.type = WIFI_OP_SETUP_AP;//启动AP配网
      wifi_connect_result_ = -1;
      xQueueSend(wifiQueue_, &msg, pdMS_TO_TICKS(100));
  }

}

// 是否已连接
bool WiFiConnect::isConnected() const { return WiFi.status() == WL_CONNECTED; }

// 获取当前IP地址
IPAddress WiFiConnect::getIP() const { return WiFi.localIP(); }

// 启动断网检测，自动重连
void WiFiConnect::startAutoReconnect(WiFiTipCallback connect,WiFiTipCallback disconnect,uint32_t tipInterval){
  disconnect_tipCallback_ = disconnect;
  connect_tipCallback_ = connect;
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
  if (checkWiFiTask_) {
    vTaskDelete(checkWiFiTask_);
    checkWiFiTask_ = NULL;
  }
  if (wifiManagerTask_) {
    vTaskDelete(wifiManagerTask_);
    wifiManagerTask_ = NULL;
  }
  if (disconnect_wifiTipTask_) {
    vTaskDelete(disconnect_wifiTipTask_);
    disconnect_wifiTipTask_ = NULL;
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

  // ----- 用户提交WiFi信息 -----直接投递队列（不负责连接！）
  server_.on("/connect", HTTP_POST, [this]() {
    WiFiMessage msg;
    msg.type = WIFI_OP_CONNECT;
    msg.ssid = server_.arg("ssid");
    msg.password = server_.arg("password");
    wifi_connect_result_ = -1; // 等待连接
    // 立即响应网页，告诉前端“正在连接...”
    server_.send(200, "text/html; charset=utf-8", html_connect);
    delay(100);  // 确保网页发送完毕
    xQueueSend(wifiQueue_, &msg, pdMS_TO_TICKS(100));
  
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
}


// ===== WiFi核心串行管理Task =====
void WiFiConnect::WiFiManagerTaskFunc(void* param) {
    WiFiConnect* obj = WiFiConnect::instance_;
    WiFiMessage msg;
    for (;;) {
        if (xQueueReceive(obj->wifiQueue_, &msg, portMAX_DELAY) == pdPASS) {
            switch (msg.type) {
            case WIFI_OP_CONNECT:
                Serial.println("【WiFiManagerTask】开始连接WiFi...");
                if (WiFi.getMode() != WIFI_AP_STA) WiFi.mode(WIFI_AP_STA);
                WiFi.begin(msg.ssid.c_str(), msg.password.c_str());
                {
                    int timeout = 10;
                    while (WiFi.status() != WL_CONNECTED && timeout-- > 0) {
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        Serial.print(".");
                    }
                    Serial.println();
                    if (WiFi.status() == WL_CONNECTED) {
                        obj->wifi_connect_result_ = 1;
                        obj->prefs_.begin("wifi", false);
                        obj->prefs_.putString("ssid", msg.ssid);
                        obj->prefs_.putString("password", msg.password);
                        obj->prefs_.end();
                        Serial.println("WiFi连接成功，IP: " + WiFi.localIP().toString());
                        vTaskDelay(3000 / portTICK_PERIOD_MS);
                        WiFi.softAPdisconnect(true);// 现在关闭AP
                        if (obj->DnsServiceTask_ != NULL) {
                          // 释放后台资源
                          vTaskDelete(obj->DnsServiceTask_);
                          obj->DnsServiceTask_ = NULL;
                        }
                    } else {
                        obj->wifi_connect_result_ = 0;
                        WiFi.disconnect();
                        Serial.println("WiFi连接失败或超时");
                        // 自动切AP模式
                        WiFiMessage apMsg;
                        apMsg.type = WIFI_OP_SETUP_AP;
                        xQueueSend(obj->wifiQueue_, &apMsg, pdMS_TO_TICKS(100));
                    }
                }
                break;
            case WIFI_OP_SETUP_AP:
                Serial.println("【WiFiManagerTask】切换到AP模式等待配网");
                obj->setupAP();
                break;
            case WIFI_OP_DISCONNECT:
                Serial.println("【WiFiManagerTask】断开WiFi...");
                WiFi.disconnect(true);
                WiFi.mode(WIFI_OFF);
                obj->wifi_connect_result_ = 0;
                break;
            } // switch
        } // if
    } // for
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
            if (obj->disconnect_wifiTipTask_ == NULL && obj->disconnect_tipCallback_) {
                xTaskCreate(disconnect_tipTaskFunc, "WiFiTipTask", 4096, obj, 2,
                            &(obj->disconnect_wifiTipTask_));
                Serial.println("[RTOS] 断网提示Task启动");
            }
            // 投递重连消息，不再直接调用get/connect
            obj->prefs_.begin("wifi", true);
            String ssid = obj->prefs_.getString("ssid", "");
            String password = obj->prefs_.getString("password", "");
            obj->prefs_.end();
            WiFiMessage msg;
            msg.type = WIFI_OP_CONNECT;
            msg.ssid = ssid;
            msg.password = password;
            xQueueSend(obj->wifiQueue_, &msg, pdMS_TO_TICKS(100));
            Serial.println("[RTOS] WiFi断开，投递重连请求");
        }

        // 2. 联网时，清除断网提示任务
        if (!lastConnected && nowConnected && obj->disconnect_wifiTipTask_ != NULL) {
            vTaskDelete(obj->disconnect_wifiTipTask_);
            obj->disconnect_wifiTipTask_ = NULL;
            Serial.println("[RTOS] WiFi恢复，断网提示Task销毁");
            if (obj->connect_tipCallback_) {
                obj->connect_tipCallback_(); // 直接调，不需要起任务
            }
        }

        lastConnected = nowConnected;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//断网提示：
void WiFiConnect::disconnect_tipTaskFunc(void* param) {
    WiFiConnect* obj = (WiFiConnect*)param;
    while (WiFi.status() != WL_CONNECTED) {
        if (obj->disconnect_tipCallback_) obj->disconnect_tipCallback_(); // 执行用户回调
        vTaskDelay(obj->tipInterval_ / portTICK_PERIOD_MS);
    }
    obj->disconnect_wifiTipTask_ = NULL;  // <---- 这句更健壮
    vTaskDelete(NULL);
}

