import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.io.StringWriter;

public class EventLogStream extends PrintStream {
	public static final int INFORMATION = 0;
	public static final int WARNING     = 1;
	public static final int ERROR       = 2;
	private static ByteArrayOutputStream info    = new ByteArrayOutputStream();
	private static ByteArrayOutputStream warning = new ByteArrayOutputStream();
	private static ByteArrayOutputStream error   = new ByteArrayOutputStream();
	
	public static void initialize() {
		System.setOut(new EventLogStream(INFORMATION));
		System.setErr(new EventLogStream(WARNING));
	}
	
	private int type;
	ByteArrayOutputStream buffer;
	
	public EventLogStream(int type) {
		super(type==INFORMATION?info:(type==WARNING?warning:error));
		this.type = type;
		switch(type) {
			case INFORMATION: buffer = info;    break;
			case WARNING:     buffer = warning; break;
			case ERROR:       buffer = error;   break;
		}
	}
	
	public void close() {
		flush();
	}
	
	public void flush() {
		WriteEventLog(type, new String(buffer.toByteArray()));
		buffer.reset();
	}
	
	public void write(byte[] b, int off, int len) {
		buffer.write(b, off, len);
	}
	
	public void write(byte[] b) throws IOException {
		buffer.write(b);
	}
	
	public void write(int b) {
		buffer.write(b);
	}
	
	public void print(String s) {
		if("".equals(s)) {
			buffer.reset();
		} else {
			super.print(s);
		}
	}
	
	public static String getStackTrace(Throwable t) {
		StringWriter s = new StringWriter();
		PrintWriter w = new PrintWriter(s);
		t.printStackTrace(w);
		w.flush();
		s.flush();
		return s.toString();
	}
	
	public static native void WriteEventLog(int type, String message);
}
