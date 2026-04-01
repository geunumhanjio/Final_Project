#include "frucvideoprocessor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <opencv2/core.hpp>
#include <opencv2/core/ocl.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>
#include <opencv2/videoio.hpp>

namespace {

constexpr int kFrucMultiplier = 4;
constexpr double kFastFlowScale = 0.25;
const char *kFastSuffix = "_FRUC_FAST";
const char *kHighQualitySuffix = "_FRUC_HQ";

bool shouldAbortProcessing(QString *errorMessage)
{
    if (!QThread::currentThread()->isInterruptionRequested()) {
        return false;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("FRUC processing was canceled during shutdown.");
    }
    return true;
}

QString suffixForVariant(FrucVideoProcessor::Variant variant)
{
    return (variant == FrucVideoProcessor::Variant::Fast)
               ? QString::fromLatin1(kFastSuffix)
               : QString::fromLatin1(kHighQualitySuffix);
}

QString buildOutputPath(const QString &inputFilePath, FrucVideoProcessor::Variant variant)
{
    const QFileInfo inputInfo(inputFilePath);
    const QString suffix = inputInfo.suffix().isEmpty() ? QStringLiteral("mp4") : inputInfo.suffix();
    return inputInfo.dir().filePath(inputInfo.completeBaseName() + suffixForVariant(variant) + QLatin1Char('.') + suffix);
}

bool isDerivedFrucFile(const QFileInfo &info)
{
    const QString baseName = info.completeBaseName();
    return baseName.contains(QString::fromLatin1(kFastSuffix), Qt::CaseInsensitive)
           || baseName.contains(QString::fromLatin1(kHighQualitySuffix), Qt::CaseInsensitive);
}

bool validateVideoCapture(cv::VideoCapture &capture, int *width, int *height, double *fps, QString *errorMessage)
{
    if (!capture.isOpened()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open the original recording.");
        }
        return false;
    }

    const int videoWidth = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));
    const int videoHeight = static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    double videoFps = capture.get(cv::CAP_PROP_FPS);

    if (videoWidth <= 0 || videoHeight <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Recording resolution is invalid.");
        }
        return false;
    }

    if (videoFps <= 1.0) {
        videoFps = 30.0;
    }

    if (width) {
        *width = videoWidth;
    }
    if (height) {
        *height = videoHeight;
    }
    if (fps) {
        *fps = videoFps;
    }
    return true;
}

bool processFastFrucVideo(const QString &inputFilePath, const QString &outputFilePath, QString *errorMessage)
{
    if (shouldAbortProcessing(errorMessage)) {
        return false;
    }

    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);
    cv::ocl::setUseOpenCL(true);

    cv::VideoCapture capture(inputFilePath.toStdString());
    int width = 0;
    int height = 0;
    double fps = 0.0;
    if (!validateVideoCapture(capture, &width, &height, &fps, errorMessage)) {
        return false;
    }

    const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    cv::VideoWriter writer(outputFilePath.toStdString(), fourcc, fps * kFrucMultiplier, cv::Size(width, height));
    if (!writer.isOpened()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create the FRUC FAST output file.");
        }
        return false;
    }

    cv::UMat frame1;
    capture >> frame1;
    if (frame1.empty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Recording is empty.");
        }
        return false;
    }

    cv::UMat gray1;
    cv::cvtColor(frame1, gray1, cv::COLOR_BGR2GRAY);

    cv::Mat baseXMat(height, width, CV_32FC1);
    cv::Mat baseYMat(height, width, CV_32FC1);
    for (int y = 0; y < height; ++y) {
        if (shouldAbortProcessing(errorMessage)) {
            return false;
        }
        for (int x = 0; x < width; ++x) {
            baseXMat.at<float>(y, x) = static_cast<float>(x);
            baseYMat.at<float>(y, x) = static_cast<float>(y);
        }
    }
    const cv::UMat baseX = baseXMat.getUMat(cv::ACCESS_READ);
    const cv::UMat baseY = baseYMat.getUMat(cv::ACCESS_READ);

    cv::Ptr<cv::DISOpticalFlow> dis = cv::DISOpticalFlow::create(cv::DISOpticalFlow::PRESET_FAST);
    dis->setUseSpatialPropagation(true);

    cv::UMat frame2;
    cv::UMat gray2;
    cv::UMat frameInter;
    cv::UMat gray1Small;
    cv::UMat gray2Small;
    cv::UMat flowFwSmall;
    cv::UMat flowBwSmall;
    cv::UMat flowFw;
    cv::UMat flowBw;

    while (true) {
        if (shouldAbortProcessing(errorMessage)) {
            return false;
        }

        capture >> frame2;
        if (frame2.empty()) {
            break;
        }

        cv::cvtColor(frame2, gray2, cv::COLOR_BGR2GRAY);

        cv::resize(gray1, gray1Small, cv::Size(), kFastFlowScale, kFastFlowScale, cv::INTER_AREA);
        cv::resize(gray2, gray2Small, cv::Size(), kFastFlowScale, kFastFlowScale, cv::INTER_AREA);

        dis->calc(gray1Small, gray2Small, flowFwSmall);
        cv::multiply(flowFwSmall, -1.0, flowBwSmall);

        cv::resize(flowFwSmall, flowFw, gray1.size(), 0, 0, cv::INTER_LINEAR);
        cv::resize(flowBwSmall, flowBw, gray1.size(), 0, 0, cv::INTER_LINEAR);

        cv::multiply(flowFw, 1.0 / kFastFlowScale, flowFw);
        cv::multiply(flowBw, 1.0 / kFastFlowScale, flowBw);

        writer.write(frame1);

        std::vector<cv::UMat> fwParts;
        std::vector<cv::UMat> bwParts;
        cv::split(flowFw, fwParts);
        cv::split(flowBw, bwParts);

        for (int i = 1; i < kFrucMultiplier; ++i) {
            if (shouldAbortProcessing(errorMessage)) {
                return false;
            }

            const float t = static_cast<float>(i) / static_cast<float>(kFrucMultiplier);

            cv::UMat mapX1;
            cv::UMat mapY1;
            cv::UMat mapX2;
            cv::UMat mapY2;
            cv::UMat tempX;
            cv::UMat tempY;

            cv::multiply(bwParts[0], t, tempX);
            cv::add(baseX, tempX, mapX1);
            cv::multiply(bwParts[1], t, tempY);
            cv::add(baseY, tempY, mapY1);

            cv::multiply(fwParts[0], 1.0f - t, tempX);
            cv::add(baseX, tempX, mapX2);
            cv::multiply(fwParts[1], 1.0f - t, tempY);
            cv::add(baseY, tempY, mapY2);

            cv::UMat warp1;
            cv::UMat warp2;
            cv::remap(frame1, warp1, mapX1, mapY1, cv::INTER_LINEAR);
            cv::remap(frame2, warp2, mapX2, mapY2, cv::INTER_LINEAR);
            cv::addWeighted(warp1, 1.0f - t, warp2, t, 0.0, frameInter);

            writer.write(frameInter);
        }

        frame1 = frame2.clone();
        gray1 = gray2.clone();
    }

    writer.write(frame1);
    writer.release();
    capture.release();
    return true;
}

bool processHighQualityFrucVideo(const QString &inputFilePath, const QString &outputFilePath, QString *errorMessage)
{
    if (shouldAbortProcessing(errorMessage)) {
        return false;
    }

    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);

    cv::VideoCapture capture(inputFilePath.toStdString());
    int width = 0;
    int height = 0;
    double fps = 0.0;
    if (!validateVideoCapture(capture, &width, &height, &fps, errorMessage)) {
        return false;
    }

    const int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    cv::VideoWriter writer(outputFilePath.toStdString(), fourcc, fps * kFrucMultiplier, cv::Size(width, height));
    if (!writer.isOpened()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create the FRUC HQ output file.");
        }
        return false;
    }

    cv::Mat frame1;
    capture >> frame1;
    if (frame1.empty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Recording is empty.");
        }
        return false;
    }

    cv::Mat gray1;
    cv::cvtColor(frame1, gray1, cv::COLOR_BGR2GRAY);

    cv::Mat frame2;
    cv::Mat gray2;
    cv::Mat frameInter;
    cv::Mat flowFw;
    cv::Mat flowBw;

    cv::Mat mapX1(height, width, CV_32FC1);
    cv::Mat mapY1(height, width, CV_32FC1);
    cv::Mat mapX2(height, width, CV_32FC1);
    cv::Mat mapY2(height, width, CV_32FC1);

    cv::Ptr<cv::DISOpticalFlow> dis = cv::DISOpticalFlow::create(cv::DISOpticalFlow::PRESET_MEDIUM);

    while (true) {
        if (shouldAbortProcessing(errorMessage)) {
            return false;
        }

        capture >> frame2;
        if (frame2.empty()) {
            break;
        }

        cv::cvtColor(frame2, gray2, cv::COLOR_BGR2GRAY);

        dis->calc(gray1, gray2, flowFw);
        dis->calc(gray2, gray1, flowBw);

        cv::medianBlur(flowFw, flowFw, 5);
        cv::medianBlur(flowBw, flowBw, 5);

        writer.write(frame1);

        for (int i = 1; i < kFrucMultiplier; ++i) {
            if (shouldAbortProcessing(errorMessage)) {
                return false;
            }

            const float t = static_cast<float>(i) / static_cast<float>(kFrucMultiplier);

            for (int y = 0; y < height; ++y) {
                if (shouldAbortProcessing(errorMessage)) {
                    return false;
                }

                for (int x = 0; x < width; ++x) {
                    const cv::Point2f fw = flowFw.at<cv::Point2f>(y, x);
                    const cv::Point2f bw = flowBw.at<cv::Point2f>(y, x);

                    mapX1.at<float>(y, x) = static_cast<float>(x) + t * bw.x;
                    mapY1.at<float>(y, x) = static_cast<float>(y) + t * bw.y;

                    mapX2.at<float>(y, x) = static_cast<float>(x) + (1.0f - t) * fw.x;
                    mapY2.at<float>(y, x) = static_cast<float>(y) + (1.0f - t) * fw.y;
                }
            }

            cv::Mat warp1;
            cv::Mat warp2;
            cv::remap(frame1, warp1, mapX1, mapY1, cv::INTER_LINEAR);
            cv::remap(frame2, warp2, mapX2, mapY2, cv::INTER_LINEAR);
            cv::addWeighted(warp1, 1.0f - t, warp2, t, 0.0, frameInter);

            writer.write(frameInter);
        }

        frame2.copyTo(frame1);
        gray2.copyTo(gray1);
    }

    writer.write(frame1);
    writer.release();
    capture.release();
    return true;
}

} // namespace

FrucVideoProcessor::FrucVideoProcessor(const QString &inputFilePath, Variant variant, QObject *parent)
    : QThread(parent)
    , m_inputFilePath(QDir::cleanPath(inputFilePath))
    , m_variant(variant)
{
}

bool FrucVideoProcessor::shouldProcessSourceFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }

    const QString suffix = info.suffix().trimmed().toLower();
    if (suffix != QStringLiteral("mp4")) {
        return false;
    }

    return !isDerivedFrucFile(info);
}

QString FrucVideoProcessor::outputPathFor(const QString &inputFilePath, Variant variant)
{
    return buildOutputPath(QDir::cleanPath(inputFilePath), variant);
}

void FrucVideoProcessor::run()
{
    if (isInterruptionRequested()) {
        emit processingFailed(m_inputFilePath, QStringLiteral("FRUC processing was canceled during shutdown."));
        return;
    }

    if (!shouldProcessSourceFile(m_inputFilePath)) {
        emit processingFailed(m_inputFilePath, QStringLiteral("FRUC processing skipped for this file."));
        return;
    }

    const QString outputFilePath = outputPathFor(m_inputFilePath, m_variant);
    const QFileInfo existingOutput(outputFilePath);
    if (existingOutput.exists() && existingOutput.size() > 1024) {
        emit processingFinished(m_inputFilePath, outputFilePath);
        return;
    }

    QFile::remove(outputFilePath);

    QString errorMessage;
    const bool success = (m_variant == Variant::Fast)
                             ? processFastFrucVideo(m_inputFilePath, outputFilePath, &errorMessage)
                             : processHighQualityFrucVideo(m_inputFilePath, outputFilePath, &errorMessage);

    if (!success) {
        QFile::remove(outputFilePath);
        emit processingFailed(m_inputFilePath,
                              errorMessage.isEmpty()
                                  ? QStringLiteral("FRUC processing failed.")
                                  : errorMessage);
        return;
    }

    emit processingFinished(m_inputFilePath, outputFilePath);
}
