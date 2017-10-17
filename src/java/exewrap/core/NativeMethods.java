package exewrap.core;

public class NativeMethods {
	public static native void WriteConsole(byte[] b, int off, int len);
	public static native void WriteEventLog(int type, String message);
	public static native void UncaughtException(String thread, String message, String trace);
	public static native String SetEnvironment(String key, String value);
}
