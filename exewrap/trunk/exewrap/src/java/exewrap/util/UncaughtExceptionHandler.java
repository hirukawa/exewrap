package exewrap.util;

import exewrap.core.NativeMethods;

public class UncaughtExceptionHandler implements java.lang.Thread.UncaughtExceptionHandler {
	
	static {
		Thread.setDefaultUncaughtExceptionHandler(new UncaughtExceptionHandler());
	}
	
	public void uncaughtException(Thread thread, Throwable throwable) {
		NativeMethods.UncaughtException(thread.getName(), throwable);
	}
}
