//	@frameworks/base/core/java/android/content/res/TypedArray.java
/**
 * Container for an array of values that were retrieved with
 * {@link Resources.Theme#obtainStyledAttributes(AttributeSet, int[], int, int)}
 * or {@link Resources#obtainAttributes}.  Be
 * sure to call {@link #recycle} when done with them.
 *
 * The indices used to retrieve values from this structure correspond to
 * the positions of the attributes given to obtainStyledAttributes.
 */
public class TypedArray {


	static TypedArray obtain(Resources res, int len) {
        TypedArray attrs = res.mTypedArrayPool.acquire();//查找在缓冲区中是否有空闲的成员可以用，和recycle对应
        if (attrs == null) {
            attrs = new TypedArray(res);
        }

        attrs.mRecycled = false;
        // Reset the assets, which may have changed due to configuration changes
        // or further resource loading.
        attrs.mAssets = res.getAssets();
        attrs.mMetrics = res.getDisplayMetrics();
        attrs.resize(len);
        return attrs;
    }

    /** @hide */
    protected TypedArray(Resources resources) {
        mResources = resources;
        mMetrics = mResources.getDisplayMetrics();
        mAssets = mResources.getAssets();
    }


    /**
     * Recycles the TypedArray, to be re-used by a later caller. After calling
     * this function you must not ever touch the typed array again.
     *
     * @throws RuntimeException if the TypedArray has already been recycled.
     */
    public void recycle() {
        if (mRecycled) {
            throw new RuntimeException(toString() + " recycled twice!");
        }

        mRecycled = true;

        // These may have been set by the client.
        mXml = null;
        mTheme = null;
        mAssets = null;

        mResources.mTypedArrayPool.release(this);
    }




     /**
     * Retrieves the resource identifier for the attribute at
     * <var>index</var>.  Note that attribute resource as resolved when
     * the overall {@link TypedArray} object is retrieved.  As a
     * result, this function will return the resource identifier of the
     * final resource value that was found, <em>not</em> necessarily the
     * original resource that was specified by the attribute.
     *
     * @param index Index of attribute to retrieve.
     * @param defValue Value to return if the attribute is not defined or
     *                 not a resource.
     *
     * @return Attribute resource identifier, or defValue if not defined.
     * @throws RuntimeException if the TypedArray has already been recycled.
     */
    @AnyRes
    public int getResourceId(@StyleableRes int index, int defValue) {
        if (mRecycled) {
            throw new RuntimeException("Cannot make calls to a recycled instance!");
        }

        index *= AssetManager.STYLE_NUM_ENTRIES;
        final int[] data = mData;
        if (data[index+AssetManager.STYLE_TYPE] != TypedValue.TYPE_NULL) {
            final int resid = data[index+AssetManager.STYLE_RESOURCE_ID];
            if (resid != 0) {
                return resid;
            }
        }
        return defValue;
    }
}