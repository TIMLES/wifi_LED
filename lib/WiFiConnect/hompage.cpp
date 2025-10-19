
#include "WiFiConnect.h"


const char* homepage_html = R"(
<!DOCTYPE html>
<html lang='zh-CN'>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Wi-Fi配置</title>
  <style>
    body {
      background:#F2F2F2;
      font-family: 'Arial', sans-serif;
      margin:0;
      padding:0;
      display:flex;
      justify-content:center;
      align-items:center;
      height:100vh;
    }
    .container {
      background:#fff;
      border-radius:10px;
      box-shadow:0 2px 8px rgba(0,0,0,0.10);
      padding:32px 20px;
      max-width:350px;
      width:90vw;
    }
    h2 {
      color:#2566c1;
      margin-bottom:25px;
      text-align:center;
    }
    label {
      font-size:15px;
      color:#333;
      display:block;
      margin-bottom:8px;
    }
    input[type=text], input[type=password] {
      width:100%;
      padding:10px;
      margin-bottom:16px;
      border:1px solid #ccc;
      border-radius:5px;
      box-sizing:border-box;
      font-size:16px;
    }
    input[type=submit] {
      width:100%;
      padding:12px;
      background:#2566c1;
      color:#fff;
      border:none;
      border-radius:5px;
      font-size:17px;
      cursor:pointer;
      margin-top:8px;
      transition:background .2s;
    }
    input[type=submit]:hover {
      background:#1453a3;
    }
    .tip {
      font-size:13px;
      color:#666;
      margin-bottom:13px;
      text-align:center;
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>Wi-Fi 配网</h2>
    <div class="tip">请输入您家路由器的Wi-Fi名称和密码</div>
    <form action='/connect' method='POST'>
      <label for='ssid'>Wi-Fi名称</label>
      <input type='text' name='ssid' id='ssid' maxlength='32' required placeholder='如: MyRouter'>
      <label for='password'>Wi-Fi密码</label>
      <input type='password' name='password' id='password' maxlength='64' required placeholder='请输入密码'>
      <input type='submit' value='连接并保存'>
    </form>
  </div>
</body>
</html>
    )";


const char* html_connect = R"(
<!DOCTYPE html>
<html lang='zh-CN'>
<head>
  <meta charset='UTF-8'>
  <title>WiFi连接中</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    html,body {height:100%; margin:0;}
    body {
      display: flex; flex-direction: column; justify-content: center; align-items: center;
      min-height: 100vh; background: linear-gradient(135deg,#e3f0ff 40%,#f9fbfc 100%);
      font-family: "SF Pro","Helvetica Neue",Arial,sans-serif;
      margin:0;
    }
    .container {
      background: #fff;
      padding: 38px 30px 32px 30px;
      border-radius: 16px;
      box-shadow: 0 4px 32px rgba(37,102,193,0.09);
      min-width: 320px; max-width: 90vw;
      display: flex; flex-direction: column; align-items: center;
    }
    h2 {
      margin-bottom: 30px; font-weight: 600; font-size: 1.7em; color:#1c4379;
      letter-spacing: 0.5px;
    }
    .progress-bar-bg {
      width: 180px; height: 14px; background: #ebeff5; border-radius:9px;
      box-shadow:0 1px 4px rgba(37,102,193,0.11);
      margin-bottom: 12px; position: relative;
      overflow: hidden;
    }
    .progress-bar-fg {
      height: 100%; background: linear-gradient(90deg, #2566c1 20%, #35a0fc 95%);
      border-radius: 9px; transition: width 0.48s cubic-bezier(.62,.02,.2,1);
    }
    .count {
      font-size:17px; color:#2566c1;
      letter-spacing:0.5px;
      margin-bottom: 26px;
    }
    #status {
      margin-top:10px; font-size:18px; color:#333;
      min-height: 32px;
    }
    .message-success, .message-error {
      padding:18px 20px; border-radius:8px; font-size:18px; line-height:1.6;
      margin-top:24px; margin-bottom:12px;
    }
    .message-success {
      background: #edfff2; color: #197849; border: 1px solid #aaefce;
    }
    .message-error {
      background: #fff0ef; color: #ba2f20; border: 1px solid #f9b3b3;
    }
    .btn {
      padding:13px 34px; font-size:18px; border:none;
      background: linear-gradient(90deg,#2566c1 70%,#3a81ea 100%);
      color:#fff; border-radius:7px; cursor:pointer;
      box-shadow: 0 2px 10px rgba(37,102,193,0.07);
      font-weight:600; letter-spacing:.02em; margin-top:14px;
      transition: background 0.18s;
    }
    .btn:active, .btn:focus, .btn:hover {background: linear-gradient(90deg,#185fbb 80%,#3293ee 100%);}
    @media (max-width:500px){
      .container {padding:28px 7vw;} h2 {font-size:1.15em;}
      .progress-bar-bg {width:58vw;}
      .btn {padding:11px 10vw; font-size:16px;}
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>正在连接WiFi路由器…</h2>
    <div class="progress-bar-bg">
      <div class="progress-bar-fg" id="progressBar"></div>
    </div>
    <div class="count">剩余时间：<span id="timer">20</span>秒</div>
    <div id="status">请稍候…</div>
    <button id="btnRetry" class="btn" style="display:none;" onclick="location.href='/'">返回重新配置</button>
  </div>
<script>
  let totalSeconds = 20;//等待时间
  let seconds = totalSeconds;
  let finished = false;
  let successCountdown = 0;
  function updateTimer() {
    if (seconds > 0) {
      seconds--;
      document.getElementById('timer').innerText = seconds;
      // 进度条
      let progressPercent = Math.max(0, (seconds/totalSeconds)*100);
      document.getElementById('progressBar').style.width = progressPercent + '%';
      if(!finished) setTimeout(updateTimer, 1000);
    } else if (!finished) {
      location.href='/';
    }
  }
  document.getElementById('progressBar').style.width = '100%';
  updateTimer();

function showSuccessCountdown() {
  let tip = document.getElementById('successCountdownTip');
  if (!tip) return;
  if (successCountdown > 0) {
    tip.innerText = `即将退出页面 (${successCountdown}s)…`;
    successCountdown--;
    setTimeout(showSuccessCountdown, 1000);
  } else {
    // 直接覆盖body，提示用户手动关闭
    document.body.innerHTML = `
      <div style="height:100vh; display:flex; align-items:center; justify-content:center; flex-direction:column;">
        <h2 style="color:#2566c1; margin-bottom:12px;">WiFi连接成功 🎉</h2>
        <p style="font-size:18px; color:#4a4a4a;">您可以关闭本页面<br><span style="color:#888;font-size:14px;">（如页面未自动关闭，请手动关闭）</span></p>
      </div>`;
  }
}

  function checkStatus() {
    fetch('/status').then(r=>r.json()).then(data=>{
      if(data.status===1){
        finished = true;
        successCountdown = 3;   // 成功后倒计时5秒
        document.getElementById('status').innerHTML =
          `<div class="message-success">
            连接成功！<br>
            IP：${data.ip}<br>
            <span id="successCountdownTip" style="color:#28a745; font-weight:600;">即将退出页面 (5s)…</span>
          </div>`;
        document.getElementById('btnRetry').style.display = 'none';
        showSuccessCountdown();
      }else if(data.status===0){
        finished = true;
        document.getElementById('status').innerHTML =
          `<div class="message-error">连接失败，请返回重新配置</div>`;
        document.getElementById('btnRetry').style.display = '';
        setTimeout(()=>location.href='/', 3000);
      }else{
        setTimeout(checkStatus, 1200);
      }
    }).catch(r=>setTimeout(checkStatus,1200));
  }
  checkStatus();
</script>
</body>
</html>
)";