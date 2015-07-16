package exewrap.util;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;

public class FileLogStream extends OutputStream {
	static {
		String path = System.getProperty("java.application.path");
		String name = System.getProperty("java.application.name");
		name = name.substring(0, name.lastIndexOf('.')) + ".log";
		FileLogStream stream = new FileLogStream(new File(path, name));
		System.setOut(new PrintStream(stream));
		System.setErr(new PrintStream(stream));
	}

	private File file;
	private FileOutputStream out;
	
	public FileLogStream(File file) {
		this.file = file;
	}
	
	private void open() {
		try {
			this.out = new FileOutputStream(this.file);
		} catch(FileNotFoundException e) {}
	}
	
	public void close() throws IOException {
		if(this.out != null) {
			this.out.close();
		}
	}
	
	public void flush() throws IOException {
	}
	
	public void write(byte[] b, int off, int len) throws IOException {
		if(this.out == null) {
			open();
		}
		this.out.write(b, off, len);
		this.out.flush();
	}
	
	public void write(byte[] b) throws IOException {
		if(out == null) {
			open();
		}
		this.out.write(b);
		this.out.flush();
	}
	
	public void write(int b) throws IOException {
		if(out == null) {
			open();
		}
		this.out.write(b);
		this.out.flush();
	}
}
