/*
 * uart_app.c
 *
 *  Created on: Nov 27, 2025
 *      Author: SSAFY
 */

#include "uart_app.h"
#include "usart.h"
#include "motor.h"
#include "servo.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

extern UART_HandleTypeDef huart2;

/*-------------------- 명령 구조체 --------------------*/

typedef struct {
    int8_t  speed;   // -100 ~ 100
    int8_t  steer;   // -100 ~ 100
    uint8_t flags;   // bit0: enable, bit1: estop
    uint8_t seq;     // sequence number
} DriveCmd;

volatile DriveCmd g_cmd;
volatile uint32_t last_cmd_tick = 0;

/*-------------------- 공용 UART printf --------------------*/

void Uart_Print(const char *fmt, ...)
{
    char buf[64];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n > 0) {
        HAL_UART_Transmit(&huart2, (uint8_t*)buf, (uint16_t)n, 100);
    }
}

/*-------------------- 초기화 --------------------*/

void Uart_App_Init(void)
{
    const char *start_msg = "\r\n[STM] RC Car UART Ready\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t*)start_msg, strlen(start_msg), 100);

    Motor_SetDirection(MOTOR_STOP);
    Servo_SetAngle(75);

    g_cmd.speed = 0;
    g_cmd.steer = 0;
    g_cmd.flags = 0;
    g_cmd.seq   = 0;

    last_cmd_tick = HAL_GetTick();
}

/*-------------------- 패킷 파서 --------------------*/
/*
   [0] 0xAA
   [1] 0x55
   [2] seq     (0~255)
   [3] speed   (int8_t, -100~100)
   [4] steer   (int8_t, -100~100)
   [5] flags   (bit0: enable, bit1: estop)
   [6] checksum = (byte0 + ... + byte5) & 0xFF
*/

static void Uart_ParseByte(uint8_t byte)
{
    static uint8_t buf[7];
    static uint8_t idx = 0;

    // 헤더 1바이트 동기화 (0xAA)
    if (idx == 0) {
        if (byte != 0xAA) {
            return;
        }
        buf[idx++] = byte;
        return;
    }

    // 헤더 2바이트 동기화 (0x55)
    if (idx == 1) {
        if (byte != 0x55) {
            idx = 0;
            return;
        }
        buf[idx++] = byte;
        return;
    }

    buf[idx++] = byte;

    if (idx < 7) {
        return;
    }

    // 체크섬 검사
    uint8_t checksum = 0;
    for (int i = 0; i < 6; i++) {
        checksum += buf[i];
    }
    checksum &= 0xFF;

    if (checksum == buf[6]) {
        // g_cmd 갱신
        g_cmd.seq   = buf[2];
        g_cmd.speed = (int8_t)buf[3];
        g_cmd.steer = (int8_t)buf[4];
        g_cmd.flags = buf[5];

        last_cmd_tick = HAL_GetTick();

        // 패킷 내용 로그
        Uart_Print("[PKT] %02X %02X %02X %02X %02X %02X %02X\r\n",
                   buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);
    } else {
        Uart_Print("[PKT ERR] checksum mismatch: got=%02X\r\n", buf[6]);
    }

    idx = 0;
}

/*-------------------- UART 수신 태스크 --------------------*/

void Uart_App_Task(void)
{
    uint8_t ch;

    // 1바이트씩 non-blocking 비슷하게 받기 (타임아웃 10ms)
    if (HAL_UART_Receive(&huart2, &ch, 1, 10) == HAL_OK) {
        Uart_ParseByte(ch);
    }
}

/*-------------------- 제어 태스크 --------------------*/

void Control_Task(void)
{
    static uint8_t last_logged_seq = 0xFF;

    // enable 플래그 꺼져 있으면 정지 유지
    if ((g_cmd.flags & 0x01) == 0) {
        Motor_SetSpeedPercent(0);
        return;
    }

    // emergency stop
    if (g_cmd.flags & 0x02) {
        Motor_SetSpeedPercent(0);
        // 필요하다면 여기서 E-STOP 상태머신으로 진입 가능
        return;
    }

    // 정상 주행
    Motor_SetSpeedPercent(g_cmd.speed);
    Servo_SetSteerPercent(g_cmd.steer);

    // 새 패킷(seq가 바뀌었을 때)만 [CMD] 로그 한 번 찍기
    if (g_cmd.seq != last_logged_seq) {
        last_logged_seq = g_cmd.seq;

        Uart_Print("[CMD] seq=%u speed=%d steer=%d flags=0x%02X\r\n",
                   (unsigned)g_cmd.seq,
                   (int)g_cmd.speed,
                   (int)g_cmd.steer,
                   (unsigned)g_cmd.flags);
    }
}
