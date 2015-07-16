package exewrap.core;

import java.io.IOException;
import java.net.URL;

public class URLStreamHandler extends java.net.URLStreamHandler {
	
	private byte[] buf;
	
	public URLStreamHandler(byte[] buf) {
		this.buf = buf;
	}
	
	protected java.net.URLConnection openConnection(URL url) throws IOException {
		return new URLConnection(url, this.buf);
	}
}
