import time

class FPSLimiter:
    """
    FPS限制器：用于让你的主循环以指定的帧率(FPS)运行。
    调用wait(fps)，可精确控制循环速率。支持高精度（忙等）与低速（sleep）两种模式。
    """

    def __init__(self):
        # 初始化记录第一帧的时间
        self.last_time = time.perf_counter()
    
    def wait(self, fps):
        """
        等待直到下一帧，应在主循环每一帧结尾调用，参数fps为目标帧率。
        实现思路：累加每帧“应到时间”，确保即使某帧处理慢，下一帧也不跑飞，整体节奏不漂移。
        
        参数：
            fps -- 目标帧率，如 60、120、240等
        """
        frame_time = 1.0 / fps    # 每帧持续时间（秒）
        next_time = self.last_time + frame_time  # 本帧理论目标时间戳
        now = time.perf_counter()
        remain = next_time - now  # 距离下一帧应到达的时间，还剩多少秒

        if fps < 61:
            # FPS较低时直接sleep，避免浪费CPU资源
            if remain > 0:
                time.sleep(remain)
            self.last_time = next_time  # 更新时间戳，保证节拍对齐
            return
        
        # FPS较高时，先大部分sleep（节省CPU），最后0.5ms忙等定位
        if remain > 0.001:
            time.sleep(remain - 0.0005)
        # 最后0.5ms用忙等，保证高帧率精度
        while time.perf_counter() < next_time:
            pass
        self.last_time = next_time  # 更新时间戳，进入下一个节拍
