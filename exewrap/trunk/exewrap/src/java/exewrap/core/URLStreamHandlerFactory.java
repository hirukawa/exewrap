package exewrap.core;

import java.net.URLStreamHandler;
import java.util.Map;

public class URLStreamHandlerFactory implements java.net.URLStreamHandlerFactory {

	private URLStreamHandler handler;
	
	public URLStreamHandlerFactory(ClassLoader loader, Map<String, byte[]> resources) {
		this.handler = new exewrap.core.URLStreamHandler(loader, resources);
	}
	
	@Override
	public URLStreamHandler createURLStreamHandler(String protocol) {
		if ("exewrap".equals(protocol)) {
			return handler;
		}
		return null;
	}
}
