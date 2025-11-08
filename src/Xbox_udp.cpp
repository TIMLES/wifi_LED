#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "WiFiConnect.h"

// 创建全局对象（推荐放在setup之前）
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

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);

// OLED窗口、球属性
const int width = 72;
const int height = 40;
const int xOffset = 28;
const int yOffset = 24;
int ball_x = width / 2;
int ball_y = height / 2;
int ball_radius = 3;

// 子弹支持
struct Bullet {
  float x, y;      // 坐标(基于窗口width/height)
  float vx, vy;    // 速度
  int ttl;         // 剩余生命（帧数）
};
const int MAX_BULLET = 20;
Bullet bullets[MAX_BULLET];
int bullet_cnt = 0;
unsigned long last_shot_time = 0;
const unsigned long SHOT_INTERVAL = 120; // 毫秒（调节连发速度）

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  WiFi.onEvent(onWiFiEvent);
  WiFi.config(local_IP, gateway, subnet);
  wifiConn.connect();
  wifiConn.startAutoReconnect();
}

unsigned long lastFpsTime = 0;
unsigned int frameCount = 0;
// 敌人参数
float enemy_x = width / 8, enemy_y = height / 8;
float enemy_vx = 1.0, enemy_vy = 0.5;
bool player_alive = true;
bool enemy_alive = true;
unsigned long last_enemy_respawn = 0;

void loop() {
  if (udpEnabled) {
    int packetSize = udp.parsePacket();
    if (packetSize == 14) {
      uint8_t msg[14];
      int len = udp.read(msg, 14);
      if (len == 14) {
        // 解析手柄数据
        int lx = msg[0];  // 左摇杆X
        int ly = msg[1];  // 左摇杆Y
        int rx = msg[2];  // 右摇杆X
        int ry = msg[3];  // 右摇杆Y
        int lt = msg[4];  // 左扳机
        int rt = msg[5];  // 右扳机
        int dpad_up = msg[6];
        int dpad_down = msg[7];
        int dpad_left = msg[8];
        int dpad_right = msg[9];
        int btn_a = msg[10];
        int btn_b = msg[11];
        int btn_x = msg[12];
        int btn_y = msg[13];

        // 小球运动逻辑（同之前）
        int vx = map(lx, 0, 255, -5, 5) + dpad_right - dpad_left;
        vx = -vx;
        int vy = map(ly, 0, 255, -5, 5) + dpad_up - dpad_down;
        ball_x += vx;
        ball_y += vy;
        // 边界处理
        if (ball_x - ball_radius < 0) { ball_x = ball_radius; }
        if (ball_x + ball_radius > width) { ball_x = width - ball_radius; }
        if (ball_y - ball_radius < 0) { ball_y = ball_radius; }
        if (ball_y + ball_radius > height) { ball_y = height - ball_radius; }

        // ---- 连发射击功能 ----
        // 计算射线方向（用右摇杆）
        float dir_x = -map(rx, 0, 255, -100, 100) / 100.0;
        float dir_y = map(ry, 0, 255, -100, 100) / 100.0;
        float mag = sqrt(dir_x * dir_x + dir_y * dir_y);
        if (mag < 0.1) { dir_x = -1; dir_y = 0; } else { dir_x /= mag; dir_y /= mag; }

        bool aiming = (lt > 50);
        bool shooting = aiming && (rt > 50);

        // 连发
        unsigned long now = millis();
        if (aiming && shooting) {
          if (now - last_shot_time > SHOT_INTERVAL) {
            if (bullet_cnt < MAX_BULLET) {
              bullets[bullet_cnt].x = ball_x;
              bullets[bullet_cnt].y = ball_y;
              bullets[bullet_cnt].vx = dir_x * 4.0;
              bullets[bullet_cnt].vy = dir_y * 4.0;
              bullets[bullet_cnt].ttl = 60; //1秒左右
              bullet_cnt++;
            }
            last_shot_time = now;
          }
        } else {
          last_shot_time = now; // 防止松开后长按重新计时导致瞬间刷屏
        }



  if (!player_alive) {
     // 死亡时可延时重启或按A键重生
     if(btn_a) { player_alive=true; ball_x=width/2; ball_y=height/2; }
  }
  if (!enemy_alive && millis()-last_enemy_respawn > 1200){
      enemy_x = random(10, width-10);
      enemy_y = random(10, height-10);
      enemy_vx = (random(0,2)==0?1.0:-1.0) * 0.5;
      enemy_vy = (random(0,2)==0?1.0:-1.0) * 0.6;
      enemy_alive = true;
  }
  // 敌人运动
  if(enemy_alive){
    enemy_x += enemy_vx;
    enemy_y += enemy_vy;
    if (enemy_x < 5 || enemy_x > width - 5) enemy_vx = -enemy_vx;
    if (enemy_y < 5 || enemy_y > height - 5) enemy_vy = -enemy_vy;
  }
  // 玩家与敌人碰撞
  if(player_alive && enemy_alive){
     float dp2 = sq(ball_x-enemy_x) + sq(ball_y-enemy_y);
     if (dp2 < sq(ball_radius+5)) player_alive=false;
  }


        // ---------- OLED绘制 ----------
// OLED绘制整合
u8g2.clearBuffer();
u8g2.drawFrame(xOffset, yOffset, width, height);

if (player_alive)
  u8g2.drawCircle(xOffset + ball_x, yOffset + ball_y, ball_radius, U8G2_DRAW_ALL);

if (enemy_alive)
  u8g2.drawDisc(xOffset + int(enemy_x), yOffset + int(enemy_y), 5, U8G2_DRAW_ALL);

// 画所有子弹(已经在for循环里画)
for(int i=0; i<bullet_cnt;) {
  bullets[i].x += bullets[i].vx; bullets[i].y += bullets[i].vy; bullets[i].ttl--;
  if(enemy_alive) {
    float dist2 = sq(enemy_x-bullets[i].x)+sq(enemy_y-bullets[i].y);
    if(dist2 < sq(5+2)){
      enemy_alive = false;
      last_enemy_respawn = millis();
    }
  }
  bool out = bullets[i].x < 0 || bullets[i].x > width || bullets[i].y < 0 || bullets[i].y > height || bullets[i].ttl <= 0;
  u8g2.drawDisc(xOffset + int(bullets[i].x), yOffset + int(bullets[i].y), 2, U8G2_DRAW_ALL);
  if(out) bullets[i]=bullets[--bullet_cnt];
  else i++;
}

// 瞄准线
const float line_len = 12.0;
int sx = xOffset + ball_x, sy = yOffset + ball_y;
int ex = int(sx + line_len * dir_x), ey = int(sy + line_len * dir_y);
if (aiming) {
  u8g2.drawLine(sx, sy, ex, ey);
}

// Game Over提示
if (!player_alive) {
  u8g2.setFontDirection(2);
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(xOffset + 60, yOffset + height / 2, "GAME OVER");
}

u8g2.sendBuffer();

        // 帧速率统计
        frameCount++;
        if (millis() - lastFpsTime >= 1000) {
          Serial.printf("UDP接收帧率: %d FPS | 子弹: %d\n", frameCount, bullet_cnt);
          frameCount = 0;
          lastFpsTime = millis();
        }
      }
    }
  }
}