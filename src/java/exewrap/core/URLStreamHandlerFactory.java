package exewrap.core;

import java.net.URLStreamHandler;

public class URLStreamHandlerFactory implements java.net.URLStreamHandlerFactory {

	private URLStreamHandler handler;
	
	public URLStreamHandlerFactory(ClassLoader loader) {
		this.handler = new exewrap.core.URLStreamHandler(loader);
	}
	
	@Override
	public URLStreamHandler createURLStreamHandler(String protocol) {
		if ("exewrap".equals(protocol)) {
			return handler;
		}
		return null;
	}
}
