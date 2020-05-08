package exewrap.core;

public class NativeMethods {
	public static native void   WriteEventLog(int type, String message);
	public static native void   UncaughtException(Thread thread, Throwable throwable);
	public static native String SetEnvironment(String key, String value);
}
