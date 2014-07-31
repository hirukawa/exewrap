import java.io.IOException;
import java.net.URL;

public class URLStreamHandler extends java.net.URLStreamHandler {
	
	protected java.net.URLConnection openConnection(URL url) throws IOException {
		return new URLConnection(url);
	}
}
