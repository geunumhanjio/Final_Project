#ifndef RPI_SERIAL_BRIDGE__SERIAL_PROTOCOL_HPP_
#define RPI_SERIAL_BRIDGE__SERIAL_PROTOCOL_HPP_

#include <cstdint>
#include <vector>
#include <functional>
#include <string>

namespace rpi_serial_bridge
{

/**
 * @brief STM32 시리얼 통신 프로토콜 처리 클래스
 * 
 * 패킷 형식: [0xFF][0xFE][CMD][LEN][DATA...][CHECKSUM]
 */
class SerialProtocol
{
public:
  // 패킷 명령 코드
  static constexpr uint8_t CMD_VELOCITY = 0x01;         // 속도 명령
  static constexpr uint8_t CMD_STOP = 0x02;             // 정지
  static constexpr uint8_t CMD_EMERGENCY_STOP = 0x03;   // 비상 정지
  static constexpr uint8_t CMD_EMERGENCY_RELEASE = 0x04; // 비상 정지 해제
  
  static constexpr uint8_t RSP_ODOM = 0x81;             // 오도메트리 응답
  static constexpr uint8_t RSP_IMU = 0x82;              // IMU 응답

  // 패킷 헤더
  static constexpr uint8_t HEADER1 = 0xFF;
  static constexpr uint8_t HEADER2 = 0xFE;

  SerialProtocol();
  ~SerialProtocol() = default;

  /**
   * @brief 속도 명령 패킷 생성
   * @param left_mm_s 좌측 바퀴 속도 (mm/s)
   * @param right_mm_s 우측 바퀴 속도 (mm/s)
   * @return 전송할 패킷 데이터
   */
  std::vector<uint8_t> createVelocityPacket(int16_t left_mm_s, int16_t right_mm_s);

  /**
   * @brief 정지 명령 패킷 생성
   */
  std::vector<uint8_t> createStopPacket();

  /**
   * @brief 비상 정지 명령 패킷 생성
   */
  std::vector<uint8_t> createEmergencyStopPacket();

  /**
   * @brief 비상 정지 해제 명령 패킷 생성
   */
  std::vector<uint8_t> createEmergencyReleasePacket();

  /**
   * @brief 수신 바이트 처리 (상태 머신)
   * @param byte 수신한 바이트
   */
  void processByte(uint8_t byte);

  /**
   * @brief ODOM 데이터 콜백 등록
   */
  void setOdomCallback(std::function<void(int16_t, int16_t)> callback) {
    odom_callback_ = callback;
  }

  /**
   * @brief IMU 데이터 콜백 등록
   */
  void setImuCallback(std::function<void(const int16_t[6])> callback) {
    imu_callback_ = callback;
  }

  /**
   * @brief 통계 정보 반환
   */
  struct Stats {
    uint32_t packets_received = 0;
    uint32_t checksum_errors = 0;
    uint32_t unknown_commands = 0;
  };
  Stats getStats() const { return stats_; }

private:
  // 파서 상태
  enum class State {
    WAIT_HEADER1,
    WAIT_HEADER2,
    READ_CMD,
    READ_LENGTH,
    READ_DATA,
    READ_CHECKSUM
  };

  State state_;
  uint8_t cmd_;
  uint8_t length_;
  std::vector<uint8_t> data_buffer_;
  uint8_t checksum_;

  // 콜백
  std::function<void(int16_t, int16_t)> odom_callback_;
  std::function<void(const int16_t[6])> imu_callback_;

  // 통계
  Stats stats_;

  /**
   * @brief 패킷 생성 헬퍼
   */
  std::vector<uint8_t> createPacket(uint8_t cmd, const std::vector<uint8_t>& data);

  /**
   * @brief 체크섬 계산
   */
  uint8_t calculateChecksum(uint8_t cmd, uint8_t length, const std::vector<uint8_t>& data);

  /**
   * @brief 완성된 패킷 처리
   */
  void handlePacket();

  /**
   * @brief ODOM 패킷 파싱
   */
  void handleOdomPacket();

  /**
   * @brief IMU 패킷 파싱
   */
  void handleImuPacket();
};

}  // namespace rpi_serial_bridge

#endif  // RPI_SERIAL_BRIDGE__SERIAL_PROTOCOL_HPP_
