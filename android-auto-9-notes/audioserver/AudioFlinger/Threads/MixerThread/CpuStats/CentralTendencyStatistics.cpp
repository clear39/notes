/***
 * /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/include/cpustats/CentralTendencyStatistics.h
 * /work/workcodes/tsu-aosp-p9.0.0_2.1.0-auto-ga/frameworks/av/media/libcpustats/CentralTendencyStatistics.cpp
 * 
 * */   


/**
 * 
*/
// Not multithread safe
class CentralTendencyStatistics {

}



/**
 * 
*/
 CentralTendencyStatistics:: CentralTendencyStatistics() :
            mMean(NAN), mMedian(NAN), mMinimum(INFINITY), mMaximum(-INFINITY), mN(0), mM2(0),
            mVariance(NAN), mVarianceKnownForN(0), mStddev(NAN), mStddevKnownForN(0) { 

}


void CentralTendencyStatistics::sample(double x)
{
    // update min and max
    if (x < mMinimum)
        mMinimum = x;
    if (x > mMaximum)
        mMaximum = x;
    // Knuth
    if (mN == 0) {
        mMean = 0;
    }
    ++mN;
    double delta = x - mMean;
    mMean += delta / mN;
    mM2 += delta * (x - mMean);
}