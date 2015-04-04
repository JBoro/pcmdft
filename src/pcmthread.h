#ifndef PCM_THREAD_H
#define PCM_THREAD_H

#include <QThread>
#include <QByteArray>

#include <memory>

namespace PCMDFT
{

    class PCMSettings;


    class PCMThread : public QThread
    {
        Q_OBJECT

    public:
        PCMThread (std::shared_ptr<const PCMSettings> spSettings);
        ~PCMThread ();
        void run();
        void init();
        void quit();

    public slots:
        void slotDebug (QString value);

    signals:
        void sigTimeSeriesReady (QByteArray bytes);
        void sigError (QString value);
        void sigDebug (QString value) const;

    private:
        std::shared_ptr<const PCMSettings> spSettings_;
        struct PCMHandle;
        std::unique_ptr<PCMHandle> spPCMHandle_;
        volatile bool quit_;
        std::size_t periodSize_;
    };

}

#endif
