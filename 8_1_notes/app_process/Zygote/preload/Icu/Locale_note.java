//	@libcore/ojluni/src/main/java/java/util/Locale.java

public final class Locale implements Cloneable, Serializable {
	 /**
     * Enum for locale categories.  These locale categories are used to get/set
     * the default locale for the specific functionality represented by the
     * category.
     *
     * @see #getDefault(Locale.Category)
     * @see #setDefault(Locale.Category, Locale)
     * @since 1.7
     */
    public enum Category {

        /**
         * Category used to represent the default locale for
         * displaying user interfaces.
         */
        DISPLAY("user.language.display",
                "user.script.display",
                "user.country.display",
                "user.variant.display"),

        /**
         * Category used to represent the default locale for
         * formatting dates, numbers, and/or currencies.
         */
        FORMAT("user.language.format",
               "user.script.format",
               "user.country.format",
               "user.variant.format");

        Category(String languageKey, String scriptKey, String countryKey, String variantKey) {
            this.languageKey = languageKey;
            this.scriptKey = scriptKey;
            this.countryKey = countryKey;
            this.variantKey = variantKey;
        }

        final String languageKey;
        final String scriptKey;
        final String countryKey;
        final String variantKey;
    }


     public static Locale forLanguageTag(String languageTagx) {
        LanguageTag tag = LanguageTag.parse(languageTag, null);
        InternalLocaleBuilder bldr = new InternalLocaleBuilder();
        bldr.setLanguageTag(tag);
        BaseLocale base = bldr.getBaseLocale();
        LocaleExtensions exts = bldr.getLocaleExtensions();
        if (exts == null && base.getVariant().length() > 0) {
            exts = getCompatibilityExtensions(base.getLanguage(), base.getScript(),base.getRegion(), base.getVariant());
        }
        return getInstance(base, exts);
    }


    public Locale(String language, String country, String variant) {
        if (language== null || country == null || variant == null) {
            throw new NullPointerException();
        }
        baseLocale = BaseLocale.getInstance(convertOldISOCodes(language), "", country, variant);
        localeExtensions = getCompatibilityExtensions(language, "", country, variant);
    }

}