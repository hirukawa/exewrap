package exewrap.util;

import java.io.PrintWriter;
import java.io.StringWriter;

import exewrap.core.ExewrapClassLoader;

public class UncaughtExceptionHandler implements java.lang.Thread.UncaughtExceptionHandler {
	
	static {
		Thread.setDefaultUncaughtExceptionHandler(new UncaughtExceptionHandler());
	}
	
	public void uncaughtException(Thread t, Throwable e) {
		e.printStackTrace();
		ExewrapClassLoader.UncaughtException(t.getName(), e.toString(), getStackTrace(e));
	}
	
	private static String getStackTrace(Throwable t) {
		StringWriter s = new StringWriter();
		PrintWriter w = new PrintWriter(s);
		t.printStackTrace(w);
		w.flush();
		s.flush();
		return s.toString();
	}
}
