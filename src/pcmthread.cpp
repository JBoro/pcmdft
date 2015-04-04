#include "pcmthread.h"
#include <QObject>

#include <iostream>

#include <alsa/asoundlib.h>

#include "pcmsettings.h"
#include "buffer.h"

namespace PCMDFT
{

    class PCMThread::PCMHandle : public QObject
    {
        Q_OBJECT
    public:
        snd_pcm_t* pcm_handle_;

        PCMHandle (const std::string& pcmName, snd_pcm_stream_t type)
        {
            int err;

            if ( (err = snd_pcm_open (&pcm_handle_, pcmName.c_str (), type, 0)) < 0)
                throw std::runtime_error (snd_strerror (err));

            emit sigDebug ("Opened : " + QString::fromStdString (pcmName));
        }
        ~PCMHandle ()
        {
            snd_pcm_close (pcm_handle_);
            emit sigDebug ("Closed PCM handle");
        }
        operator snd_pcm_t* ()
        {
            return pcm_handle_;
        }

    signals:
        void sigDebug (QString value);

    };

#include "pcmthread.moc"

    PCMThread::PCMThread (std::shared_ptr<const PCMSettings> spSettings) :
        QThread (), spSettings_ {spSettings}, quit_ {false}
    {}

    PCMThread::~PCMThread () = default;

    void PCMThread::quit()
    {
        quit_ = true;
    }

    void PCMThread::slotDebug (QString value)
    {
        emit sigDebug (value);
    }

    void PCMThread::init()
    {
        snd_pcm_t* pcm_handle;
        snd_pcm_hw_params_t* hwparams;
        snd_pcm_uframes_t buffersize_return;
        unsigned int tmp;
        int err;

        std::unique_ptr<PCMHandle> spPCMHandle {new PCMHandle {spSettings_->pcmName_, SND_PCM_STREAM_CAPTURE}};
        QObject::connect (spPCMHandle.get(), &PCMThread::PCMHandle::sigDebug, this, &PCMThread::slotDebug);

        snd_pcm_hw_params_alloca (&hwparams);

        if ( (err = snd_pcm_hw_params_any (*spPCMHandle, hwparams)) < 0)
            throw std::runtime_error (snd_strerror (err));

        if ( (err = snd_pcm_hw_params_set_access (*spPCMHandle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
            throw std::runtime_error (snd_strerror (err));

        if ( (err = snd_pcm_hw_params_set_format (*spPCMHandle, hwparams, SND_PCM_FORMAT_FLOAT)) < 0)
            throw std::runtime_error (snd_strerror (err));

        tmp = spSettings_->rate_;

        if ( (err = snd_pcm_hw_params_set_rate_near (*spPCMHandle, hwparams, &tmp, 0)) < 0)
            throw std::runtime_error (snd_strerror (err));

        tmp = spSettings_->channels_;

        if ( (err = snd_pcm_hw_params_set_channels (*spPCMHandle, hwparams, tmp)) < 0)
            throw std::runtime_error (snd_strerror (err));

        tmp = spSettings_->periods_;

        if ( (err = snd_pcm_hw_params_set_periods (*spPCMHandle, hwparams, tmp, 0)) < 0)
            throw std::runtime_error (snd_strerror (err));

        buffersize_return = spSettings_->periodSize_ * spSettings_->periods_;

        if ( (err = snd_pcm_hw_params_set_buffer_size_near (*spPCMHandle, hwparams, &buffersize_return)) < 0)
            throw std::runtime_error (snd_strerror (err));

        if (buffersize_return != static_cast<snd_pcm_uframes_t> (spSettings_->periodSize_ * spSettings_->periods_))
        {
            DebugHelper dbgHelper;
            dbgHelper << "Period size " << spSettings_->periodSize_ << " not available, using " << buffersize_return / spSettings_->periods_;
            emit sigDebug (dbgHelper.string());
            periodSize_ = buffersize_return / spSettings_->periods_;
        }
        else
        {
            periodSize_ = spSettings_->periodSize_;
        }

        if ( (err = snd_pcm_hw_params (*spPCMHandle, hwparams)) < 0)
            throw std::runtime_error (snd_strerror (err));

        spPCMHandle_ = std::move (spPCMHandle);
        emit sigDebug ("Initialized : " + QString::fromStdString (spSettings_->pcmName_));
    }

    void PCMThread::run()
    {
        try
        {
            init ();

            while (!quit_)
            {
                QByteArray bytes;
                bytes.reserve (periodSize_ * spSettings_->frameSize_);
                snd_pcm_sframes_t nframes;

                while ( (nframes = snd_pcm_readi (*spPCMHandle_, bytes.data(),
                                                  periodSize_)) < 0)
                {
                    snd_pcm_prepare (*spPCMHandle_);
                    emit sigDebug ("<<<<<<<<<<<<<<< Buffer Overrun >>>>>>>>>>>>>>>");
                }

                bytes.resize (nframes * spSettings_->frameSize_);
                emit sigTimeSeriesReady (bytes);
            }
        }
        catch
            (const std::exception& e)
        {
            emit sigError (QString {"pcm error: "} + e.what());
        }
    }



}