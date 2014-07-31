import java.lang.Thread.UncaughtExceptionHandler;
import java.io.PrintWriter;
import java.io.StringWriter;

public class UncaughtHandler implements UncaughtExceptionHandler {
	
	public static void initialize() {
		Thread.setDefaultUncaughtExceptionHandler(new UncaughtHandler());
	}
	
	public void uncaughtException(Thread t, Throwable e) {
		System.err.print("Exception in thread \"" + t.getName() + "\" ");
		e.printStackTrace();
		UncaughtException(e.toString(), getStackTrace(e));
	}
	
	private static String getStackTrace(Throwable t) {
		StringWriter s = new StringWriter();
		PrintWriter w = new PrintWriter(s);
		t.printStackTrace(w);
		w.flush();
		s.flush();
		return s.toString();
	}
	
	public static native void UncaughtException(String message, String trace);
}
