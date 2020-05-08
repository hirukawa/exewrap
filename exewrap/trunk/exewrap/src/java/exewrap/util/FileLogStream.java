package exewrap.util;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;

public class FileLogStream extends OutputStream {
	static {
		String path = System.getProperty("java.application.path");
		String name = System.getProperty("java.application.name");
		name = name.substring(0, name.lastIndexOf('.')) + ".log";
		FileLogStream stream = new FileLogStream(path, name);
		String encoding = System.getProperty("exewrap.log.encoding");
		if(encoding != null) {
			try {
				System.setOut(new PrintStream(stream, true, encoding));
				System.setErr(new PrintStream(stream, true, encoding));
			} catch(UnsupportedEncodingException e) {
				encoding = null;
			}
		}
		if(encoding == null) {
			System.setOut(new PrintStream(stream, true));
			System.setErr(new PrintStream(stream, true));
		}
	}

	private String path;
	private String name;
	private FileOutputStream out;
	
	public FileLogStream(String path, String name) {
		this.path = path;
		this.name = name;
	}
	
	private void open() {
		if(out == null) {
			try {
				out = new FileOutputStream(new File(path, name));
			} catch(Exception ignore) {}
		}
		if(out == null) {
			try {
				out = new FileOutputStream(new File(System.getProperty("java.io.tmpdir"), name));
			} catch(Exception ignore) {}
		}
	}
	
	public void close() throws IOException {
		try {
			if(out != null) {
				out.close();
				out = null;
			}
		} catch(Exception ignore) {}
	}
	
	public void flush() throws IOException {
	}
	
	public void write(byte[] b, int off, int len) throws IOException {
		try {
			if(out == null) {
				open();
			}
			if(out != null) {
				out.write(b, off, len);
				out.flush();
			}
		} catch(Exception ignore) {}
	}
	
	public void write(byte[] b) throws IOException {
		try {
			if(out == null) {
				open();
			}
			if(out != null) {
				out.write(b);
				out.flush();
			}
		} catch(Exception ignore) {}
	}
	
	public void write(int b) throws IOException {
		try {
			if(out == null) {
				open();
			}
			if(out != null) {
				out.write(b);
				out.flush();
			}
		} catch(Exception ignore) {}
	}
}
