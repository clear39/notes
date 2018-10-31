//	@external/icu/android_icu4j/src/main/java/android/icu/text/DecimalFormatSymbols.java


/**
 * <strong>[icu enhancement]</strong> ICU's replacement for {@link java.text.DecimalFormatSymbols}.&nbsp;Methods, fields, and other functionality specific to ICU are labeled '<strong>[icu]</strong>'.
 *
 * This class represents the set of symbols (such as the decimal separator, the grouping
 * separator, and so on) needed by <code>DecimalFormat</code> to format
 * numbers. <code>DecimalFormat</code> creates for itself an instance of
 * <code>DecimalFormatSymbols</code> from its locale data.  If you need to change any of
 * these symbols, you can get the <code>DecimalFormatSymbols</code> object from your
 * <code>DecimalFormat</code> and modify it.
 *
 * @see          java.util.Locale
 * @see          DecimalFormat
 * @author       Mark Davis
 * @author       Alan Liu
 */
public class DecimalFormatSymbols implements Cloneable, Serializable {

	
    /**
     * <strong>[icu]</strong> Creates a DecimalFormatSymbols object for the given locale.
     * @param locale the locale
     */
    public DecimalFormatSymbols(ULocale locale) {
        initialize(locale);
    }


    /**
     * Initializes the symbols from the locale data.
     */
    private void initialize( ULocale locale ) {
        this.requestedLocale = locale.toLocale();
        this.ulocale = locale;
        CacheData data = cachedLocaleData.getInstance(locale, null /* unused */);
        setLocale(data.validLocale, data.validLocale);
        setDigitStrings(data.digits);
        String[] numberElements = data.numberElements;

        // Copy data from the numberElements map into instance fields
        setDecimalSeparatorString(numberElements[0]);
        setGroupingSeparatorString(numberElements[1]);

        // See CLDR #9781
        // assert numberElements[2].length() == 1;
        patternSeparator = numberElements[2].charAt(0);

        setPercentString(numberElements[3]);
        setMinusSignString(numberElements[4]);
        setPlusSignString(numberElements[5]);
        setExponentSeparator(numberElements[6]);
        setPerMillString(numberElements[7]);
        setInfinity(numberElements[8]);
        setNaN(numberElements[9]);
        setMonetaryDecimalSeparatorString(numberElements[10]);
        setMonetaryGroupingSeparatorString(numberElements[11]);
        setExponentMultiplicationSign(numberElements[12]);

        digit = DecimalFormat.PATTERN_DIGIT;  // Localized pattern character no longer in CLDR
        padEscape = DecimalFormat.PATTERN_PAD_ESCAPE;
        sigDigit  = DecimalFormat.PATTERN_SIGNIFICANT_DIGIT;


        CurrencyDisplayInfo info = CurrencyData.provider.getInstance(locale, true);

        // Obtain currency data from the currency API.  This is strictly
        // for backward compatibility; we don't use DecimalFormatSymbols
        // for currency data anymore.
        currency = Currency.getInstance(locale);
        if (currency != null) {
            intlCurrencySymbol = currency.getCurrencyCode();
            currencySymbol = currency.getName(locale, Currency.SYMBOL_NAME, null);
            CurrencyFormatInfo fmtInfo = info.getFormatInfo(intlCurrencySymbol);
            if (fmtInfo != null) {
                currencyPattern = fmtInfo.currencyPattern;
                setMonetaryDecimalSeparatorString(fmtInfo.monetarySeparator);
                setMonetaryGroupingSeparatorString(fmtInfo.monetaryGroupingSeparator);
            }
        } else {
            intlCurrencySymbol = "XXX";
            currencySymbol = "\u00A4"; // 'OX' currency symbol
        }


        // Get currency spacing data.
        initSpacingInfo(info.getSpacingInfo());
    }


    private void initSpacingInfo(CurrencySpacingInfo spcInfo) {
        currencySpcBeforeSym = spcInfo.getBeforeSymbols();
        currencySpcAfterSym = spcInfo.getAfterSymbols();
    }
}