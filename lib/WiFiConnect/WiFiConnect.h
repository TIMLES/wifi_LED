#pragma once  // 只编译一次，防止头文件重复包含
#include <Arduino.h>

// 页面HTML内容声明（可以在 WiFiConnect.cpp 中定义具体内容）
extern const char* homepage_html;  // 配网主页HTML
extern const char* html_connect;   // 连接等待页面HTML

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

// 兼容 ESP32 / S3 / C3
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3
#define HAS_DUAL_CORE 1
#else
#define HAS_DUAL_CORE 0
#endif

#if HAS_DUAL_CORE
#define CORE_APP 1  // APP_CPU
#define CORE_PRO 0  // PRO_CPU
#else
#define CORE_APP 0  // 单核设备上只能是 0
#define CORE_PRO 0
#endif
/**
 * WiFiConnect
 * -----------
 * 集成ESP32 WiFi连接、失败AP配网、断网自动重连功能的管理类。
 * 主文件只需 new 一个对象并调用 connect(), startAutoReconnect()。
 */

// 自定义回调函数，用于WiFi状态变化提示
typedef void (
    *WiFiTipCallback)();  // typedef void
                          // 函数名();表示定义一个函数指针类型，函数指针指向的函数

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
   * @param cb
   * 断网提示回调函数指针（void类型，无参数），断网时会定时调用；可省略，省略则不提示。
   * @param tipInterval
   * 断网提示回调调用间隔（单位：毫秒），默认为1000ms（1秒），可根据实际需要设定。
   */
  void startAutoReconnect(WiFiTipCallback cb = nullptr,
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

  /**
   * 启动AP模式
   * 用户在STA失败后自动转为AP模式调用
   */
  void setupAP();

  /**
   * FreeRTOS任务：wifi连接。
   * 用来在网页填写完ssid/password后异步尝试连接并保存配置
   * @param param 任务参数（未用）
   */
  static void connectWiFiTaskFunc(
      void* param);  // xTaskCreate 只能接受静态函数或普通全局函数

  /**
   * FreeRTOS任务：DNS服务。
   * 用于轮询DNS处理网页/断网检测
   * @param param 任务参数（未用）
   */
  static void DnsServiceTaskFunc(void* param);

  /**
   * 专为FreeRTOS任务系统设计（xTaskCreate）
   * 用于断网检测并自动重连
   * @param param 任务参数（未用）
   */
  static void checkWiFiTaskFunc(void* param);

  /**
   * FreeRTOS任务：断网提示
   * 用于断网检测并自动重连
   * @param param 任务参数（未用）
   */
  static void tipTaskFunc(void* param);  // 提示任务体

  // ============== AP——STA配置参数 ===============
  String ap_ssid_;      // AP模式热点名
  String ap_password_;  // AP模式密码
  int ap_port_;         // AP模式Web服务端口
  //  临时变量：连接/配网状态控制
  String pending_ssid_;          // 等待连接的SSID（网页提交、异步连接用）
  String pending_password_;      // 等待连接的密码（网页提交、异步连接用）
  uint32_t tipInterval_ = 1000;  // 提示间隔（默认1000ms）
  // ============== 系统对象和状态管理 ================
  DNSServer dnsServer_;  // DNS服务器对象（强制网页门户用）
  WebServer server_;     // Web服务器对象（用于配网页面）
  Preferences prefs_;    // Flash参数存储（保存WiFi账号密码）

  TaskHandle_t connectWiFiTask_;    //    正在执行连接任务句柄
  TaskHandle_t DnsServiceTask_;     // 正在执行DNS服务(仅AP模式)任务句柄
  TaskHandle_t checkWiFiTask_;      // 正在断网检测任务句柄
  TaskHandle_t wifiReconnectTask_;  // 正在执行重连任务句柄

  WiFiTipCallback tipCallback_ = nullptr;  // 用户自定义回调
  TaskHandle_t wifiTipTask_ = NULL;        // 提示任务句柄

  volatile int wifi_connect_result_;  // 连接结果：-1等待, 0失败, 1成功

  // ============== 静态指针：任务入口用 ================
  /**
   * 用于FreeRTOS任务回调，将静态方法与本对象实例绑定
   */
  static WiFiConnect* instance_;
};