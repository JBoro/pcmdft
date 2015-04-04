#ifndef DFTTHREAD_H
#define DFTTHREAD_H
#include <QObject>
#include <QThread>
#include <QStringList>

#include <memory>


namespace PCMDFT
{

class PCMSettings;

class DFTThread : public QObject
{
    Q_OBJECT
public:
    DFTThread (std::shared_ptr<const PCMSettings> spSettings, QObject* parent = 0,
                std::size_t clPlatId = 0, std::size_t clDeviceId = 0);
    ~DFTThread();

    static QStringList getPlatformList();
    static QStringList getDeviceList (std::size_t platformId);

    void waitForThread();

public slots:
    void slotQuit();
    void slotTimeSeriesUpdate (QByteArray bytes);

signals:
    void sigFreqCompReady (QByteArray tsBytes, QByteArray fcBytes);
    void sigError (QString value);
    void sigDebug (QString value);

private:
    void init();
    std::unique_ptr<QThread> spThread_;
    std::shared_ptr<const PCMSettings> spSettings_;
    struct CLData;
    std::unique_ptr<CLData> spCLData_;
    std::size_t clPlatId_, clDeviceId_;
};

}


#endif
