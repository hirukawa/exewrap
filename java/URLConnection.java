import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

public class URLConnection extends java.net.URLConnection {
	/* package private */ static final Map resources = new HashMap();
	
	protected URLConnection(URL url) {
		super(url);
	}
	
	public void connect() throws IOException {
	}
	
	public InputStream getInputStream() throws IOException {
		String name = getURL().getFile();
		byte[] buffer = (byte[])resources.get(name);
		if(buffer != null) {
			return new ByteArrayInputStream(buffer);
		}
		return null;
	}
}
