/*
 * isp_node_custom.cpp
 *
 * 교육용 커스텀 ISP 노드 — 모든 알고리즘을 OpenCV 고수준 함수 없이
 * C++ 픽셀 배열 직접 순회(Raw Pixel Manipulation)로 from-scratch 구현
 *
 * 구현된 3가지 알고리즘:
 *   1. Box Blur (노이즈 제거용 평균 블러 - Bilateral의 교육적 대안)
 *   2. Histogram Equalization (명암비 개선 - CLAHE의 교육적 대안)
 *   3. Unsharp Mask (에지 선명화 - 가중치 픽셀 뺄셈으로 구현)
 *
 * ※ 처리 속도가 OpenCV 버전보다 훨씬 느립니다. (SIMD 최적화 없음)
 *   이 노드의 목적은 알고리즘 교육 및 성능 비교 데이터 수집입니다.
 */

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>
#include <opencv2/opencv.hpp>      // imdecode / imencode / Mat 구조체만 사용
#include <vector>
#include <chrono>
#include <algorithm>               // std::min, std::max, std::clamp
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// 알고리즘 1: Box Blur (커널 크기 k×k, 모든 픽셀의 평균)
//  → Bilateral 대신 사용. 픽셀 반복문으로 slinding window sum 구현
// ─────────────────────────────────────────────────────────────────────────────
cv::Mat custom_box_blur(const cv::Mat& src, int radius)
{
    /*
     * 각 픽셀(y, x)에 대해 주변 (2*radius+1) x (2*radius+1) 영역의
     * 픽셀 RGB 값을 직접 더한 뒤 개수로 나눠 평균을 구합니다.
     */
    int rows = src.rows;
    int cols = src.cols;
    cv::Mat dst(rows, cols, CV_8UC3);

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            int sum_b = 0, sum_g = 0, sum_r = 0, count = 0;

            for (int ky = -radius; ky <= radius; ++ky) {
                int ny = std::clamp(y + ky, 0, rows - 1);
                for (int kx = -radius; kx <= radius; ++kx) {
                    int nx = std::clamp(x + kx, 0, cols - 1);
                    const cv::Vec3b& pix = src.at<cv::Vec3b>(ny, nx);
                    sum_b += pix[0];
                    sum_g += pix[1];
                    sum_r += pix[2];
                    ++count;
                }
            }
            dst.at<cv::Vec3b>(y, x) = cv::Vec3b(
                static_cast<uint8_t>(sum_b / count),
                static_cast<uint8_t>(sum_g / count),
                static_cast<uint8_t>(sum_r / count)
            );
        }
    }
    return dst;
}

// ─────────────────────────────────────────────────────────────────────────────
// 알고리즘 2: Histogram Equalization on Luminance (밝기 채널 명암비 개선)
//  → CLAHE 대신 사용. 히스토그램 누적분포함수(CDF) 직접 계산
// ─────────────────────────────────────────────────────────────────────────────
cv::Mat custom_hist_equalize(const cv::Mat& src_bgr)
{
    /*
     * 단계:
     *  1. BGR → YCrCb 변환 (Y = 밝기, Cr/Cb = 색상)
     *  2. Y 채널에 대해 [0..255] 밝기 값의 히스토그램(빈도 수) 계산
     *  3. 히스토그램의 누적합(CDF) 계산
     *  4. CDF를 [0..255]로 정규화하여 새로운 밝기 값 매핑 테이블(LUT) 생성
     *  5. Y 채널의 각 픽셀에 LUT를 적용하여 균등하게 펼쳐진 밝기로 변경
     *  6. YCrCb → BGR 복원
     */
    cv::Mat ycrcb;
    cv::cvtColor(src_bgr, ycrcb, cv::COLOR_BGR2YCrCb);

    int rows = ycrcb.rows, cols = ycrcb.cols;
    int total_pixels = rows * cols;

    // Step 1: 히스토그램 계산 (밝기 0~255 각 값의 픽셀 수)
    int hist[256] = {0};
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
            hist[ycrcb.at<cv::Vec3b>(y, x)[0]]++;

    // Step 2: CDF(누적 분포 함수) 계산
    int cdf[256] = {0};
    cdf[0] = hist[0];
    for (int i = 1; i < 256; ++i)
        cdf[i] = cdf[i - 1] + hist[i];

    // Step 3: CDF를 [0,255]로 정규화하여 LUT(Look-Up Table) 생성
    int cdf_min = 0;
    for (int i = 0; i < 256; ++i) {
        if (cdf[i] > 0) { cdf_min = cdf[i]; break; }
    }
    uint8_t lut[256];
    for (int i = 0; i < 256; ++i) {
        lut[i] = static_cast<uint8_t>(
            std::round(
                (static_cast<float>(cdf[i] - cdf_min) /
                 static_cast<float>(total_pixels - cdf_min)) * 255.0f
            )
        );
    }

    // Step 4: 각 픽셀의 Y(밝기) 채널에 LUT 적용
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x)
            ycrcb.at<cv::Vec3b>(y, x)[0] = lut[ycrcb.at<cv::Vec3b>(y, x)[0]];

    cv::Mat result;
    cv::cvtColor(ycrcb, result, cv::COLOR_YCrCb2BGR);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// 알고리즘 3: Unsharp Mask (에지 선명화)
//  → 알고리즘 1의 커스텀 블러를 사용하여 픽셀별 가중치 뺄셈으로 구현
//    sharpened[y][x] = alpha * original[y][x] - beta * blurred[y][x]
// ─────────────────────────────────────────────────────────────────────────────
cv::Mat custom_unsharp_mask(const cv::Mat& src, float alpha, float beta)
{
    /*
     * Unsharp Mask 공식: sharpened = alpha * original + beta * (original - blurred)
     * 이를 전개하면:      sharpened = (alpha + beta) * original - beta * blurred
     * 여기서 alpha=1.5, beta=-0.5로 설정하면 addWeighted(src, 1.5, blurred, -0.5, 0)과 동일
     *
     * 여기서는 커스텀 Box Blur(radius=2)를 직접 사용하여
     * 픽셀 단위 뺄셈과 덧셈을 for 루프로 수행합니다.
     */
    cv::Mat blurred = custom_box_blur(src, 2);

    int rows = src.rows, cols = src.cols;
    cv::Mat dst(rows, cols, CV_8UC3);

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const cv::Vec3b& orig = src.at<cv::Vec3b>(y, x);
            const cv::Vec3b& blur = blurred.at<cv::Vec3b>(y, x);
            cv::Vec3b& out = dst.at<cv::Vec3b>(y, x);

            for (int c = 0; c < 3; ++c) {
                float val = alpha * orig[c] + beta * blur[c];
                out[c] = static_cast<uint8_t>(std::clamp(static_cast<int>(val), 0, 255));
            }
        }
    }
    return dst;
}

// ─────────────────────────────────────────────────────────────────────────────
// ROS 2 Node
// ─────────────────────────────────────────────────────────────────────────────
class ISPNodeCustom : public rclcpp::Node
{
public:
    ISPNodeCustom() : Node("isp_node_custom")
    {
        subscription_ = this->create_subscription<sensor_msgs::msg::CompressedImage>(
            "/camera/image_raw/compressed", 10,
            std::bind(&ISPNodeCustom::image_callback, this, std::placeholders::_1));

        publisher_ = this->create_publisher<sensor_msgs::msg::CompressedImage>(
            "/camera/image_isp_custom/compressed", 10);

        RCLCPP_INFO(this->get_logger(), "🧪 커스텀 ISP 노드 (Raw Pixel 구현) 시작!");
        RCLCPP_INFO(this->get_logger(), "👉 수신: /camera/image_raw/compressed");
        RCLCPP_INFO(this->get_logger(), "👉 송출: /camera/image_isp_custom/compressed");
    }

private:
    void image_callback(const sensor_msgs::msg::CompressedImage::SharedPtr msg)
    {
        // ── 처리 시간 측정 시작 ──
        auto t_start = std::chrono::high_resolution_clock::now();

        try {
            // 1. JPEG 디코딩
            std::vector<uint8_t> img_bytes(msg->data.begin(), msg->data.end());
            cv::Mat frame = cv::imdecode(img_bytes, cv::IMREAD_COLOR);
            if (frame.empty()) {
                RCLCPP_ERROR(this->get_logger(), "이미지 디코딩 실패!");
                return;
            }

            // 2. 커스텀 Box Blur (노이즈 제거)
            cv::Mat step1 = custom_box_blur(frame, 1);   // radius=1 (3x3 커널)

            // 3. 커스텀 Histogram Equalization (밝기/명암비 개선)
            cv::Mat step2 = custom_hist_equalize(step1);

            // 4. 커스텀 Unsharp Mask (에지 선명화)
            cv::Mat step3 = custom_unsharp_mask(step2, 1.5f, -0.5f);

            // 5. JPEG 인코딩
            std::vector<uchar> encoded_buf;
            cv::imencode(".jpg", step3, encoded_buf,
                         {cv::IMWRITE_JPEG_QUALITY, 80});

            // 6. ROS 2 메시지 발행
            auto out_msg = sensor_msgs::msg::CompressedImage();
            out_msg.header = msg->header;
            out_msg.format = "jpeg";
            out_msg.data.assign(encoded_buf.begin(), encoded_buf.end());
            publisher_->publish(out_msg);

        } catch (const std::exception& e) {
            RCLCPP_ERROR(this->get_logger(), "에러 발생: %s", e.what());
        }

        // ── 처리 시간 측정 종료 및 출력 ──
        auto t_end = std::chrono::high_resolution_clock::now();
        auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            t_end - t_start).count();

        RCLCPP_INFO(this->get_logger(),
            "[Custom ISP] Processing Latency: %ld ms", latency_ms);
    }

    rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr subscription_;
    rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr publisher_;
};

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ISPNodeCustom>());
    rclcpp::shutdown();
    return 0;
}
