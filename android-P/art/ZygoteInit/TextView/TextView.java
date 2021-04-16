

public class TextView extends View implements ViewTreeObserver.OnPreDrawListener {

	public static void preloadFontCache() {
        Paint p = new Paint();
        p.setAntiAlias(true);
        // Ensure that the Typeface is loaded here.
        // Typically, Typeface is preloaded by zygote but not on all devices, e.g. Android Auto.
        // So, sets Typeface.DEFAULT explicitly here for ensuring that the Typeface is loaded here
        // since Paint.measureText can not be called without Typeface static initializer.
        p.setTypeface(Typeface.DEFAULT);
        // We don't care about the result, just the side-effect of measuring.
        p.measureText("H");
    }


}