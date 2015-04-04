#include "pcmdft.h"

#include <QLabel>
#include <QVector>
#include <QPointF>
#include <QCloseEvent>
#include <QDebug>
#include <QMessageBox>
#include <QAction>
#include <QMenuBar>
#include <QPushButton>
#include <qwt_plot_curve.h>
#include <qwt_series_data.h>

#include <iostream>

#include "pcmthread.h"
#include "dftthread.h"
#include "buffer.h"
#include "pcmsettings.h"
#include "ui_pcmdftwindow.h"

namespace PCMDFT
{

    pcmdft::pcmdft() : spWindow_ {new Ui::MainWindow}, spSettings_ {new PCMSettings}
    {
        spWindow_->setupUi (this);
        spWindow_->tsPlotL->setAutoReplot (true);
        spWindow_->tsPlotR->setAutoReplot (true);
        spWindow_->tsPlotL->setAutoDelete (true);
        spWindow_->tsPlotR->setAutoDelete (true);
        spWindow_->fcPlotL->setAutoReplot (true);
        spWindow_->fcPlotR->setAutoReplot (true);
        spWindow_->fcPlotL->setAutoDelete (true);
        spWindow_->fcPlotR->setAutoDelete (true);

        spLCurve_.reset (new QwtPlotCurve ("L"));
        spRCurve_.reset (new QwtPlotCurve ("R"));
        spLFcCurve_.reset (new QwtPlotCurve ("Lfc"));
        spRFcCurve_.reset (new QwtPlotCurve ("Rfc"));
        spLCurve_->attach (spWindow_->tsPlotL);
        spRCurve_->attach (spWindow_->tsPlotR);
        spLFcCurve_->attach (spWindow_->fcPlotL);
        spRFcCurve_->attach (spWindow_->fcPlotR);

        spWindow_->comboPlatforms->insertItems (0, DFTThread::getPlatformList());

        if (spWindow_->comboPlatforms->count())
        {
            slotPlatformChanged (0);
        }

        QObject::connect (spWindow_->comboPlatforms, static_cast<void (QComboBox::*) (int) > (&QComboBox::currentIndexChanged), this, &pcmdft::slotPlatformChanged);
        QObject::connect (spWindow_->btnStart, &QPushButton::clicked, this, &pcmdft::slotStartClicked);
        QObject::connect (spWindow_->btnStop, &QPushButton::clicked, this, &pcmdft::slotStopClicked);
        QObject::connect (spWindow_->actionQuit, &QAction::triggered, this, &pcmdft::close);
    }

    pcmdft::~pcmdft() = default;

    void pcmdft::stopThreads()
    {
        if (spPCMThread_)
        {
            QObject::disconnect (spPCMThread_.get(), &PCMThread::sigTimeSeriesReady, 0, 0);
            spPCMThread_->quit();
            spPCMThread_->wait();
            spPCMThread_.reset (nullptr);
        }

        if (spDFTThread_)
        {
            QObject::disconnect (spDFTThread_.get(), 0, 0, 0);
            emit sigQuit();
            spDFTThread_->waitForThread();
            spDFTThread_.reset (nullptr);
        }
    }

    void pcmdft::closeEvent (QCloseEvent* event)
    {
        stopThreads();

        if (event)
            event->accept();
    }

    void pcmdft::slotPlatformChanged (int platformIdx)
    {
        spWindow_->comboDevices->clear();
        spWindow_->comboDevices->insertItems (0, DFTThread::getDeviceList (platformIdx));
    }

    void pcmdft::slotStopClicked ()
    {
        stopThreads();
    }

    void pcmdft::slotStartClicked ()
    {
        int platformIdx = spWindow_->comboPlatforms->currentIndex(),
            deviceIdx = spWindow_->comboDevices->currentIndex();

        if (platformIdx < 0 || deviceIdx < 0)
        {
            slotError ("Select both a platform and a device");
            return;
        }

        try
        {
            spPCMThread_.reset (new PCMThread {spSettings_});
            spDFTThread_.reset (new DFTThread {spSettings_, 0, platformIdx, deviceIdx});
            QObject::connect (spPCMThread_.get(), &PCMThread::sigTimeSeriesReady, spDFTThread_.get(), &DFTThread::slotTimeSeriesUpdate);
            QObject::connect (spDFTThread_.get(), &DFTThread::sigFreqCompReady, this, &pcmdft::slotFreqCompReady);
            QObject::connect (this, &pcmdft::sigQuit, spDFTThread_.get(), &DFTThread::slotQuit);
            QObject::connect (spPCMThread_.get(), &PCMThread::sigError, this, &pcmdft::slotError);
            QObject::connect (spPCMThread_.get(), &PCMThread::sigDebug, this, &pcmdft::slotDebug);
            QObject::connect (spDFTThread_.get(), &DFTThread::sigError, this, &pcmdft::slotError);
            QObject::connect (spDFTThread_.get(), &DFTThread::sigDebug, this, &pcmdft::slotDebug);
            spPCMThread_->start();
        }
        catch
            (const std::exception& e)
        {
            slotError (QString {"Error starting pcmdft: "} + e.what());
        }
    }

    void pcmdft::slotError (QString value)
    {
        stopThreads();
        QMessageBox::information (this, tr ("pcmdft"),
                                  tr ("The following error occurred: %1.")
                                  .arg (value));
    }

    void pcmdft::slotDebug (QString value)
    {
        qDebug() << value;
    }

    void pcmdft::slotFreqCompReady (QByteArray tsBytes, QByteArray fcBytes)
    {
        QVector<QPointF> lTs, rTs, lFc, rFc;
        TSBuffer tsBuf {spSettings_, tsBytes};
        FreqBuffer fcBuf {spSettings_, fcBytes};
        lTs.reserve (tsBuf.size () / tsBuf.size1());
        rTs.reserve (tsBuf.size () / tsBuf.size1());
        lFc.reserve (fcBuf.size () / fcBuf.size1());
        rFc.reserve (fcBuf.size () / fcBuf.size1());


        for (int i = 0; i < tsBuf.size (); i += 2)
        {
            lTs.push_back (QPointF { (i / 2), tsBuf.at (0, i / 2) });
            rTs.push_back (QPointF { (i / 2), tsBuf.at (1, i / 2) });
        }

        float deltaT {1. / spSettings_->rate_* spSettings_->periodSize_};

        for (int i = 2; i < fcBuf.size (); i += 2)
        {
            lFc.push_back (QPointF {1. / deltaT * (i / 2), fcBuf.at (0, i / 2) });
            rFc.push_back (QPointF {1. / deltaT * (i / 2), fcBuf.at (1, i / 2) });
        }

        QwtPointSeriesData* lSeries = new QwtPointSeriesData {lTs};
        QwtPointSeriesData* rSeries = new QwtPointSeriesData {rTs};
        QwtPointSeriesData* lFcSeries = new QwtPointSeriesData {lFc};
        QwtPointSeriesData* rFcSeries = new QwtPointSeriesData {rFc};
        spLCurve_->setData (lSeries);
        spRCurve_->setData (rSeries);
        spLFcCurve_->setData (lFcSeries);
        spRFcCurve_->setData (rFcSeries);
    }

}
