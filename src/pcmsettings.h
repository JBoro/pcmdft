#ifndef PCM_SETTINGS_H
#define PCM_SETTINGS_H

namespace PCMDFT
{

    using SampleType = float;

    class PCMSettings
    {
    public:
        PCMSettings ()
        {}

        ~PCMSettings ()
        {}

        //default settings
        std::string pcmName_ {"plughw:0"}, clProgramName_ {"rdft.cl"}, clKernel_ {"rdft"};
        std::size_t sampleSize_ {sizeof (SampleType) }, rate_ {44100}, channels_ {2}, periodSize_ {8192}, periods_ {4}, frameSize_ {sampleSize_ * channels_};
    };

}
#endif
