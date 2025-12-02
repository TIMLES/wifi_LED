# ws_udp_server.py （新建或原文件）
import asyncio
import socket
import websockets
import threading
UDP_IP = "0.0.0.0"
UDP_PORT = 4210
clients = set()

async def ws_handler(websocket):
    clients.add(websocket)
    try:
        async for message in websocket:
            pass  # not used
    finally:
        clients.remove(websocket)

# 数据包解析，不同类型分辨
def parse_packet(data):
        # 长度为16，判定为手柄
    if len(data) == 16:
        fields = [
            'lx','ly','rx','ry','lt','rt','dpad_up','dpad_down','dpad_left','dpad_right',
            'btn_a','btn_b','btn_x','btn_y','btn_lb','btn_rb'
        ]
        vals = list(data)
        return dict(zip(fields, vals))
    try:
        # 尝试按utf-8解码，属于状态字符串
        text = data.decode('utf-8', errors='ignore').strip()
        if text.startswith("ESP_FPS:"):
            fps_val = text.split(":",1)[-1]
            try: fps_val = int(fps_val)
            except: pass
            return {"type": "esp_status", "fps": fps_val}
        # 可扩展其它协议，如 ESP_STATUS:xxx
        # elif text.startswith("ESP_STATUS:"):
        #    ...
        return {"type": "unknown", "raw": text}
    except Exception as e:
        return {"type": "unknown", "error": str(e), "raw": repr(data)}

async def broadcast(msg):
    if not clients: return
    to_remove = set()
    for client in clients.copy():
        try:
            await client.send(msg)
        except Exception as e:
            print(f"[WS] 客户端断开了: {e}")
            to_remove.add(client)
    clients.difference_update(to_remove)

async def udp_loop(stop_event: threading.Event = None):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    print(f"[UDP] Listening {UDP_IP}:{UDP_PORT}")
    while True:
        if stop_event and stop_event.is_set():
            print("[UDP] 停止信号收到，退出 UDP loop")
            break
        data, addr = await asyncio.to_thread(sock.recvfrom, 1024)
        parsed = parse_packet(data)
        import json
        msg = json.dumps(parsed)
        await broadcast(msg)
    sock.close()

async def backend_main(stop_event: threading.Event = None):
    ws_server = await websockets.serve(ws_handler, "0.0.0.0", 5678)
    print("[WS] started at ws://localhost:5678")
    try:
        await udp_loop(stop_event)
    finally:
        print("[WS] 停止中...")
        ws_server.close()
        await ws_server.wait_closed()

def run_ws_udp_server(stop_event: threading.Event = None):
    # 必须在自己的线程中启动 event_loop
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    try:
        loop.run_until_complete(backend_main(stop_event))
    except Exception as e:
        print("[服务端] websocket/udp服务异常退出:", e)
    finally:
        loop.close()