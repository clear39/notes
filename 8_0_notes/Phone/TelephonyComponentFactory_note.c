/**
 * This class has one-line methods to instantiate objects only. The purpose is to make code
 * unit-test friendly and use this class as a way to do dependency injection. Instantiating objects
 * this way makes it easier to mock them in tests.
 */


//TelephonyComponentFactory为构造器，没有具体实现，用于封装一些组件类创建
public class TelephonyComponentFactory {

    /*
    *单实例
    */
	public static TelephonyComponentFactory getInstance() {
        if (sInstance == null) {
            sInstance = new TelephonyComponentFactory(); //没有实现构造函数
        }
        return sInstance;
    }


    public DcTracker makeDcTracker(Phone phone) {
        return new DcTracker(phone);
    }

}