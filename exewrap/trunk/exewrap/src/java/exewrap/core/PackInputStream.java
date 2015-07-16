package exewrap.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.util.jar.JarOutputStream;
import java.util.jar.Pack200;

public class PackInputStream extends InputStream implements Runnable {

	private static final int PIPE_SIZE = 512 * 1024;
	private InputStream in;
	private PipedInputStream pipeIn;
	private JarOutputStream jarOut;
	private IOException exception;
	
	public PackInputStream(InputStream in) {
		this.in = in;
	}
	
	public void run() {
		try {
			Pack200.newUnpacker().unpack(PackInputStream.this.in, this.jarOut);
		} catch(IOException e) {
			if(PackInputStream.this.exception == null) {
				PackInputStream.this.exception = e;
			}
		}
		try {
			if(this.jarOut != null) {
				this.jarOut.finish();
			}
		} catch(IOException e) {
			if(PackInputStream.this.exception == null) {
				PackInputStream.this.exception = e;
			}
		}
		try {
			if(this.jarOut != null) {
				this.jarOut.close();
			}
		} catch(IOException e) {
			if(PackInputStream.this.exception == null) {
				PackInputStream.this.exception = e;
			}
		}
	}
	
	private void init() throws IOException {
		PipedOutputStream pipeOut = new PipedOutputStream();
		this.jarOut = new JarOutputStream(pipeOut);
		this.pipeIn = new PipedInputStream(pipeOut, PIPE_SIZE);
		
		new Thread(this).start();
	}
	
	public int available() throws IOException {
		if(this.exception != null) {
			throw this.exception;
		}
		
		if(this.pipeIn == null) {
			init();
		}
		return this.pipeIn.available();
	}

	public long skip(long n) throws IOException {
		if(this.exception != null) {
			throw this.exception;
		}
		
		if(this.pipeIn == null) {
			init();
		}
		return this.pipeIn.skip(n);
	}

	public int read() throws IOException {
		if(this.exception != null) {
			throw this.exception;
		}
		
		if(this.pipeIn == null) {
			init();
		}
		return this.pipeIn.read();
	}

	public int read(byte[] b) throws IOException {
		if(this.exception != null) {
			throw this.exception;
		}
		
		if(this.pipeIn == null) {
			init();
		}
		return this.pipeIn.read(b);
	}

	public int read(byte[] b, int off, int len) throws IOException {
		if(this.exception != null) {
			throw this.exception;
		}
		
		if(this.pipeIn == null) {
			init();
		}
		return this.pipeIn.read(b, off, len);
	}

	public void close() throws IOException {
		this.pipeIn.close();
		this.in.close();
	}
}
