# MQTT Interface Spec (ROS / RPi / GUI)

> 기본 토픽 prefix: `robot/*`  
> JSON 인코딩 (UTF-8)  

---

## 1) MAP state — `robot/map_state`  (ROS → GUI)

~~~json
{
  "map_id": "2025-12-08-001",

  "width_cells": 800,
  "height_cells": 600,
  "resolution": 0.05,

  "origin_x": -10.0,
  "origin_y": -5.0,
  "origin_yaw": 0.0,

  "timestamp": "2025-12-08T12:34:56Z"
}
~~~

- `map_id` (string): 맵 버전
- `width_cells`, `height_cells` (int): grid 크기
- `resolution` (double): meters/cell
- `origin_x`, `origin_y`, `origin_yaw` (double): map 좌표계 원점
- `timestamp` (string): ISO-8601 (UTC 권장)

---

## 2) 모터 속도 및 조향 — `robot/motor_cmd`  (ROS → GUI)  
GUI에서 속도 게이지/로그에 그대로 씀 (speed 0~100, steer 0~180)

~~~json
{
  "source": "auto_planner",
  "speed": 60,
  "steer": 140,
  "enable": true,
  "emergency_stop": false,

  "raw": {
    "speed_byte": 60,
    "steer_byte": 140,
    "flags_byte": 1
  },

  "mode": "auto",
  "timestamp": "2025-12-08T12:40:00.123Z"
}
~~~

- `speed` (int): 0~100
- `steer` (int): 0~180 (90이 직진 기준)
- `enable` (bool), `emergency_stop` (bool)
- `raw` (object, optional): UART로 내려가는 실제 바이트 값 기록용
- `mode` (string): `"manual" | "auto" | "remote" ...`
- `source` (string): 발행 주체

---

## 3) 카메라 이미지 — `robot/camera_shot`  (RPi → GUI)

~~~json
{
  "shot_id": "shot_0001",

  "x": 2.5,
  "y": 0.3,
  "theta": -1.57,

  "image_url": "https://example.com/images/shot_0001.jpg",

  "timestamp": "2025-12-08T12:36:10.456Z"
}
~~~

- `image_url` (string): 원본(또는 프레임) 이미지 경로
- `(x,y,theta)`는 shot이 찍힌 위치(지도 기준) 메타

---

## 4) 카메라 제어(촬영 시작/중지) — `robot/camera_cmd`  (GUI → RPi)
GUI에서 publish하는 토픽
~~~json
{
  "source": "qt_gui",
  "action": "start",
  "period_ms": 1000,
  "timestamp": "2025-12-22T12:00:00Z"
}
~~~

- `action`: `"start"` 또는 `"stop"`
- `period_ms`: start일 때 촬영 주기(ms)