#include "rpi_serial_bridge/serial_protocol.hpp"
#include <cstring>

namespace rpi_serial_bridge
{

SerialProtocol::SerialProtocol()
: state_(State::WAIT_HEADER1),
  cmd_(0),
  length_(0),
  checksum_(0)
{
  data_buffer_.reserve(16);
}

std::vector<uint8_t> SerialProtocol::createVelocityPacket(int16_t left_mm_s, int16_t right_mm_s)
{
  std::vector<uint8_t> data(4);
  // Little-endian
  data[0] = left_mm_s & 0xFF;
  data[1] = (left_mm_s >> 8) & 0xFF;
  data[2] = right_mm_s & 0xFF;
  data[3] = (right_mm_s >> 8) & 0xFF;
  
  return createPacket(CMD_VELOCITY, data);
}

std::vector<uint8_t> SerialProtocol::createStopPacket()
{
  return createPacket(CMD_STOP, {});
}

std::vector<uint8_t> SerialProtocol::createEmergencyStopPacket()
{
  return createPacket(CMD_EMERGENCY_STOP, {});
}

std::vector<uint8_t> SerialProtocol::createEmergencyReleasePacket()
{
  return createPacket(CMD_EMERGENCY_RELEASE, {});
}

std::vector<uint8_t> SerialProtocol::createPacket(uint8_t cmd, const std::vector<uint8_t>& data)
{
  std::vector<uint8_t> packet;
  uint8_t length = static_cast<uint8_t>(data.size());
  uint8_t checksum = calculateChecksum(cmd, length, data);

  packet.push_back(HEADER1);
  packet.push_back(HEADER2);
  packet.push_back(cmd);
  packet.push_back(length);
  packet.insert(packet.end(), data.begin(), data.end());
  packet.push_back(checksum);

  return packet;
}

uint8_t SerialProtocol::calculateChecksum(uint8_t cmd, uint8_t length, const std::vector<uint8_t>& data)
{
  uint8_t checksum = cmd ^ length;
  for (uint8_t byte : data) {
    checksum ^= byte;
  }
  return checksum;
}

void SerialProtocol::processByte(uint8_t byte)
{
  switch (state_) {
    case State::WAIT_HEADER1:
      if (byte == HEADER1) {
        state_ = State::WAIT_HEADER2;
      }
      break;

    case State::WAIT_HEADER2:
      if (byte == HEADER2) {
        state_ = State::READ_CMD;
      } else if (byte == HEADER1) {
        // Stay in WAIT_HEADER2 (could be start of new packet)
        state_ = State::WAIT_HEADER2;
      } else {
        state_ = State::WAIT_HEADER1;
      }
      break;

    case State::READ_CMD:
      cmd_ = byte;
      checksum_ = byte;
      state_ = State::READ_LENGTH;
      break;

    case State::READ_LENGTH:
      length_ = byte;
      checksum_ ^= byte;
      data_buffer_.clear();
      
      if (length_ > 16) {  // 최대 길이 제한
        state_ = State::WAIT_HEADER1;
      } else if (length_ == 0) {
        state_ = State::READ_CHECKSUM;
      } else {
        state_ = State::READ_DATA;
      }
      break;

    case State::READ_DATA:
      data_buffer_.push_back(byte);
      checksum_ ^= byte;
      
      if (data_buffer_.size() >= length_) {
        state_ = State::READ_CHECKSUM;
      }
      break;

    case State::READ_CHECKSUM:
      stats_.packets_received++;
      
      if (checksum_ == byte) {
        handlePacket();
      } else {
        stats_.checksum_errors++;
      }
      
      state_ = State::WAIT_HEADER1;
      break;
  }
}

void SerialProtocol::handlePacket()
{
  switch (cmd_) {
    case RSP_ODOM:
      handleOdomPacket();
      break;
    
    case RSP_IMU:
      handleImuPacket();
      break;
    
    default:
      stats_.unknown_commands++;
      break;
  }
}

void SerialProtocol::handleOdomPacket()
{
  if (data_buffer_.size() != 4) {
    return;
  }

  // Little-endian int16_t
  int16_t left = static_cast<int16_t>(data_buffer_[0] | (data_buffer_[1] << 8));
  int16_t right = static_cast<int16_t>(data_buffer_[2] | (data_buffer_[3] << 8));

  if (odom_callback_) {
    odom_callback_(left, right);
  }
}

void SerialProtocol::handleImuPacket()
{
  if (data_buffer_.size() != 12) {
    return;
  }

  // 6개의 int16_t 값 (accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z)
  int16_t imu_data[6];
  for (int i = 0; i < 6; i++) {
    imu_data[i] = static_cast<int16_t>(
      data_buffer_[i * 2] | (data_buffer_[i * 2 + 1] << 8)
    );
  }

  if (imu_callback_) {
    imu_callback_(imu_data);
  }
}

}  // namespace rpi_serial_bridge
