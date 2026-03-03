#include "app_packet_parser.h"
#include "drv_motor.h"
#include <stdio.h>
#include <string.h>

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *proto_huart;
static uint8_t rx_byte;

/* State machine */
static volatile ParseState_t parse_state = PARSE_WAIT_HEADER1;
static Packet_t rx_packet;
static uint8_t data_index;
static uint8_t running_checksum;

/* Double-buffer: ISR writes, main loop reads */
static volatile bool packet_ready = false;
static Packet_t ready_packet;

/* Statistics */
static ProtoStats_t stats;

/* UART error tracking */
static volatile uint32_t uart_error_count = 0;

/* Private function prototypes -----------------------------------------------*/
static void Protocol_Dispatch(const Packet_t *pkt);
static void Protocol_HandleVelocity(const Packet_t *pkt);
static void Protocol_SendPacket(uint8_t cmd, const uint8_t *data, uint8_t len);

/* Exported functions --------------------------------------------------------*/

void Protocol_Init(UART_HandleTypeDef *huart)
{
    proto_huart = huart;

    parse_state = PARSE_WAIT_HEADER1;
    packet_ready = false;
    memset(&stats, 0, sizeof(stats));

    /* Start receiving first byte via interrupt */
    HAL_UART_Receive_IT(proto_huart, &rx_byte, 1);
}

void Protocol_FeedByte(uint8_t byte)
{
    switch (parse_state) {

    case PARSE_WAIT_HEADER1:
        if (byte == PROTO_HEADER1) {
            parse_state = PARSE_WAIT_HEADER2;
        }
        break;

    case PARSE_WAIT_HEADER2:
        if (byte == PROTO_HEADER2) {
            parse_state = PARSE_WAIT_CMD;
        } else if (byte == PROTO_HEADER1) {
            /* Stay: might be start of a new header */
        } else {
            parse_state = PARSE_WAIT_HEADER1;
        }
        break;

    case PARSE_WAIT_CMD:
        rx_packet.cmd = byte;
        running_checksum = byte;
        parse_state = PARSE_WAIT_LEN;
        break;

    case PARSE_WAIT_LEN:
        rx_packet.len = byte;
        running_checksum ^= byte;
        data_index = 0;

        if (byte > PROTO_MAX_DATA_LEN) {
            stats.rx_invalid_len++;
            parse_state = PARSE_WAIT_HEADER1;
        } else if (byte == 0) {
            parse_state = PARSE_WAIT_CHECKSUM;
        } else {
            parse_state = PARSE_WAIT_DATA;
        }
        break;

    case PARSE_WAIT_DATA:
        rx_packet.data[data_index++] = byte;
        running_checksum ^= byte;

        if (data_index >= rx_packet.len) {
            parse_state = PARSE_WAIT_CHECKSUM;
        }
        break;

    case PARSE_WAIT_CHECKSUM:
        rx_packet.checksum = byte;

        if (running_checksum == byte) {
            /* Valid packet: copy to ready buffer */
            if (!packet_ready) {
                memcpy((void *)&ready_packet, &rx_packet, sizeof(Packet_t));
                packet_ready = true;
            }
            stats.rx_packets++;
        } else {
            stats.rx_checksum_errors++;
        }
        parse_state = PARSE_WAIT_HEADER1;
        break;
    }
}

bool Protocol_Process(void)
{
    if (!packet_ready) {
        return false;
    }

    Packet_t pkt;
    memcpy(&pkt, (const void *)&ready_packet, sizeof(Packet_t));
    packet_ready = false;

    Protocol_Dispatch(&pkt);
    return true;
}

const ProtoStats_t* Protocol_GetStats(void)
{
    return &stats;
}

/* UART RX complete callback (called from HAL ISR) --------------------------*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == proto_huart->Instance) {
        Protocol_FeedByte(rx_byte);
        /* Re-arm for next byte */
        HAL_UART_Receive_IT(proto_huart, &rx_byte, 1);
    }
}

/* UART Error callback — re-arm RX so the interrupt chain doesn't break -----*/
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == proto_huart->Instance) {
        uart_error_count++;
        /* Clear error flags and re-arm RX interrupt */
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        HAL_UART_Receive_IT(proto_huart, &rx_byte, 1);
    }
}

uint32_t Protocol_GetErrorCount(void)
{
    return uart_error_count;
}

/* Private functions ---------------------------------------------------------*/

static void Protocol_Dispatch(const Packet_t *pkt)
{
#ifdef DEBUG_PACKET
    printf("[PKT] CMD=0x%02X LEN=%d", pkt->cmd, pkt->len);
    for (uint8_t i = 0; i < pkt->len; i++) {
        printf(" D[%d]=0x%02X", i, pkt->data[i]);
    }
    printf("\r\n");
#endif

    switch (pkt->cmd) {

    case CMD_VELOCITY:
        Protocol_HandleVelocity(pkt);
        break;

    case CMD_STOP:
        Motor_SoftStop();
        break;

    case CMD_ESTOP:
        Motor_EmergencyStop();
        break;

    case CMD_RELEASE:
        Motor_ReleaseEmergency();
        break;

    default:
        stats.rx_unknown_cmd++;
        break;
    }
}

static void Protocol_HandleVelocity(const Packet_t *pkt)
{
    if (pkt->len != 4) {
        stats.rx_invalid_len++;
        return;
    }

    /* Little-Endian: [vL_low, vL_high, vR_low, vR_high] */
    int16_t v_left  = (int16_t)(pkt->data[0] | (pkt->data[1] << 8));
    int16_t v_right = (int16_t)(pkt->data[2] | (pkt->data[3] << 8));

    Motor_SetVelocity(v_left, v_right);
}

static void Protocol_SendPacket(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t buf[PROTO_MAX_PACKET_SIZE];
    uint8_t idx = 0;

    buf[idx++] = PROTO_HEADER1;
    buf[idx++] = PROTO_HEADER2;
    buf[idx++] = cmd;
    buf[idx++] = len;

    uint8_t checksum = cmd ^ len;
    for (uint8_t i = 0; i < len; i++) {
        buf[idx++] = data[i];
        checksum ^= data[i];
    }
    buf[idx++] = checksum;

    HAL_UART_Transmit(proto_huart, buf, idx, 10);
}

void Protocol_SendOdom(int16_t left_ticks, int16_t right_ticks)
{
    uint8_t data[4];
    data[0] = (uint8_t)(left_ticks & 0xFF);
    data[1] = (uint8_t)((left_ticks >> 8) & 0xFF);
    data[2] = (uint8_t)(right_ticks & 0xFF);
    data[3] = (uint8_t)((right_ticks >> 8) & 0xFF);

    Protocol_SendPacket(RSP_ODOM, data, 4);
}

void Protocol_SendIMU(const MPU6050_Data_t *imu)
{
    uint8_t data[12];
    const int16_t *raw = (const int16_t *)imu;

    for (uint8_t i = 0; i < 6; i++) {
        data[i * 2]     = (uint8_t)(raw[i] & 0xFF);
        data[i * 2 + 1] = (uint8_t)((raw[i] >> 8) & 0xFF);
    }

    Protocol_SendPacket(RSP_IMU, data, 12);
}
