#pragma once  // 只编译一次，防止头文件重复包含
#include <Arduino.h>

// 页面HTML内容声明（可以在 WiFiConnect.cpp 中定义具体内容）
extern const char* homepage_html;  // 配网主页HTML
extern const char* html_connect;   // 连接等待页面HTML

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>


/**
 * WiFiConnect
 * -----------
 * 集成ESP32 WiFi连接、失败AP配网、断网自动重连功能的管理类。
 * 主文件只需 new 一个对象并调用 connect(), startAutoReconnect()。
 */
// ============= WiFi操作消息队列相关类型定义 =============
enum WiFiOpType {
    WIFI_OP_CONNECT,    // STA连接
    WIFI_OP_SETUP_AP,   // 启动AP模式
    WIFI_OP_DISCONNECT, // 断开WiFi
    WIFI_OP_RECONNECT,  // 自动重连
};

struct WiFiMessage {
    WiFiOpType type;
    String ssid;
    String password;
    // 可扩展更多参数
};



// 自定义回调函数，用于WiFi状态变化提示
typedef void (*WiFiTipCallback)();  // typedef void
                          

class WiFiConnect {
 public:
  /**
   * 构造函数
   * @param apSsid 配网AP热点名称
   * @param apPassword 配网AP热点密码
   * @param apPort 配网Web服务端口（一般为80）
   */
  WiFiConnect(const char* apSsid, const char* apPassword, int apPort = 80);

  /**
   * WiFi连接入口
   * 自动尝试STA模式连接保存的WiFi，失败则切换为配网AP模式并提供网页配网入口。
   */
  void connect();

  /**
   * 启动断网自动重连功能。
   * @param connect
   * 断网提示回调函数指针（void类型，无参数），联网时仅仅会调用一次；可省略，省略则不提示。
   * @param disconnect
   * 断网提示回调函数指针（void类型，无参数），断网时会定时调用多次；可省略，省略则不提示。
   * @param tipInterval
   * 断网提示回调调用间隔（单位：毫秒），默认为1000ms（1秒），可根据实际需要设定。
   */
  void startAutoReconnect(WiFiTipCallback connect = nullptr,
                          WiFiTipCallback disconnect = nullptr,
                          uint32_t tipInterval = 1000);

  /**
   * 当前WiFi是否已连接
   * @return true已连接，false未连接
   */
  bool isConnected() const;

  /**
   * 获取当前WiFi的IP地址
   * @return IPAddress对象
   */
  IPAddress getIP() const;

  /**
   * 结束，释放资源
   * 注意：调用此函数后如需重新连接请重新创建WiFiConnect对象
   */
  void end();  // 结束WiFiConnect，释放资源

  
 private:
  // ================== 内部功能实现区 ===================

  // 启动AP模式
  void setupAP();

  // ======= WiFi管理任务，从队列统一调度所有WiFi操作 =======
  static void WiFiManagerTaskFunc(void* param);
  //* FreeRTOS任务：DNS服务。
  static void DnsServiceTaskFunc(void* param);
  //断网检测线程
  static void checkWiFiTaskFunc(void* param);
  //断网提示
  static void disconnect_tipTaskFunc(void* param);  // 提示任务


  // ============== AP——STA配置参数 ===============
  String ap_ssid_;      // AP模式热点名
  String ap_password_;  // AP模式密码
  int ap_port_;         // AP模式Web服务端口
 
  uint32_t tipInterval_ = 1000;  // 提示间隔（默认1000ms）
  // ============== 系统对象和状态管理 ================
  DNSServer dnsServer_;  // DNS服务器对象（强制网页门户用）
  WebServer server_;     // Web服务器对象（用于配网页面）
  Preferences prefs_;    // Flash参数存储（保存WiFi账号密码）

    // ======= 任务和队列句柄 =======
  QueueHandle_t wifiQueue_;         // WiFi操作消息队列
  TaskHandle_t wifiManagerTask_;    // WiFi统一管理任务
  TaskHandle_t checkWiFiTask_;      // 断网检测任务
  TaskHandle_t disconnect_wifiTipTask_;        // 断网提示任务

  TaskHandle_t DnsServiceTask_;     // 正在执行DNS服务(仅AP模式)任务句柄



  WiFiTipCallback connect_tipCallback_ = nullptr;  // 用户自定义回调
  WiFiTipCallback disconnect_tipCallback_ = nullptr;  // 用户自定义回调


  volatile int wifi_connect_result_;  // 连接结果：-1等待, 0失败, 1成功

  // ============== 静态指针：任务入口用 ================
  /**
   * 用于FreeRTOS任务回调，将静态方法与本对象实例绑定
   */
  static WiFiConnect* instance_;
};