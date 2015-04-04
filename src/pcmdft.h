#ifndef pcmdft_H
#define pcmdft_H

#include <QMainWindow>
#include <QThread>
#include <QTimer>
#include <QByteArray>
#include <QMutex>

#include <memory>

namespace Ui
{
    class MainWindow;
}

class QwtPlotCurve;

namespace PCMDFT
{
    class PCMThread;
    class PCMSettings;
    class DFTThread;

    class pcmdft : public QMainWindow
    {
        Q_OBJECT

    public:

        pcmdft();
        ~pcmdft();

    protected:
        void closeEvent (QCloseEvent* event);

    public slots:
        //void update();
        void slotFreqCompReady (QByteArray tsBytes, QByteArray fcBytes);
        void slotPlatformChanged (int platformIdx);
        void slotError (QString value);
        void slotDebug (QString value);
        void slotStartClicked();
        void slotStopClicked();

    signals:
        void sigQuit();

    private:
        void stopThreads();
        std::unique_ptr<PCMThread> spPCMThread_;
        std::unique_ptr<DFTThread> spDFTThread_;
        std::unique_ptr<QTimer> spTimer_;
        std::unique_ptr<Ui::MainWindow> spWindow_;
        std::shared_ptr<PCMSettings> spSettings_;
        std::unique_ptr<QwtPlotCurve> spLCurve, spRCurve, spLFcCurve, spRFcCurve;
    };

}

#endif // pcmdft_H
