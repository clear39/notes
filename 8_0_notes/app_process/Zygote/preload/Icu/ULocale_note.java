//	@external/icu/android_icu4j/src/main/java/android/icu/util/ULocale.java


/*

	ULocale[] localesToPin = { ULocale.ROOT, ULocale.US, ULocale.getDefault() };
    for (ULocale uLocale : localesToPin) {
        new DecimalFormatSymbols(uLocale);
    }

*/


/**
 * <strong>[icu enhancement(增强)]</strong> ICU's replacement for {@link java.util.Locale}.&nbsp;Methods, fields, and other functionality specific to ICU are labeled '<strong>[icu]</strong>'.
 *
 * A class analogous(相似的) to {@link java.util.Locale} that provides additional
 * support for ICU protocol.  In ICU 3.0 this class is enhanced to support
 * RFC 3066 language identifiers.
 *
 * <p>Many classes and services in ICU follow a factory idiom, in
 * which a factory method or object responds to a client request with
 * an object.  The request includes a locale (the <i>requested</i>
 * locale), and the returned object is constructed using data for that
 * locale.  The system may lack data for the requested locale, in
 * which case the locale fallback mechanism will be invoked until a
 * populated locale is found (the <i>valid</i> locale).  Furthermore,
 * even when a populated locale is found (the <i>valid</i> locale),
 * further fallback may be required to reach a locale containing the
 * specific data required by the service (the <i>actual</i> locale).
 *
 * <p>ULocale performs <b>'normalization'</b> and <b>'canonicalization'</b> of locale ids.
 * Normalization 'cleans up' ICU locale ids as follows:
 * <ul>
 * <li>language, script, country, variant, and keywords are properly cased<br>
 * (lower, title, upper, upper, and lower case respectively)</li>
 * <li>hyphens used as separators are converted to underscores</li>
 * <li>three-letter language and country ids are converted to two-letter
 * equivalents where available</li>
 * <li>surrounding spaces are removed from keywords and values</li>
 * <li>if there are multiple keywords, they are put in sorted order</li>
 * </ul>
 * Canonicalization additionally performs the following:
 * <ul>
 * <li>POSIX ids are converted to ICU format IDs</li>
 * <li>'grandfathered' 3066 ids are converted to ICU standard form</li>
 * <li>'PREEURO' and 'EURO' variants are converted to currency keyword form,
 * with the currency
 * id appropriate to the country of the locale (for PREEURO) or EUR (for EURO).
 * </ul>
 * All ULocale constructors automatically normalize the locale id.  To handle
 * POSIX ids, <code>canonicalize</code> can be called to convert the id
 * to canonical form, or the <code>canonicalInstance</code> factory method
 * can be called.
 *
 * <p>Note: The <i>actual</i> locale is returned correctly, but the <i>valid</i>
 * locale is not, in most cases.
 *
 * @see java.util.Locale
 * @author weiv
 * @author Alan Liu
 * @author Ram Viswanadha
 */
@SuppressWarnings("javadoc")    // android.icu.text.Collator is in another project
public final class ULocale implements Serializable, Comparable<ULocale> {


	  /**
     * The root ULocale.
     */
    public static final ULocale ROOT = new ULocale("", EMPTY_LOCALE);


        /**
     * Useful constant for country/region.
     */
    public static final ULocale US = new ULocale("en_US", Locale.US);

    private static Locale defaultLocale = Locale.getDefault();
    private static ULocale defaultULocale;

    static {
        defaultULocale = forLocale(defaultLocale);

        // For Java 6 or older JRE, ICU initializes the default script from
        // "user.script" system property. The system property was added
        // in Java 7. On JRE 7, Locale.getDefault() should reflect the
        // property value to the Locale's default. So ICU just relies on
        // Locale.getDefault().

        // Note: The "user.script" property is only used by initialization.
        //
        if (JDKLocaleHelper.hasLocaleCategories()) {
            for (Category cat: Category.values()) {
                int idx = cat.ordinal();
                defaultCategoryLocales[idx] = JDKLocaleHelper.getDefault(cat);
                defaultCategoryULocales[idx] = forLocale(defaultCategoryLocales[idx]);
            }
        } else {
            // Make sure the current default Locale is original.
            // If not, it means that someone updated the default Locale.
            // In this case, user.XXX properties are already out of date
            // and we should not use user.script.
            if (JDKLocaleHelper.isOriginalDefaultLocale(defaultLocale)) {
                // Use "user.script" if available
                String userScript = JDKLocaleHelper.getSystemProperty("user.script");
                if (userScript != null && LanguageTag.isScript(userScript)) {
                    // Note: Builder or forLanguageTag cannot be used here
                    // when one of Locale fields is not well-formed.
                    BaseLocale base = defaultULocale.base();
                    BaseLocale newBase = BaseLocale.getInstance(base.getLanguage(), userScript,
                            base.getRegion(), base.getVariant());
                    defaultULocale = getInstance(newBase, defaultULocale.extensions());
                }
            }

            // Java 6 or older does not have separated category locales,
            // use the non-category default for all
            for (Category cat: Category.values()) {
                int idx = cat.ordinal();
                defaultCategoryLocales[idx] = defaultLocale;
                defaultCategoryULocales[idx] = defaultULocale;
            }
        }
    }

    /**
     * Returns the current default ULocale.
     * <p>
     * The default ULocale is synchronized to the default Java Locale. This method checks
     * the current default Java Locale and returns an equivalent ULocale.
     *
     * @return the default ULocale.
     */
    public static ULocale getDefault() {
        synchronized (ULocale.class) {
            if (defaultULocale == null) {
                // When Java's default locale has extensions (such as ja-JP-u-ca-japanese),
                // Locale -> ULocale mapping requires BCP47 keyword mapping data that is currently
                // stored in a resource bundle. However, UResourceBundle currently requires
                // non-null default ULocale. For now, this implementation returns ULocale.ROOT
                // to avoid the problem.

                // TODO: Consider moving BCP47 mapping data out of resource bundle later.

                return ULocale.ROOT;
            }

            Locale currentDefault = Locale.getDefault();
            if (!defaultLocale.equals(currentDefault)) {// 这里不执行括号代码
                defaultLocale = currentDefault;
                defaultULocale = forLocale(currentDefault);

                if (!JDKLocaleHelper.hasLocaleCategories()) {
                    // Detected Java default Locale change.
                    // We need to update category defaults to match the
                    // Java 7's behavior on Java 6 or older environment.
                    for (Category cat : Category.values()) {
                        int idx = cat.ordinal();
                        defaultCategoryLocales[idx] = currentDefault;
                        defaultCategoryULocales[idx] = forLocale(currentDefault);
                    }
                }
            }
            return defaultULocale;
        }
    }



    /**
     * <strong>[icu]</strong> Converts this ULocale object to a {@link java.util.Locale}.
     * @return a {@link java.util.Locale} that either exactly represents this object
     * or is the closest approximation.
     */
    public Locale toLocale() {
        if (locale == null) {
            locale = JDKLocaleHelper.toLocale(this);
        }
        return locale;
    }

}