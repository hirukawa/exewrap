package exewrap.core;

import java.io.IOException;
import java.net.URL;

public class URLStreamHandler extends java.net.URLStreamHandler {
	
	private byte[] buf;
	private ClassLoader loader;
	
	public URLStreamHandler(byte[] buf) {
		this.buf = buf;
	}
	
	public URLStreamHandler(ClassLoader loader) {
		this.loader = loader;
	}
	
	protected java.net.URLConnection openConnection(URL url) throws IOException {
		if(this.buf != null) {
			return new URLConnection(url, this.buf);
		} else if(this.loader != null) {
			String s = url.toExternalForm();
			int i = s.toLowerCase().indexOf(".exe!/");
			if(i >= 0) {
				String name = s.substring(i + 6);
				URL resource = loader.getResource(name);
				if(resource != null) {
					return resource.openConnection();
				}
			}
		}
		return new URLConnection(url, null);
	}
}
