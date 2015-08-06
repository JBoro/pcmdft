#ifndef BUFFER_H
#define BUFFER_H
#include <QTextStream>
#include <QString>
#include <vector>
#include <cstring>
#include <cmath>
#include "pcmsettings.h"

namespace PCMDFT
{

    class Buffer
    {
    public:
        Buffer() = default;
        ~Buffer() = default;

        SampleType& at (std::size_t chnl, std::size_t i)
        {
            return data_[chnl][i];
        }

        const SampleType& at (std::size_t chnl, std::size_t i) const
        {
            return const_cast<Buffer&> (*this).at (chnl, i);
        }

        std::size_t size() const
        {
            std::size_t sz = 0;

            for (auto it = data_.begin(); it != data_.end(); ++it)
            {
                sz += it->size ();
            }

            return sz;
        }

        std::size_t size (std::size_t chnl) const
        {
            data_[chnl].size();
        }


        std::size_t size1() const
        {
            return data_.size ();
        }


    protected:
        std::vector<std::vector<SampleType>> data_;
    };


    class TSBuffer : public Buffer
    {
    public:
        TSBuffer (std::shared_ptr<const PCMSettings> spSettings, const QByteArray& bytes) : Buffer{}
        {
            data_.resize (spSettings->channels_);

            for (int i = 0; i < spSettings->channels_; ++i)
            {
                data_[i].reserve (bytes.length() / spSettings->frameSize_);
            }

            for (int i = 0; i < bytes.length (); i += spSettings->frameSize_)
            {
                for (int j = 0, k = 0; j < spSettings->frameSize_; j += spSettings->sampleSize_, ++k)
                {
                    SampleType s;
                    std::copy (bytes.data() + i + j, bytes.data() + i + j + sizeof (SampleType), 
                                    reinterpret_cast<char*> (&s));
                    data_[k].push_back (s);
                }
            }
        }

        ~TSBuffer() = default;
    };

    class FreqBuffer : public Buffer
    {
    private:
        struct CpxNum
        {
            SampleType r_, i_;
            SampleType operator () ()
            {
                return ::sqrtf ( (r_ * r_) + (i_ * i_));
            }
        };

    public:
        FreqBuffer (std::shared_ptr<const PCMSettings> spSettings, const QByteArray& bytes) : Buffer{}
        {
            data_.resize (spSettings->channels_);
            int chnlSz = bytes.length () / spSettings->channels_;

            for (int i = 0; i < spSettings->channels_; ++i)
            {
                data_[i].reserve (chnlSz / spSettings->sampleSize_ / 2);
                int beg = chnlSz * i;
                int end = beg + chnlSz;

                for (int j = beg; j < end; j += sizeof (CpxNum))
                {
                    CpxNum fc;
                    std::copy (bytes.data() + j, bytes.data() + j + sizeof (fc), 
                                    reinterpret_cast<char*> (&fc));
                    data_[i].push_back (fc());
                }
            }
        }

        ~FreqBuffer() = default;
    };

    class DebugHelper
    {
    public:
        QString string()
        {
            return qStr_;
        }
        template <typename _T>
        friend DebugHelper& operator<< (DebugHelper& dbgHelper, const _T& value);
    private:
        QString qStr_;
    };

    template <typename _T>
    DebugHelper& operator<< (DebugHelper& dbgHelper, const _T& value)
    {
        QTextStream qStream {&dbgHelper.qStr_};
        qStream << value;
        return dbgHelper;
    }
}

#endif