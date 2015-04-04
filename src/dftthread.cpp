#include "dftthread.h"
#include <QTextStream>
#include <fstream>
#include <iostream>
#include <cmath>
#define __CL_ENABLE_EXCEPTIONS
#include <CL/cl.hpp>

#include "buffer.h"

namespace PCMDFT
{
struct DFTThread::CLData
{
    std::unique_ptr<cl::Program> spProgram_;
    std::vector<cl::Platform> platforms_;
    std::vector<cl::Device> devices_;
    std::unique_ptr<cl::Kernel> spKernel_;
    std::unique_ptr<cl::Context> spContext_;
};

DFTThread::DFTThread (std::shared_ptr<const PCMSettings> spSettings, QObject* parent,
                      std::size_t clPlatId, std::size_t clDeviceId) :
    QObject {parent}, spThread_ {new QThread}, spSettings_ {spSettings},
spCLData_ {new CLData}, clPlatId_ {clPlatId}, clDeviceId_ {clDeviceId}
{
    this->moveToThread (spThread_.get());
    spThread_->start();
}

DFTThread::~DFTThread() = default;

QStringList DFTThread::getPlatformList()
{
    std::vector<cl::Platform> platforms;
    cl::Platform::get (&platforms);
    QStringList qList;

    for (std::vector<cl::Platform>::iterator it = platforms.begin(); it != platforms.end(); ++it)
    {
        qList << QString::fromStdString (it->getInfo<CL_PLATFORM_NAME> ());
    }

    return qList;
}

QStringList DFTThread::getDeviceList (std::size_t platformId)
{
    std::vector<cl::Platform> platforms;
    std::vector<cl::Device> devices;
    cl::Platform::get (&platforms);
    platforms.at (platformId).getDevices (CL_DEVICE_TYPE_ALL, &devices);
    QStringList qList;

    for (std::vector<cl::Device>::iterator it = devices.begin(); it != devices.end(); ++it)
    {
        qList << QString::fromStdString (it->getInfo<CL_DEVICE_NAME>());
    }

    return qList;
}

void DFTThread::init()
{
    std::ifstream programFile {spSettings_->clProgramName_};
    std::string programString {std::istreambuf_iterator<char> (programFile),
                               (std::istreambuf_iterator<char>())
                              };
    cl::Program::Sources source {1, std::make_pair (programString.c_str(),
                                 programString.length() + 1)
                                };
    cl::Platform::get (&spCLData_->platforms_);
    spCLData_->platforms_.at (clPlatId_).getDevices (CL_DEVICE_TYPE_ALL, &spCLData_->devices_);
    spCLData_->spContext_.reset (new cl::Context {spCLData_->devices_});

    // Build and create the kernel
    spCLData_->spProgram_.reset (new cl::Program {*spCLData_->spContext_, source});
    spCLData_->spProgram_->build (spCLData_->devices_);
    spCLData_->spKernel_.reset (new cl::Kernel {*spCLData_->spProgram_, spSettings_->clKernel_.c_str() });
}

void DFTThread::slotQuit()
{
    spThread_->quit();
}

void DFTThread::waitForThread()
{
    spThread_->wait();
}

void DFTThread::slotTimeSeriesUpdate (QByteArray bytes)
{
    try
    {
        if (!spCLData_->spKernel_)
        {
            init();
        }

        TSBuffer buf {spSettings_, bytes};
        QByteArray freqData;

        for (std::size_t i = 0; i < buf.size1(); ++i)
        {
            //Set the local size to the preferred multiple
            std::size_t szLocal
            {
                spCLData_->spKernel_->getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE> (spCLData_->devices_.at (clDeviceId_))
            };

            //Make sure the global size is a multiple of the local size
            std::size_t szGlobal {std::ceil ( (buf.size (i) / 2. + 1.) / szLocal)* szLocal};

            //Get the data size
            int N {buf.size (i) };

            //Get the size in bytes
            std::size_t szData {N * sizeof (buf.at (i, 0)) };

            //Populate the input buffer
            QByteArray clBuf {reinterpret_cast<const char*> (&buf.at (i, 0)), szData};
            cl::Buffer buffer1 {*spCLData_->spContext_,
                                CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, clBuf.size(), clBuf.data()
                               };

            //Allocate the output buffer
            QByteArray clOutBuf;
            clOutBuf.resize (szGlobal * 2 * sizeof (buf.at (i, 0)));
            cl::Buffer buffer2 {*spCLData_->spContext_,
                                CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, clOutBuf.size(), clOutBuf.data()
                               };

            //Set the arguments
            spCLData_->spKernel_->setArg (0, buffer1);
            spCLData_->spKernel_->setArg (1, buffer2);
            spCLData_->spKernel_->setArg (2, sizeof (N), &N);

            cl::Event profileEvent;
            cl::CommandQueue queue {*spCLData_->spContext_, spCLData_->devices_.at (clDeviceId_),
                                    CL_QUEUE_PROFILING_ENABLE
                                   };
            cl::NDRange local_size {szLocal};
            cl::NDRange global_size {szGlobal};
            {
                DebugHelper dbgHelper;
                dbgHelper << "global size " << szGlobal << " local size " << szLocal << " N: " << N;
                emit sigDebug (dbgHelper.string());
            }

            //Enqueue the kernel and wait
            queue.enqueueNDRangeKernel (*spCLData_->spKernel_, cl::NullRange, global_size, local_size, NULL, &profileEvent);
            queue.finish();

            cl_ulong start = profileEvent.getProfilingInfo<CL_PROFILING_COMMAND_START>();
            cl_ulong end = profileEvent.getProfilingInfo<CL_PROFILING_COMMAND_END>();
            {
                DebugHelper dbgHelper;
                dbgHelper << "\t\tElapsed time: " << (end - start) / 1000. / 1000. << " ms." << " on " <<
                          QString::fromStdString (spCLData_->devices_.at (clDeviceId_).getInfo<CL_DEVICE_NAME>());
                emit sigDebug (dbgHelper.string());
            }

            //Read the data back
            queue.enqueueReadBuffer (buffer2, CL_TRUE, 0, clOutBuf.size(), clOutBuf.data());

            //Reduce to the data size, extra data is possible if the data size was not a multiple of the local size
            clOutBuf.resize (szData);
            freqData.append (clOutBuf);
        }

        emit sigFreqCompReady (bytes, freqData);
    }
    catch
        (const std::exception& e)
    {
        emit sigError (QString {"DFTThread error: "} + e.what());
    }
}

}

