# NUCLEO-F103RB

## DC Motor
모터 드라이버와 연결된 DC 모터를 구동한다.  
TIM2 CH1(PWM) + PC6/PC8 방향 제어 핀을 사용한다.

### Motor_Init
- PC6, PC8와 연결된 모터의 방향 핀을 초기화한다.  
- TIM2 CH1 PWM을 시작한다.
- 참고: [Motor gpio핀 연결](../../RCcar/STM_Motor/ST_Motor.md)

### Motor_SetDirection
모터의 회전 방향을 제어한다.

- `MOTOR_FORWARD` : 앞으로 전진  
- `MOTOR_BACKWARD` : 뒤로 후진  
- `MOTOR_STOP` : 양쪽 핀 LOW, PWM을 0%로 설정해 정지

### Motor_SetDuty (static)
- 실제 PWM duty(0~100%)를 설정하는 내부 함수.

### Motor_SetSpeedPercent
- 모터 속도를 퍼센트 단위(-100 - 100)로 설정한다.
- 하드웨어 특성상 63% 이하에서는 모터가 돌지 않으므로,  
  1-100%를 63-100% 구간으로 선형 매핑한다.
- 최종 duty는 `Motor_SetDuty()`를 통해 PWM에 반영된다.


<br>
<br>
<br>

## Servo Motor
조향(steering)을 담당하는 서보 모터를 제어한다.  
TIM3 CH1(PWM)을 사용한다.

### Servo_Init
- TIM3 CH1 PWM을 시작한다.
- 타이머 설정: 1us 해상도, Period = 19999 (20ms 주기)
  - 1ms - 2ms 펄스 → 서보 0° - 180°

### Servo_SetAngle
- 입력: `uint8_t angle` (0 ~ 180도)
- 0-180도를 1ms-2ms 펄스로 매핑:

### Servo_SetSteerPercent
- 입력: `int8_t steer_percent` (-100 - 100)
- -100 - 100 범위를 각도(0° - 150°)로 선형 매핑한다.
- 매핑:
  - `steer_percent = -100` → 약 0° (최좌측)
  - `steer_percent = 0`    → 75° (정중앙)
  - `steer_percent = 100`  → 약 150° (최우측)


<br>
<br>
<br>

## UART
USART2(UART2)를 활용해 **라즈베리파이 ↔ STM32** 간 양방향 통신을 구현했다.  
라즈베리파이에서 UART로 패킷을 보내면, STM32가 이를 파싱해 모터 및 서보를 제어한다.  

### 패킷 설정
예시:  
`PKT: AA 55 01 14 F6 01 6B`

각 바이트의 의미:

- [0] `header1 = 0xAA`  
- [1] `header2 = 0x55`  
- [2] `seq     = 0~255` (패킷 시퀀스 번호)  
- [3] `speed   = int8_t (-100 ~ 100)`  
- [4] `steer   = int8_t (-100 ~ 100)`  
- [5] `flags   = uint8_t (bit0: enable, bit1: emergency stop)`  
- [6] `checksum = (byte0 + ... + byte5) & 0xFF`

#### 헤더 (0, 1번 바이트)
- `0xAA = 1010 1010`  
- `0x55 = 0101 0101`  
서로 비트 패턴이 완전히 반대라서,  
랜덤 노이즈가 우연히 만들어낼 확률이 낮고,  
프레임의 시작을 찾는 동기화(synchronization) 용도로 사용한다.

#### seq (2번 바이트)
- 라즈베리파이가 패킷을 보낼 때마다 `0,1,2,3…` 순서로 증가시키는 값.
- `Control_Task()`에서는 이전에 처리한 seq와 비교해,
  새로운 패킷이 들어왔을 때만 로그를 남기도록 사용한다.

#### speed / steer (3, 4번 바이트)
- `int8_t` 타입으로 -100 ~ 100 범위를 사용.
- `speed`:
  - 0 : 정지  
  - 양수 : 전진, 크기가 클수록 빠르게  
  - 음수 : 후진, 절대값이 클수록 빠르게
- `steer`:
  - 0 : 직진  
  - 음수 : 좌회전, 절대값이 클수록 더 꺾임  
  - 양수 : 우회전, 절대값이 클수록 더 꺾임

#### flags (5번 바이트)
- 향후 확장을 위한 플래그 비트 집합.
- 현재 사용:
  - `bit0 (0x01)`: enable
    - 0 → 비활성화 (정지 상태 유지)  
    - 1 → 명령 유효 (정상 주행 허용)
  - `bit1 (0x02)`: emergency stop
    - 1 → 긴급 정지 요청 (속도 0으로 강제)

#### checksum (6번 바이트)
- 전송 중 오류 검출용.
- STM32에서 계산한 값과 수신된 checksum이 다르면,
  해당 패킷은 잘못된 데이터로 판단하고 무시한다.

<br>
<br>

### STM 코드 설명
#### Uart_App_Init
- 모터/서보 초기 상태 설정:
  - `Motor_SetDirection(MOTOR_STOP);`  
  - `Servo_SetAngle(75);` (센터)

#### Uart_ParseByte (static)
- 1바이트씩 들어오는 UART 데이터를 받아 패킷 단위로 조립한다.

#### Uart_App_Task
- 메인 루프에서 반복 호출되는 UART 수신 처리 함수.
- `HAL_UART_Receive(&huart2, &ch, 1, 10)` 으로 1바이트 수신 시도.
  - 성공하면 `Uart_ParseByte(ch)` 호출로 패킷 파싱.

#### Control_Task
- 메인 루프에서 반복 호출되는 제어 로직.
- DC 모터/서보에 실제 명령을 적용하고, 필요 시 로그를 출력한다.

동작 순서:

1. **타임아웃 체크 (500ms)**
   - `now - last_cmd_tick > 500` 이면:
     - `Motor_SetSpeedPercent(0);` (정지)  
     - `Servo_SetSteerPercent(0);` (직진)  
     - 리턴
   - → 0.5초 동안 명령이 없으면 자동으로 차를 멈춘다.

2. **enable 플래그 확인**
   - `if ((g_cmd.flags & 0x01) == 0)` 이면:
     - 모터 속도를 0으로 유지하고 리턴
   - → 라즈베리파이가 의도적으로 차를 비활성화 할 수 있다.

3. **emergency stop 플래그 확인**
   - `if (g_cmd.flags & 0x02)` 이면:
     - 즉시 속도를 0으로 만들고 리턴
   - → 급정지(비상 정지) 시 사용.

4. **정상 주행 제어**
   - `Motor_SetSpeedPercent(g_cmd.speed);`
   - `Servo_SetSteerPercent(g_cmd.steer);`

5. **명령 로그 출력 ([CMD])**
   - `g_cmd.seq`가 이전에 처리한 `last_logged_seq`와 다를 때만:
   - `[CMD] seq=... speed=... steer=... flags=...` 로그 출력
   - → 같은 명령이 루프마다 반복 출력되는 것을 방지.

---

<br>
<br>

# Raspberry-pi
라즈베리파이와 SMT보드의 통신을 위해,  
라즈베리파이에서 UART 프로그램을 작성 후 빌드한다.  

```
./UART/
      ├─ main.cpp
      ├─ uart.cpp
      └─ uart.hpp
```

단, main.cpp의 `dev = "/dev/ttyACM0"` 의 시리얼 포트를 꼭 수정하도록 한다.  

### Build
```
g++ -std=c++17 main.cpp uart.cpp -o uart
```

### 실행
```
./uart
```
프로그램을 실행하면 라즈베리파이에서 STM32 보드로 패킷을 전송하고 받을 수 있다.  
`Ex) 20 0 1`
- speed (uint8_t)
- steer (uint8_t)
- flags (uint8_t, bit0: enable, bit1: emergency stop)  


STM32 보드는 수신한 패킷을 그대로 재전송(echo)하여, 패킷이 정상적으로 전송·수신되고 있는지 확인할 수 있다.  

logs/YYMMDD_HHMM.txt 형식의 파일로  
터미널에 출력되는 STM32 로그가 그대로 저장된다.  