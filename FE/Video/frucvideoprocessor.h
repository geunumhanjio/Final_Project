#ifndef FRUCVIDEOPROCESSOR_H
#define FRUCVIDEOPROCESSOR_H

#include <QThread>
#include <QString>

class FrucVideoProcessor : public QThread
{
    Q_OBJECT

public:
    enum class Variant {
        Fast,
        HighQuality
    };

    explicit FrucVideoProcessor(const QString &inputFilePath, Variant variant, QObject *parent = nullptr);

    static bool shouldProcessSourceFile(const QString &filePath);
    static QString outputPathFor(const QString &inputFilePath, Variant variant);

signals:
    void processingFinished(const QString &inputFilePath, const QString &outputFilePath);
    void processingFailed(const QString &inputFilePath, const QString &message);

protected:
    void run() override;

private:
    QString m_inputFilePath;
    Variant m_variant = Variant::Fast;
};

#endif // FRUCVIDEOPROCESSOR_H
