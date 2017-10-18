package exewrap.core;

import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.util.Map;

public class URLStreamHandler extends java.net.URLStreamHandler {
	
	private ClassLoader loader;
	private Map<String, byte[]> resources;
	
	public URLStreamHandler(ClassLoader loader, Map<String, byte[]> resources) {
		this.loader = loader;
		this.resources = resources;
	}
	
	protected java.net.URLConnection openConnection(URL url) throws IOException {
		if(this.loader != null) {
			String s = url.toExternalForm();
			int i = s.toLowerCase().indexOf(".exe!/");
			if(i >= 0) {
				String name = s.substring(i + 6);
				byte[] bytes = resources.get(name);
				if(bytes != null) {
					return new URLConnection(url, bytes);
				}
			}
		}
		return new URLConnection(url, null);
	}
	
	protected java.net.URLConnection openConnection(URL url, Proxy proxy) throws IOException {
		return openConnection(url);
	}
}
