#ifndef __TEST_PKT_ECHO_H__
#define __TEST_PKT_ECHO_H__

#if defined(MODE_PKT_ECHO)

/**
 * @brief 패킷 에코 테스트 초기화
 */
void Test_PktEcho_Init(void);

/**
 * @brief 패킷 에코 테스트 루프 — main() while(1) 에서 호출
 *
 * - RPi로부터 패킷 수신 시 USART2(printf)로 내용 출력
 * - 모터 명령은 정상 실행됨
 * - 500ms 주기: 수신 통계 출력
 */
void Test_PktEcho_Loop(void);

#endif /* MODE_PKT_ECHO */

#endif /* __TEST_PKT_ECHO_H__ */
