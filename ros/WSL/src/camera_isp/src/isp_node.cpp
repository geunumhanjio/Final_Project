/*
 * isp_node.cpp — 적응형 ISP 노드 (3-Mode Adaptive Preset System)
 *
 * 지원 모드:
 *   0: DAY     — 기본 (낮, 실내 조명 정상)
 *   1: NIGHT   — 야간/저조도 (노이즈 제거 강화, 샤프닝 완화)
 *   2: BACKLIT — 역광 (CLAHE 2배 강화)
 *
 * 설계 결정:
 *   - 모드 판단 로직은 매 30 프레임마다 썸네일(320×240)로 수행 → 렉 스파이크 없음
 *   - DAY→NIGHT: brightness < 40,  NIGHT→DAY: brightness > 60 (Deadzone: 40~60)
 *   - isp_mode_ 는 std::atomic<int> 로 스레드 안전하게 관리
 *   - 모드 전환 시 this->set_parameter()로 ROS 2 파라미터 서버 동기화
 */

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <opencv2/opencv.hpp>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

// ── 프리셋 파라미터 구조체 ────────────────────────────────────────────────────
struct ISPPreset {
    int    bilateral_d;       // Bilateral 필터 직경
    double bilateral_sigma;   // Bilateral σColor / σSpace
    double gamma_val;         // [최적화 1] CLAHE를 대체하는 감마(Gamma) 보정값
    double unsharp_alpha;     // [최적화 3] Laplacian 샤프닝 강도 α
};

// 3가지 프리셋 정의 (성능 최적화를 위해 Bilateral d 반경 축소)
static constexpr ISPPreset PRESET_DAY     = { 3,  25.0, 1.0, 0.5 };  // 낮: 원본 밝기 유지
static constexpr ISPPreset PRESET_NIGHT   = { 7,  75.0, 0.8, 0.2 };  // 야간: 노이즈 제거 위주 + 살짝 밝게
static constexpr ISPPreset PRESET_BACKLIT = { 3,  25.0, 0.4, 0.8 };  // 역광: 어두운 곳을 압도적으로 부스트 (Gamma 0.4)

// 전환 임계값 (Deadzone: 실내 카메라 기준, 극저조도 타겟)
static constexpr double BR_DAY_TO_NIGHT   = 35.0;   // 확실히 어두울 때만(35 이하) → NIGHT
static constexpr double BR_NIGHT_TO_DAY   = 55.0;   // 불이 켜져서 55 이상일 때 → DAY (Deadzone: 35~55)
static constexpr double BR_BACKLIT        = 180.0;  // 역광 진입 밝기 하한 (더욱 강화)
static constexpr double CT_BACKLIT        = 40.0;   // 역광 진입 대비 상한 (더욱 강화)
static constexpr double BR_BACKLIT_EXIT   = 150.0;  // 역광 탈출 밝기 상한
static constexpr double CT_BACKLIT_EXIT   = 55.0;   // 역광 탈출 대비 하한

// ── ROS 2 노드 ────────────────────────────────────────────────────────────────
class ISPNode : public rclcpp::Node
{
public:
    ISPNode() : Node("isp_node"), isp_mode_(0),
                 last_check_time_(std::chrono::steady_clock::now() - std::chrono::seconds(MODE_CHECK_SEC)),
                 last_latency_time_(std::chrono::steady_clock::now() - std::chrono::seconds(LATENCY_CHECK_SEC)),
                 last_frame_time_(std::chrono::steady_clock::now())
    {
        subscription_ = create_subscription<sensor_msgs::msg::CompressedImage>(
            "/camera/image_raw/compressed", 10,
            std::bind(&ISPNode::image_callback, this, std::placeholders::_1));

        publisher_ = create_publisher<sensor_msgs::msg::CompressedImage>(
            "/camera/image_isp/compressed", 10);

        // 외부 터미널에서 현재 모드를 조회할 수 있는 읽기용 파라미터
        declare_parameter("isp_mode_inspect", std::string("day"));

        // [Fast Math 최적화 1] 노드가 켜질 때 모드별 정답지(LUT)를 미리 계산해 굽습니다.
        // 실행 중에는 픽셀당 수학 연산 없이 O(1) 배열 참조로 끝납니다.
        for (int m = 0; m < 3; ++m) {
            luts_[m] = cv::Mat(1, 256, CV_8U);
            double gamma = get_preset(m).gamma_val;
            uchar* p = luts_[m].ptr();
            for (int i = 0; i < 256; ++i) {
                double val = i / 255.0;
                val = std::pow(val, gamma); // 감마 보정: 어두운 영역을 확 끌어올림
                p[i] = cv::saturate_cast<uchar>(val * 255.0);
            }
        }

        RCLCPP_INFO(get_logger(), "🚀 Adaptive ISP Node 가동 (Day/Night/Backlit 자동 전환)");
        RCLCPP_INFO(get_logger(), "   - 해상도 최적화: 1080p -> 720p 강제 축소 진행");
        RCLCPP_INFO(get_logger(), "   - Fast Math 최적화 (LUT + Laplacian Kernel) 적용 완료!");
        RCLCPP_INFO(get_logger(), "   - Deadzone: Day→Night < %.0f | Night→Day > %.0f",
                    BR_DAY_TO_NIGHT, BR_NIGHT_TO_DAY);
    }

private:
    std::atomic<int> isp_mode_;   // 0=DAY, 1=NIGHT, 2=BACKLIT
    std::chrono::steady_clock::time_point last_check_time_;
    std::chrono::steady_clock::time_point last_latency_time_;
    std::chrono::steady_clock::time_point last_frame_time_;  // [P1] 프레임 드롭용
    static constexpr int  MODE_CHECK_SEC    = 3;   // 모드 체크 주기 (초)
    static constexpr int  LATENCY_CHECK_SEC = 10;  // 지연율 로그 주기 (초)
    static constexpr int  TARGET_FPS        = 15;  // [P1] 최대 처리 FPS 제한
    cv::Mat luts_[3];  // 모드별 미리 구워둔 변경 테이블 (Look-Up Table)

    // ── 모드 이름 변환 ─────────────────────────────────────────────────────
    static std::string mode_name(int m)
    {
        if (m == 1) return "NIGHT";
        if (m == 2) return "BACKLIT";
        return "DAY";
    }

    static const ISPPreset& get_preset(int m)
    {
        if (m == 1) return PRESET_NIGHT;
        if (m == 2) return PRESET_BACKLIT;
        return PRESET_DAY;
    }

    // ── 모드 감지 및 전환 (Sub-Sampling 기반 초고속 판별, cvtColor 제거) ──────────────────
    void detect_and_switch_mode(const cv::Mat& frame)
    {
        // [P2 최적화] 전체 cvtColor 변환 제거 → BGR에서 직접 Y(밝기) 루마 공식으로 계산
        // BT.601 공식: Y = 0.299*R + 0.587*G + 0.114*B (BGR 순서 = B=ch0, G=ch1, R=ch2)
        // Sub-Sampling: 10×10 격자,100픽셀만 추출 → 통계 계산 비용 ~0ms
        const int grid_rows = 10;
        const int grid_cols = 10;
        const int y_step = std::max(1, frame.rows / grid_rows);
        const int x_step = std::max(1, frame.cols / grid_cols);

        double sum    = 0.0;
        double sq_sum = 0.0;
        int    count  = 0;

        for (int i = 0; i < grid_rows; ++i) {
            for (int j = 0; j < grid_cols; ++j) {
                int y = std::min(i * y_step + y_step / 2, frame.rows - 1);
                int x = std::min(j * x_step + x_step / 2, frame.cols - 1);

                const cv::Vec3b& px = frame.at<cv::Vec3b>(y, x);
                // Y 루마: 0=B, 1=G, 2=R
                double lum = 0.114 * px[0] + 0.587 * px[1] + 0.299 * px[2];
                sum    += lum;
                sq_sum += lum * lum;
                ++count;
            }
        }

        // 밝기(Brightness) = 평균, 대비(Contrast) = 표준편차
        double brightness = sum / count;
        double variance   = (sq_sum / count) - (brightness * brightness);
        double contrast   = std::sqrt(std::max(0.0, variance));

        // ── 전환 판정 (Deadzone 포함 Hysteresis) ──────────────────────────
        int current = isp_mode_.load();
        int next    = current;

        // 역광 진입 감지 (최우선)
        if (current != 2 && brightness > BR_BACKLIT && contrast < CT_BACKLIT) {
            next = 2;
        }
        // 역광 탈출 감지 (역광 모드일 때만 검사)
        else if (current == 2 && (brightness < BR_BACKLIT_EXIT || contrast > CT_BACKLIT_EXIT)) {
            next = 0; // 역광에서 빠져나오면 일단 DAY로 복귀
        }
        // DAY → NIGHT: 정말 어두운 환경(brightness < 35)일 때만 전환
        else if (current == 0 && brightness < BR_DAY_TO_NIGHT) {
            next = 1;
        }
        // NIGHT → DAY: 불이 켜져서 밝기가 회복(brightness > 55)되었을 때만 전환
        else if (current == 1 && brightness > BR_NIGHT_TO_DAY) {
            next = 0;
        }
        // 이외의 구간 → 현재 모드 유지 (Deadzone)

        if (next != current) {
            isp_mode_.store(next);
            std::string new_name = mode_name(next);
            RCLCPP_INFO(get_logger(),
                "🔄 [%s → %s] Brightness=%.1f | Contrast=%.1f",
                mode_name(current).c_str(), new_name.c_str(),
                brightness, contrast);
            this->set_parameter(rclcpp::Parameter("isp_mode_inspect", new_name));
        }

        RCLCPP_INFO(get_logger(),
            "[Mode: %-7s] Brightness=%.1f | Contrast=%.1f",
            mode_name(isp_mode_.load()).c_str(), brightness, contrast);
    }

    // ── ISP 파이프라인 적용 (Fast Math: LUT & Laplacian 최적화 적용) ──────────────────────────────────
    cv::Mat apply_isp(const cv::Mat& frame, int mode) const
    {
        const ISPPreset& p = get_preset(mode);

        // 1. Bilateral Filter (에지 보존 노이즈 제거)
        cv::Mat filtered;
        cv::bilateralFilter(frame, filtered,
                            p.bilateral_d, p.bilateral_sigma, p.bilateral_sigma);

        // 2. [최적화 1] LUT 매핑 — 무거운 CLAHE 대신 BGR → YCrCb 변환 후 배열 읽기로 O(1) 처리
        // * 참고: YCrCb 변환 연산이 수학적으로 Lab 변환보다 훨씬 빠릅니다.
        cv::Mat ycrcb;
        cv::cvtColor(filtered, ycrcb, cv::COLOR_BGR2YCrCb);
        
        std::vector<cv::Mat> channels(3);
        cv::split(ycrcb, channels);
        
        // 밝기(Y) 채널에 LUT 1방 씌우기 (연산시간 0ms 수렴)
        cv::LUT(channels[0], luts_[mode], channels[0]);
        
        cv::merge(channels, ycrcb);
        cv::Mat enhanced;
        cv::cvtColor(ycrcb, enhanced, cv::COLOR_YCrCb2BGR);

        // 3. [최적화 3] Laplacian Sharpening — 무거운 GaussianBlur를 3x3 에지 필터 한 방으로 대체
        // 십자가 모양 통과 필터 배열 = [0, -α, 0] / [-α, 1+4α, -α] / [0, -α, 0]
        float a = static_cast<float>(p.unsharp_alpha);
        cv::Mat kernel = (cv::Mat_<float>(3, 3) << 
             0.0f, -a,           0.0f,
            -a,     1.0f + 4.0f*a, -a,
             0.0f, -a,           0.0f);

        cv::Mat sharpened;
        cv::filter2D(enhanced, sharpened, -1, kernel);

        return sharpened;
    }

    // ── 메인 이미지 콜백 ───────────────────────────────────────────────────
    void image_callback(const sensor_msgs::msg::CompressedImage::SharedPtr msg)
    {
        // [P1] 15fps 프레임 드롭: 처리 간격이 TARGET_FPS 이하면 즉시 리턴
        //   → CPU 점유율을 최대 ~50% 절감하여 LiDAR/EKF 콜백 우선 처리 보장
        {
            auto now_fp = std::chrono::steady_clock::now();
            constexpr auto MIN_INTERVAL =
                std::chrono::milliseconds(1000 / TARGET_FPS);
            if (now_fp - last_frame_time_ < MIN_INTERVAL) return;
            last_frame_time_ = now_fp;
        }

        auto t_start = std::chrono::high_resolution_clock::now();

        try {
            // 1. JPEG 디코딩
            std::vector<uint8_t> img_bytes(msg->data.begin(), msg->data.end());
            cv::Mat frame = cv::imdecode(img_bytes, cv::IMREAD_COLOR);
            if (frame.empty()) {
                RCLCPP_ERROR(get_logger(), "이미지 디코딩 실패!");
                return;
            }

            // [P2] 1080p → 720p Downscaling (픽셀 수 55% 감소)
            cv::Mat resized_frame;
            if (frame.cols > 1280) {
                cv::resize(frame, resized_frame, cv::Size(1280, 720), 0, 0, cv::INTER_AREA);
            } else {
                resized_frame = frame;
            }

            // 2. 3초마다 모드 판단 (시간 기반 — FPS 무관하게 정확히 3초)
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                               now - last_check_time_).count();
            if (elapsed >= MODE_CHECK_SEC) {
                last_check_time_ = now;
                detect_and_switch_mode(resized_frame);
            }

            // 3. 현재 모드의 프리셋으로 ISP 파이프라인 적용
            int  mode   = isp_mode_.load();
            cv::Mat result = apply_isp(resized_frame, mode);

            // 4. JPEG 인코딩 → ROS 2 CompressedImage 발행
            std::vector<uchar> encoded_buf;
            cv::imencode(".jpg", result, encoded_buf,
                         {cv::IMWRITE_JPEG_QUALITY, 80});

            auto out_msg = sensor_msgs::msg::CompressedImage();
            out_msg.header = msg->header;
            out_msg.format = "jpeg";
            out_msg.data.assign(encoded_buf.begin(), encoded_buf.end());
            publisher_->publish(out_msg);

            // [P1 버그수정] 지연시간 측정을 try 블록 내부, 인코딩 직후에 수행
            //   → 이전 코드: t_end가 10초 조건 블록 안에서만 생성되어 실제 지연을 반영 못함
            //   → 수정 후  : 매 처리 프레임의 실제 시간을 측정하여 10초마다 로그 출력
            {
                auto t_end  = std::chrono::high_resolution_clock::now();
                auto now_lat = std::chrono::steady_clock::now();
                auto el_lat  = std::chrono::duration_cast<std::chrono::seconds>(
                                   now_lat - last_latency_time_).count();
                if (el_lat >= LATENCY_CHECK_SEC) {
                    last_latency_time_ = now_lat;
                    long latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       t_end - t_start).count();
                    RCLCPP_INFO(get_logger(),
                        "[OpenCV ISP | %-7s] Latency: %ld ms",
                        mode_name(isp_mode_.load()).c_str(), latency);
                }
            }

        } catch (const cv::Exception& e) {
            RCLCPP_ERROR(get_logger(), "OpenCV 에러: %s", e.what());
        }
    }

    rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr subscription_;
    rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr publisher_;
};

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ISPNode>());
    rclcpp::shutdown();
    return 0;
}
