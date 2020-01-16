


/**
 * 
*/
 class Constants { // stores the filter constants.
    public:
        Constants() :mL(0), mShift(0), mHalfNumCoefs(0), mFirCoefs(NULL) {}
        void set(int L, int halfNumCoefs, int inSampleRate, int outSampleRate);

        int mL;            // interpolation phases in the filter.
        int mShift;        // right shift to get polyphase index
        unsigned int mHalfNumCoefs; // filter half #coefs
        const TC* mFirCoefs;     // polyphase filter bank
};

template<typename TC, typename TI, typename TO>
void AudioResamplerDyn<TC, TI, TO>::Constants::set(int L, int halfNumCoefs, int inSampleRate, int outSampleRate)
{
    int bits = 0;
    int lscale = inSampleRate/outSampleRate < 2 ? L - 1 : static_cast<int>(static_cast<uint64_t>(L)*inSampleRate/outSampleRate);

    for (int i=lscale; i; ++bits, i>>=1) ;

    mL = L;
    /**
     * frameworks/av/media/libaudioprocessing/include/media/AudioResampler.h:88:    static const int kNumPhaseBits = 30;
    */
    mShift = kNumPhaseBits - bits;// 30 -7 
    mHalfNumCoefs = halfNumCoefs;
}