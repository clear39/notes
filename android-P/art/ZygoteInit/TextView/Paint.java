


public class Paint {

	public Paint(int flags) {
        mNativePaint = nInit();
        NoImagePreloadHolder.sRegistry.registerNativeAllocation(this, mNativePaint);
        setFlags(flags | HIDDEN_DEFAULT_PAINT_FLAGS);
        // TODO: Turning off hinting has undesirable side effects, we need to
        //       revisit hinting once we add support for subpixel positioning
        // setHinting(DisplayMetrics.DENSITY_DEVICE >= DisplayMetrics.DENSITY_TV
        //        ? HINTING_OFF : HINTING_ON);
        mCompatScaling = mInvCompatScaling = 1;
        setTextLocales(LocaleList.getAdjustedDefault());
    }


    
    public void setAntiAlias(boolean aa) {
        nSetAntiAlias(mNativePaint, aa);
    }

    public Typeface setTypeface(Typeface typeface) {
        final long typefaceNative = typeface == null ? 0 : typeface.native_instance;
        nSetTypeface(mNativePaint, typefaceNative);
        mTypeface = typeface;
        return typeface;
    }

    public float measureText(char[] text, int index, int count) {
        if (text == null) {
            throw new IllegalArgumentException("text cannot be null");
        }
        if ((index | count) < 0 || index + count > text.length) {
            throw new ArrayIndexOutOfBoundsException();
        }

        if (text.length == 0 || count == 0) {
            return 0f;
        }
        if (!mHasCompatScaling) {
            return (float) Math.ceil(nGetTextAdvances(mNativePaint, text,index, count, index, count, mBidiFlags, null, 0));
        }

        final float oldSize = getTextSize();
        setTextSize(oldSize * mCompatScaling);
        final float w = nGetTextAdvances(mNativePaint, text, index, count, index, count,mBidiFlags, null, 0);
        setTextSize(oldSize);
        return (float) Math.ceil(w*mInvCompatScaling);
    }

}