//	@frameworks/base/core/java/android/content/res/ResourcesImpl.java




/**
 * The implementation of Resource access. This class contains the AssetManager and all caches
 * associated with it.
 *
 * {@link Resources} is just a thing wrapper around this class. When a configuration change
 * occurs, clients can retain the same {@link Resources} reference because the underlying
 * {@link ResourcesImpl} object will be updated or re-created.
 *
 * @hide
 */
public class ResourcesImpl {

	static {
        sPreloadedDrawables = new LongSparseArray[2];
        sPreloadedDrawables[0] = new LongSparseArray<>();
        sPreloadedDrawables[1] = new LongSparseArray<>();
    }


    /**
     * Creates a new ResourcesImpl object with CompatibilityInfo.
     *
     * @param assets Previously created AssetManager.
     * @param metrics Current display metrics to consider when selecting/computing resource values.
     * @param config Desired device configuration to consider when selecting/computing resource values (optional).
     * @param displayAdjustments this resource's Display override and compatibility info. Must not be null.
     */
    public ResourcesImpl(@NonNull AssetManager assets, @Nullable DisplayMetrics metrics,
            @Nullable Configuration config, @NonNull DisplayAdjustments displayAdjustments) {
        mAssets = assets;
        mMetrics.setToDefaults();
        mDisplayAdjustments = displayAdjustments;
        mConfiguration.setToDefaults();
        updateConfiguration(config, metrics, displayAdjustments.getCompatibilityInfo());
        mAssets.ensureStringBlocks();
    }

    /**
     * Start preloading of resource data using this Resources object.  Only
     * for use by the zygote process for loading common system resources.
     * {@hide}
     */
    public final void startPreloading() {
        synchronized (sSync) {
            if (sPreloaded) {
                throw new IllegalStateException("Resources already preloaded");
            }
            sPreloaded = true;
            mPreloading = true;
            mConfiguration.densityDpi = DisplayMetrics.DENSITY_DEVICE;
            updateConfiguration(null, null, null);
        }
    }



    public void updateConfiguration(Configuration config, DisplayMetrics metrics,CompatibilityInfo compat) {
        Trace.traceBegin(Trace.TRACE_TAG_RESOURCES, "ResourcesImpl#updateConfiguration");
        try {
            synchronized (mAccessLock) {
                if (false) {
                    Slog.i(TAG, "**** Updating config of " + this + ": old config is "
                            + mConfiguration + " old compat is " + mDisplayAdjustments.getCompatibilityInfo());
                    Slog.i(TAG, "**** Updating config of " + this + ": new config is " + config + " new compat is " + compat);
                }
                if (compat != null) {
                    mDisplayAdjustments.setCompatibilityInfo(compat);
                }
                if (metrics != null) {
                    mMetrics.setTo(metrics);
                }
                // NOTE: We should re-arrange this code to create a Display
                // with the CompatibilityInfo that is used everywhere we deal
                // with the display in relation to this app, rather than
                // doing the conversion here.  This impl should be okay because
                // we make sure to return a compatible display in the places
                // where there are public APIs to retrieve the display...  but
                // it would be cleaner and more maintainable to just be
                // consistently dealing with a compatible display everywhere in
                // the framework.
                mDisplayAdjustments.getCompatibilityInfo().applyToDisplayMetrics(mMetrics);

                final @Config int configChanges = calcConfigChanges(config);

                // If even after the update there are no Locales set, grab the default locales.
                LocaleList locales = mConfiguration.getLocales();
                if (locales.isEmpty()) {
                    locales = LocaleList.getDefault();
                    mConfiguration.setLocales(locales);
                }

                if ((configChanges & ActivityInfo.CONFIG_LOCALE) != 0) {
                    if (locales.size() > 1) {
                        // The LocaleList has changed. We must query the AssetManager's available
                        // Locales and figure out the best matching Locale in the new LocaleList.
                        String[] availableLocales = mAssets.getNonSystemLocales();
                        if (LocaleList.isPseudoLocalesOnly(availableLocales)) {
                            // No app defined locales, so grab the system locales.
                            availableLocales = mAssets.getLocales();
                            if (LocaleList.isPseudoLocalesOnly(availableLocales)) {
                                availableLocales = null;
                            }
                        }

                        if (availableLocales != null) {
                            final Locale bestLocale = locales.getFirstMatchWithEnglishSupported(availableLocales);
                            if (bestLocale != null && bestLocale != locales.get(0)) {
                                mConfiguration.setLocales(new LocaleList(bestLocale, locales));
                            }
                        }
                    }
                }

                if (mConfiguration.densityDpi != Configuration.DENSITY_DPI_UNDEFINED) {
                    mMetrics.densityDpi = mConfiguration.densityDpi;
                    mMetrics.density = mConfiguration.densityDpi * DisplayMetrics.DENSITY_DEFAULT_SCALE;
                }

                // Protect against an unset fontScale.
                mMetrics.scaledDensity = mMetrics.density * (mConfiguration.fontScale != 0 ? mConfiguration.fontScale : 1.0f);

                final int width, height;
                if (mMetrics.widthPixels >= mMetrics.heightPixels) {
                    width = mMetrics.widthPixels;
                    height = mMetrics.heightPixels;
                } else {
                    //noinspection SuspiciousNameCombination
                    width = mMetrics.heightPixels;
                    //noinspection SuspiciousNameCombination
                    height = mMetrics.widthPixels;
                }

                final int keyboardHidden;
                if (mConfiguration.keyboardHidden == Configuration.KEYBOARDHIDDEN_NO && mConfiguration.hardKeyboardHidden == Configuration.HARDKEYBOARDHIDDEN_YES) {
                    keyboardHidden = Configuration.KEYBOARDHIDDEN_SOFT;
                } else {
                    keyboardHidden = mConfiguration.keyboardHidden;
                }

                mAssets.setConfiguration(mConfiguration.mcc, mConfiguration.mnc,
                        adjustLanguageTag(mConfiguration.getLocales().get(0).toLanguageTag()),
                        mConfiguration.orientation,
                        mConfiguration.touchscreen,
                        mConfiguration.densityDpi, mConfiguration.keyboard,
                        keyboardHidden, mConfiguration.navigation, width, height,
                        mConfiguration.smallestScreenWidthDp,
                        mConfiguration.screenWidthDp, mConfiguration.screenHeightDp,
                        mConfiguration.screenLayout, mConfiguration.uiMode,
                        mConfiguration.colorMode, Build.VERSION.RESOURCES_SDK_INT);

                if (DEBUG_CONFIG) {
                    Slog.i(TAG, "**** Updating config of " + this + ": final config is " + mConfiguration + " final compat is " + mDisplayAdjustments.getCompatibilityInfo());
                }

                mDrawableCache.onConfigurationChange(configChanges);
                mColorDrawableCache.onConfigurationChange(configChanges);
                mComplexColorCache.onConfigurationChange(configChanges);
                mAnimatorCache.onConfigurationChange(configChanges);
                mStateListAnimatorCache.onConfigurationChange(configChanges);

                flushLayoutCache();
            }
            synchronized (sSync) {
                if (mPluralRule != null) {
                    mPluralRule = PluralRules.forLocale(mConfiguration.getLocales().get(0));
                }
            }
        } finally {
            Trace.traceEnd(Trace.TRACE_TAG_RESOURCES);
        }
    }



    public AssetManager getAssets() {
        return mAssets;
    }



    Drawable loadDrawable(@NonNull Resources wrapper, @NonNull TypedValue value, int id,int density, @Nullable Resources.Theme theme)
            throws NotFoundException {
        // If the drawable's XML lives in our current density qualifier,
        // it's okay to use a scaled version from the cache. Otherwise, we
        // need to actually load the drawable from XML.
        final boolean useCache = density == 0 || value.density == mMetrics.densityDpi;

        // Pretend the requested density is actually the display density. If
        // the drawable returned is not the requested density, then force it
        // to be scaled later by dividing its density by the ratio of
        // requested density to actual device density. Drawables that have
        // undefined density or no density don't need to be handled here.
        if (density > 0 && value.density > 0 && value.density != TypedValue.DENSITY_NONE) {
            if (value.density == density) {
                value.density = mMetrics.densityDpi;
            } else {
                value.density = (value.density * mMetrics.densityDpi) / density;
            }
        }

        try {
            if (TRACE_FOR_PRELOAD) {// false
                // Log only framework resources
                if ((id >>> 24) == 0x1) {
                    final String name = getResourceName(id);
                    if (name != null) {
                        Log.d("PreloadDrawable", name);
                    }
                }
            }

            final boolean isColorDrawable;
            final DrawableCache caches;
            final long key;
            //判断是否为颜色类型
            if (value.type >= TypedValue.TYPE_FIRST_COLOR_INT && value.type <= TypedValue.TYPE_LAST_COLOR_INT) {
                isColorDrawable = true;
                caches = mColorDrawableCache;
                key = value.data;
            } else {
                isColorDrawable = false;
                caches = mDrawableCache;
                key = (((long) value.assetCookie) << 32) | value.data;
            }

            // First, check whether we have a cached version of this drawable
            // that was inflated against the specified theme. Skip the cache if
            // we're currently preloading or we're not using the cache.
            if (!mPreloading && useCache) {//	mPreloading 为true表示正在Preload模式
                final Drawable cachedDrawable = caches.getInstance(key, wrapper, theme);
                if (cachedDrawable != null) {
                    cachedDrawable.setChangingConfigurations(value.changingConfigurations);
                    return cachedDrawable;
                }
            }

            // Next, check preloaded drawables. Preloaded drawables may contain
            // unresolved theme attributes.
            final Drawable.ConstantState cs;
            if (isColorDrawable) {
                cs = sPreloadedColorDrawables.get(key);
            } else {
                cs = sPreloadedDrawables[mConfiguration.getLayoutDirection()].get(key);
            }

            Drawable dr;
            boolean needsNewDrawableAfterCache = false;
            if (cs != null) {
                dr = cs.newDrawable(wrapper);
            } else if (isColorDrawable) {
                dr = new ColorDrawable(value.data);
            } else {
                dr = loadDrawableForCookie(wrapper, value, id, density, null);
            }
            // DrawableContainer' constant state has drawables instances. In order to leave the
            // constant state intact in the cache, we need to create a new DrawableContainer after
            // added to cache.
            if (dr instanceof DrawableContainer)  {
                needsNewDrawableAfterCache = true;
            }

            // Determine if the drawable has unresolved theme attributes. If it
            // does, we'll need to apply a theme and store it in a theme-specific
            // cache.
            final boolean canApplyTheme = dr != null && dr.canApplyTheme();
            if (canApplyTheme && theme != null) {
                dr = dr.mutate();
                dr.applyTheme(theme);
                dr.clearMutated();
            }

            // If we were able to obtain a drawable, store it in the appropriate
            // cache: preload, not themed, null theme, or theme-specific. Don't
            // pollute the cache with drawables loaded from a foreign density.
            if (dr != null) {
                dr.setChangingConfigurations(value.changingConfigurations);
                if (useCache) {
                    cacheDrawable(value, isColorDrawable, caches, theme, canApplyTheme, key, dr);
                    if (needsNewDrawableAfterCache) {
                        Drawable.ConstantState state = dr.getConstantState();
                        if (state != null) {
                            dr = state.newDrawable(wrapper);
                        }
                    }
                }
            }

            return dr;
        } catch (Exception e) {
            String name;
            try {
                name = getResourceName(id);
            } catch (NotFoundException e2) {
                name = "(missing name)";
            }

            // The target drawable might fail to load for any number of
            // reasons, but we always want to include the resource name.
            // Since the client already expects this method to throw a
            // NotFoundException, just throw one of those.
            final NotFoundException nfe = new NotFoundException("Drawable " + name
                    + " with resource ID #0x" + Integer.toHexString(id), e);
            nfe.setStackTrace(new StackTraceElement[0]);
            throw nfe;
        }
    }



        /**
     * Loads a drawable from XML or resources stream.
     */
    private Drawable loadDrawableForCookie(@NonNull Resources wrapper, @NonNull TypedValue value,
            int id, int density, @Nullable Resources.Theme theme) {
        if (value.string == null) {
            throw new NotFoundException("Resource \"" + getResourceName(id) + "\" (" + Integer.toHexString(id) + ") is not a Drawable (color or path): " + value);
        }

        final String file = value.string.toString();

        if (TRACE_FOR_MISS_PRELOAD) {
            // Log only framework resources
            if ((id >>> 24) == 0x1) {
                final String name = getResourceName(id);
                if (name != null) {
                    Log.d(TAG, "Loading framework drawable #" + Integer.toHexString(id)  + ": " + name + " at " + file);
                }
            }
        }

        if (DEBUG_LOAD) {
            Log.v(TAG, "Loading drawable for cookie " + value.assetCookie + ": " + file);
        }

        final Drawable dr;

        Trace.traceBegin(Trace.TRACE_TAG_RESOURCES, file);
        try {
            if (file.endsWith(".xml")) {
            	//xml文件图片
                final XmlResourceParser rp = loadXmlResourceParser(file, id, value.assetCookie, "drawable");
                dr = Drawable.createFromXmlForDensity(wrapper, rp, density, theme);
                rp.close();
            } else {
            	//图片资源文件
                final InputStream is = mAssets.openNonAsset(value.assetCookie, file, AssetManager.ACCESS_STREAMING);
                dr = Drawable.createFromResourceStream(wrapper, value, is, file, null);
                is.close();
            }
        } catch (Exception e) {
            Trace.traceEnd(Trace.TRACE_TAG_RESOURCES);
            final NotFoundException rnf = new NotFoundException("File " + file + " from drawable resource ID #0x" + Integer.toHexString(id));
            rnf.initCause(e);
            throw rnf;
        }
        Trace.traceEnd(Trace.TRACE_TAG_RESOURCES);

        return dr;
    }


}
