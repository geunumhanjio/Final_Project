#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <atomic>
#include <chrono>

class EISNode : public rclcpp::Node
{
public:
    EISNode() 
    : Node("eis_node"), 
      current_pitch_(0.0), 
      current_roll_(0.0), 
      first_imu_(true)
    {
        // 1. IMU Subscriber
        imu_sub_ = this->create_subscription<sensor_msgs::msg::Imu>(
            "/imu/data", rclcpp::SensorDataQoS(),
            std::bind(&EISNode::imu_callback, this, std::placeholders::_1));

        // 2. Camera Subscriber
        image_sub_ = this->create_subscription<sensor_msgs::msg::CompressedImage>(
            "/camera/image_raw/compressed", 10,
            std::bind(&EISNode::image_callback, this, std::placeholders::_1));

        // 3. Stabilized Camera Publisher
        image_pub_ = this->create_publisher<sensor_msgs::msg::CompressedImage>(
            "/camera/image_eis/compressed", 10);

        RCLCPP_INFO(this->get_logger(), "🚀 Standalone EIS Node Started!");
        RCLCPP_INFO(this->get_logger(), "   - Subscribing to: /camera/image_raw/compressed & /imu/data");
        RCLCPP_INFO(this->get_logger(), "   - Publishing to: /camera/image_eis/compressed");
    }

private:
    std::atomic<double> current_pitch_;
    std::atomic<double> current_roll_;
    rclcpp::Time last_imu_time_;
    bool first_imu_;

    // ── IMU 콜백: 각속도 누적 (Yaw 무시) ──────────────────────────────────
    void imu_callback(const sensor_msgs::msg::Imu::SharedPtr msg)
    {
        rclcpp::Time now = rclcpp::Time(msg->header.stamp);
        if (first_imu_) {
            last_imu_time_ = now;
            first_imu_ = false;
            return;
        }

        double dt = (now - last_imu_time_).seconds();
        last_imu_time_ = now;

        if (dt <= 0.0 || dt > 1.0) return; // 비정상적인 시간 간격 무시

        // IMU 축 매핑 (일반적인 REP-103 기준)
        // Roll(X축회전): msg->angular_velocity.x
        // Pitch(Y축회전): msg->angular_velocity.y
        // Yaw(Z축회전): msg->angular_velocity.z (의도적 무시)
        double roll_vel = msg->angular_velocity.x;
        double pitch_vel = msg->angular_velocity.y;

        // [신규] 데드존 (Deadzone) 필터링
        // IMU는 가만히 있어도 미세한 센서 노이즈(Bias)가 계속 출력됩니다.
        // 이 미세 노이즈가 누적되었다가 Decay로 깎이는 과정이 반복되면 
        // 카메라가 가만히 있는데도 화면이 스스로 스멀스멀 움직이는 '인공 꿀렁임'이 발생합니다.
        // 초당 약 1.1도(0.02 rad/s) 이하의 미세한 회전각속도는 노이즈로 간주하고 0으로 리셋합니다.
        const double deadzone = 0.02; 
        if (std::abs(roll_vel) < deadzone) roll_vel = 0.0;
        if (std::abs(pitch_vel) < deadzone) pitch_vel = 0.0;

        // 각도 누적 (라디안)
        double new_roll = current_roll_.load() + (roll_vel * dt);
        double new_pitch = current_pitch_.load() + (pitch_vel * dt);

        // [핵심] Decay 필터 적용 (High-pass filter 역할)
        // 누적된 각도를 매 프레임 0으로 천천히 끌어당김.
        // 기존 0.99는 짐벌처럼 뷰를 꽉 잡고 있어서 오히려 회전/주행 시 화면이 멍하거나 어지러울 수 있음.
        // 0.85로 확 낮추면 자동차 '서스펜션(쇼바)'처럼 짧고 강한 진동(잔떨림)만 먹고 아주 빠르게 원상복구됨.
        const double decay_factor = 0.85;
        new_roll *= decay_factor;
        new_pitch *= decay_factor;

        current_roll_.store(new_roll);
        current_pitch_.store(new_pitch);
    }

    // ── 이미지 콜백: Affine 변환(warpAffine)으로 흔들림 보정 ─────────────
    void image_callback(const sensor_msgs::msg::CompressedImage::SharedPtr msg)
    {
        auto t_start = std::chrono::high_resolution_clock::now();

        try {
            // 1. JPEG 디코딩
            std::vector<uint8_t> img_bytes(msg->data.begin(), msg->data.end());
            cv::Mat frame = cv::imdecode(img_bytes, cv::IMREAD_COLOR);
            if (frame.empty()) {
                RCLCPP_ERROR(this->get_logger(), "이미지 디코딩 실패!");
                return;
            }

            int orig_width = frame.cols;
            int orig_height = frame.rows;

            // 2. 현재 누적된 흔들림 각도 가져오기
            double roll_rad = current_roll_.load();
            double pitch_rad = current_pitch_.load();

            // 라디안 -> 디그리 변환
            double roll_deg = roll_rad * (180.0 / M_PI);
            
            // Pitch(상하) 각도를 픽셀 이동량으로 변환
            // 너무 크면 어지러움 유발, 적절한 중간값 1.5배로 타협
            double focal_length_estimate = orig_height * 1.5; 
            double dy = std::tan(pitch_rad) * focal_length_estimate;

            // IMU와 카메라의 실제 장착 부호가 이미 상쇄(반대) 방향이거나,
            // OpenCV 좌표계(y가 아래로 증가) 특성 상 부호를 한 번 더 뒤집으면 
            // 오히려 진동이 '2배'로 증폭됩니다. (지터 수치 증가 원인)
            // 따라서 반전 없이 그대로 적용합니다.
            // roll_deg = -roll_deg;
            // dy = -dy; 

            // [신규] 하드 클리핑 (블랙박스 경계선 절대 노출 방지)
            // 크롭 마진이 10%이므로, 상하 이동량이 전체 높이의 10%를 넘지 못하게 강제 고정
            double margin = 0.10;
            double max_dy = orig_height * margin;
            if (dy > max_dy) dy = max_dy;
            if (dy < -max_dy) dy = -max_dy;
            
            // 회전(Roll)도 최대 +- 5도로 제한하여 화면이 크게 틀어지는 것 방지
            if (roll_deg > 5.0) roll_deg = 5.0;
            if (roll_deg < -5.0) roll_deg = -5.0;

            // 3. Affine 변환 행렬 생성 (회전 중심은 영상 중앙)
            cv::Point2f center(orig_width / 2.0f, orig_height / 2.0f);
            cv::Mat affine_matrix = cv::getRotationMatrix2D(center, roll_deg, 1.0);
            
            // 상하 평행이동(Translation) 축적
            affine_matrix.at<double>(1, 2) += dy;

            // 4. WarpAffine 적용
            cv::Mat stabilized;
            cv::warpAffine(frame, stabilized, affine_matrix, frame.size(), 
                           cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

            // 5. 검은색 여백 크롭 
            // 클리핑으로 안전을 확보했으므로 다시 10%로 줄여서 화각(FOV) 손실 최소화
            int crop_x = static_cast<int>(orig_width * margin);
            int crop_y = static_cast<int>(orig_height * margin);
            int crop_w = orig_width - (2 * crop_x);
            int crop_h = orig_height - (2 * crop_y);

            cv::Rect crop_roi(crop_x, crop_y, crop_w, crop_h);
            cv::Mat cropped = stabilized(crop_roi);

            // 6. 원래 해상도로 복구 (또는 720p로 고정)
            cv::Mat final_frame;
            cv::resize(cropped, final_frame, cv::Size(orig_width, orig_height), 0, 0, cv::INTER_LINEAR);

            // 7. JPEG 인코딩 및 퍼블리시
            std::vector<uchar> encoded_buf;
            std::vector<int> encode_params = {cv::IMWRITE_JPEG_QUALITY, 80};
            cv::imencode(".jpg", final_frame, encoded_buf, encode_params);

            sensor_msgs::msg::CompressedImage out_msg;
            out_msg.header = msg->header;
            out_msg.format = "jpeg";
            out_msg.data.assign(encoded_buf.begin(), encoded_buf.end());
            image_pub_->publish(out_msg);

            // 디버그 로그 (로그가 너무 많다는 피드백 수용: 1초 -> 5초 주기로 변경)
            static auto last_log = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log).count() >= 5) {
                last_log = now;
                auto t_end = std::chrono::high_resolution_clock::now();
                long latency = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
                RCLCPP_INFO(this->get_logger(), 
                    "[EIS] Latency: %ldms | Pitch Shift: %.1f px | Roll Angle: %.1f deg", 
                    latency, dy, roll_deg);
            }

        } catch (const cv::Exception& e) {
            RCLCPP_ERROR(this->get_logger(), "OpenCV 에러: %s", e.what());
        }
    }

    rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr imu_sub_;
    rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr image_sub_;
    rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr image_pub_;
};

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<EISNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
