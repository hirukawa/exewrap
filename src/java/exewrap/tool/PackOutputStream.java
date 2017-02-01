package exewrap.tool;

import java.io.IOException;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.concurrent.CountDownLatch;
import java.util.jar.JarInputStream;
import java.util.jar.Pack200;

public class PackOutputStream extends OutputStream implements Runnable {

	private static final int PIPE_SIZE = 256 * 1024;
	private OutputStream out;
	private PipedOutputStream pipeOut;
	private PipedInputStream pipeIn;
	private IOException exception;
	private volatile boolean isPackCompleted = false;
	private CountDownLatch initLatch = new CountDownLatch(1);
	private CountDownLatch doneLatch = new CountDownLatch(1);
	
	public PackOutputStream(OutputStream out) {
		this.out = out;
	}
	
	public void run() {
		try {
			pipeIn = new PipedInputStream(this.pipeOut, PIPE_SIZE);
			initLatch.countDown();
			JarInputStream jarIn = new JarInputStream(pipeIn);
			Pack200.newPacker().pack(jarIn, this.out);
			isPackCompleted = true;
			finish(this.out);
			this.out.close();
		} catch (IOException e) {
			e.printStackTrace();
			if(this.exception == null) {
				this.exception = e;
			}
		} finally {
			doneLatch.countDown();
		}
	}
	
	private void init() throws IOException {
		this.pipeOut = new PipedOutputStream();
		
		new Thread(this).start();
		try {
			initLatch.await();
		} catch (InterruptedException e) {}
	}
	
	@Override
	public void write(int b) throws IOException {
		if(this.exception != null) {
			throw this.exception;
		}
		
		if(this.pipeOut == null) {
			init();
		}
		if(!this.isPackCompleted) {
			this.pipeOut.write(b);
			this.pipeOut.flush();
		}
	}
	
	@Override
	public void write(byte[] b) throws IOException {
		if(this.exception != null) {
			throw this.exception;
		}
		
		if(this.pipeOut == null) {
			init();
		}
		if(!this.isPackCompleted) {
			this.pipeOut.write(b);
			this.pipeOut.flush();
		}
	}

	@Override
	public void write(byte[] b, int off, int len) throws IOException {
		if(this.exception != null) {
			throw this.exception;
		}
		
		if(this.pipeOut == null) {
			init();
		}
		if(!isPackCompleted) {
			this.pipeOut.write(b, off, len);
			this.pipeOut.flush();
		}
	}

	@Override
	public void flush() throws IOException {
		if(this.exception != null) {
			throw this.exception;
		}
		
		if(this.pipeOut == null) {
			init();
		}
		this.pipeOut.flush();
	}

	@Override
	public void close() throws IOException {
		if(this.exception != null) {
			throw this.exception;
		}
		
		if(this.pipeOut == null) {
			init();
		}
		this.pipeOut.close();

		try {
			doneLatch.await();
		} catch (InterruptedException e1) {}
	}

	private static boolean finish(OutputStream out) {
		boolean succeeded = false;
		Method finish = null;
		
		try {
			finish = out.getClass().getMethod("finish");
		} catch (NoSuchMethodException e) {
		} catch (SecurityException e) {
		}
		if(finish != null) {
			try {
				finish.invoke(out);
				succeeded = true;
			} catch (IllegalAccessException e) {
			} catch (IllegalArgumentException e) {
			} catch (InvocationTargetException e) {
			}
		}
		return succeeded;
	}
}
