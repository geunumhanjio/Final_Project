#ifndef APPLICATIONINITIALIZER_H
#define APPLICATIONINITIALIZER_H

#include <QString>
#include <QFont>

class ApplicationInitializer
{
public:
    static bool initializeEnvironment();
    static bool initializeGStreamer();
    static bool initializeOpenCV();
    static bool initializeFonts();
    static bool initializeSSL();
    static QFont getApplicationFont();
    
private:
    static QString findGStreamerBinPath();
    static QString findGStreamerRootPath();
    static bool setupGStreamerEnvironment(const QString &binPath, const QString &rootPath);
    static bool setupOpenCVEnvironment();
};

#endif // APPLICATIONINITIALIZER_H
