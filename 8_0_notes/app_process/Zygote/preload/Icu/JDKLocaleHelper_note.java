//	@external/icu/android_icu4j/src/main/java/android/icu/util/ULocale.java


 /*
     * JDK Locale Helper
     */
private static final class JDKLocaleHelper {

	static {
        do {
            try {
                mGetScript = Locale.class.getMethod("getScript", (Class[]) null);
                mGetExtensionKeys = Locale.class.getMethod("getExtensionKeys", (Class[]) null);
                mGetExtension = Locale.class.getMethod("getExtension", char.class);
                mGetUnicodeLocaleKeys = Locale.class.getMethod("getUnicodeLocaleKeys", (Class[]) null);
                mGetUnicodeLocaleAttributes = Locale.class.getMethod("getUnicodeLocaleAttributes", (Class[]) null);
                mGetUnicodeLocaleType = Locale.class.getMethod("getUnicodeLocaleType", String.class);
                mForLanguageTag = Locale.class.getMethod("forLanguageTag", String.class);

                hasScriptsAndUnicodeExtensions = true;
            } catch (NoSuchMethodException e) {
            } catch (IllegalArgumentException e) {
            } catch (SecurityException e) {
                // TODO : report?
            }

            try {
                Class<?> cCategory = null;
                Class<?>[] classes = Locale.class.getDeclaredClasses();
                for (Class<?> c : classes) {
                    if (c.getName().equals("java.util.Locale$Category")) {
                        cCategory = c;
                        break;
                    }
                }
                if (cCategory == null) {
                    break;
                }
                mGetDefault = Locale.class.getDeclaredMethod("getDefault", cCategory);
                mSetDefault = Locale.class.getDeclaredMethod("setDefault", cCategory, Locale.class);

                Method mName = cCategory.getMethod("name", (Class[]) null);
                Object[] enumConstants = cCategory.getEnumConstants();
                for (Object e : enumConstants) {
                    String catVal = (String)mName.invoke(e, (Object[])null);
                    if (catVal.equals("DISPLAY")) {
                        eDISPLAY = e;
                    } else if (catVal.equals("FORMAT")) {
                        eFORMAT = e;
                    }
                }
                if (eDISPLAY == null || eFORMAT == null) {
                    break;
                }

                hasLocaleCategories = true;
            } catch (NoSuchMethodException e) {
            } catch (IllegalArgumentException e) {
            } catch (IllegalAccessException e) {
            } catch (InvocationTargetException e) {
            } catch (SecurityException e) {
                // TODO : report?
            }
        } while (false);
    }

  	public static Locale toLocale(ULocale uloc) {
        return hasScriptsAndUnicodeExtensions ? toLocale7(uloc) : toLocale6(uloc);//	hasScriptsAndUnicodeExtensions = true
    }
    
    private static Locale toLocale7(ULocale uloc) {
        Locale loc = null;
        String ulocStr = uloc.getName();
        if (uloc.getScript().length() > 0 || ulocStr.contains("@")) {
            // With script or keywords available, the best way
            // to get a mapped Locale is to go through a language tag.
            // A Locale with script or keywords can only have variants
            // that is 1 to 8 alphanum. If this ULocale has a variant
            // subtag not satisfying the criteria, the variant subtag
            // will be lost.
            String tag = uloc.toLanguageTag();

            // Workaround for variant casing problem:
            //
            // The variant field in ICU is case insensitive and normalized
            // to upper case letters by getVariant(), while
            // the variant field in JDK Locale is case sensitive.
            // ULocale#toLanguageTag use lower case characters for
            // BCP 47 variant and private use x-lvariant.
            //
            // Locale#forLanguageTag in JDK preserves character casing
            // for variant. Because ICU always normalizes variant to
            // upper case, we convert language tag to upper case here.
            tag = AsciiUtil.toUpperString(tag);

            try {
                loc = (Locale)mForLanguageTag.invoke(null, tag);
            } catch (IllegalAccessException e) {
                throw new RuntimeException(e);
            } catch (InvocationTargetException e) {
                throw new RuntimeException(e);
            }
        }
        if (loc == null) {
            // Without script or keywords, use a Locale constructor,
            // so we can preserve any ill-formed variants.
            loc = new Locale(uloc.getLanguage(), uloc.getCountry(), uloc.getVariant());
        }
        return loc;
    }
}
