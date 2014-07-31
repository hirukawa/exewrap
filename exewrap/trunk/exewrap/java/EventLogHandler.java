import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.LogRecord;
import java.util.logging.Logger;

public class EventLogHandler extends Handler {
	public static final int INFORMATION = 0;
	public static final int WARNING     = 1;
	public static final int ERROR       = 2;
	private static final Logger eventlog = Logger.getLogger("eventlog");
	
	public static void initialize() {
		EventLogHandler.eventlog.setUseParentHandlers(false);
		EventLogHandler.eventlog.addHandler(new EventLogHandler());
	}
	
	public void publish(LogRecord record) {
		int level = record.getLevel().intValue();
		
		if(level >= Level.SEVERE.intValue()) {
			WriteEventLog(ERROR, record.getMessage() + "");
		} else if(level >= Level.WARNING.intValue()) {
			WriteEventLog(WARNING, record.getMessage() + "");
		} else if(level >= Level.INFO.intValue()) {
			WriteEventLog(INFORMATION, record.getMessage() + "");
		}
	}

	public void flush() {
	}

	public void close() throws SecurityException {
	}
	
	public static native void WriteEventLog(int type, String message);
}
