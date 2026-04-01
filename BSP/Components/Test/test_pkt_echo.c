#if defined(MODE_PKT_ECHO)

#include "test_pkt_echo.h"
#include "app_packet_parser.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

void Test_PktEcho_Init(void)
{
    printf("=== MODE: PKT_ECHO ===\r\n");
    printf("RPi -> STM32 패킷 수신 시 내용을 시리얼 모니터로 출력합니다.\r\n");
    printf("모터 명령은 정상 실행됩니다.\r\n");
    printf("=====================\r\n");
}

static void print_packet(const Packet_t *pkt)
{
    switch (pkt->cmd) {
    case CMD_VELOCITY: {
        int16_t vL = (int16_t)(pkt->data[0] | (pkt->data[1] << 8));
        int16_t vR = (int16_t)(pkt->data[2] | (pkt->data[3] << 8));
        printf("[PKT] CMD=0x01 (VELOCITY)  vL=%+d  vR=%+d mm/s\r\n", vL, vR);
        break;
    }
    case CMD_STOP:
        printf("[PKT] CMD=0x02 (STOP)\r\n");
        break;
    case CMD_ESTOP:
        printf("[PKT] CMD=0x03 (ESTOP)\r\n");
        break;
    case CMD_RELEASE:
        printf("[PKT] CMD=0x04 (RELEASE)\r\n");
        break;
    default: {
        printf("[PKT] CMD=0x%02X (UNKNOWN)  LEN=%d  DATA:", pkt->cmd, pkt->len);
        for (uint8_t i = 0; i < pkt->len; i++) {
            printf(" %02X", pkt->data[i]);
        }
        printf("\r\n");
        break;
    }
    }
}

void Test_PktEcho_Loop(void)
{
    if (Protocol_Process()) {
        print_packet(Protocol_GetLastPacket());
    }

    Motor_CheckTimeout();

    // /* 500ms 주기: 수신 통계 출력 */
    // static uint32_t last_stat = 0;
    // uint32_t now = HAL_GetTick();
    // if (now - last_stat >= 500) {
    //     last_stat = now;
    //     const ProtoStats_t *st = Protocol_GetStats();
    //     printf("[STAT] rx_pkt:%lu  chk_err:%lu  inv_len:%lu  unk_cmd:%lu  uart_err:%lu\r\n",
    //            st->rx_packets, st->rx_checksum_errors,
    //            st->rx_invalid_len, st->rx_unknown_cmd,
    //            Protocol_GetErrorCount());
    // }
}

#endif /* MODE_PKT_ECHO */
