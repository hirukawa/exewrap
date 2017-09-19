package exewrap.core;

import java.io.ByteArrayInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;

public class URLConnection extends java.net.URLConnection {
	
	private byte[] buf;
	private InputStream in;
	
	protected URLConnection(URL url, byte[] buf) {
		super(url);
		this.buf = buf;
	}
	
	public void connect() throws IOException {
		if(this.buf == null) {
			throw new FileNotFoundException("JAR entry " + url);
		}
		if(this.in != null) {
			try { this.in.close(); } catch(Exception ex) {}
		}
		this.in = new ByteArrayInputStream(this.buf);
	}
	
	public InputStream getInputStream() throws IOException {
		if(this.in == null) {
			connect();
		}
		return this.in;
	}
}
