package exewrap.util;

import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.LogRecord;
import java.util.logging.Logger;

import exewrap.core.ExewrapClassLoader;

public class EventLogHandler extends Handler {
	public static final int INFORMATION = 0;
	public static final int WARNING     = 1;
	public static final int ERROR       = 2;
	private static final Logger eventlog = Logger.getLogger("eventlog");
	
	static {
		EventLogHandler.eventlog.setUseParentHandlers(false);
		EventLogHandler.eventlog.addHandler(new EventLogHandler());
	}
	
	public void publish(LogRecord record) {
		int level = record.getLevel().intValue();
		
		if(level >= Level.SEVERE.intValue()) {
			ExewrapClassLoader.WriteEventLog(ERROR, record.getMessage() + "");
		} else if(level >= Level.WARNING.intValue()) {
			ExewrapClassLoader.WriteEventLog(WARNING, record.getMessage() + "");
		} else if(level >= Level.INFO.intValue()) {
			ExewrapClassLoader.WriteEventLog(INFORMATION, record.getMessage() + "");
		}
	}

	public void flush() {
	}

	public void close() throws SecurityException {
	}
}
