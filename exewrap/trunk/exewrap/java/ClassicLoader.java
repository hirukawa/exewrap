import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.jar.Manifest;

public class ClassicLoader extends ClassLoader {
	
	private Map classes = new HashMap();

	public Class initialize(byte[] jar) throws IOException, ClassNotFoundException {
		JarInputStream jarInputStream;
		ByteArrayOutputStream byteArray = new ByteArrayOutputStream();
		JarEntry entry;
		byte[] buf = new byte[65536];
		int size;
		
		//initialize resources.
		jarInputStream = new JarInputStream(new ByteArrayInputStream(jar));
		while((entry = jarInputStream.getNextJarEntry()) != null) {
			if(!entry.getName().toLowerCase().endsWith(".class")) {
				byteArray.reset();
				while(jarInputStream.available() > 0) {
					size = jarInputStream.read(buf);
					if(size > 0) {
						byteArray.write(buf, 0, size);
					}
				}
				URLConnection.resources.put(entry.getName(), byteArray.toByteArray());
			}
			jarInputStream.closeEntry();
		}
		jarInputStream.close();
		
		//initialize classes.
		jarInputStream = new JarInputStream(new ByteArrayInputStream(jar));
		while((entry = jarInputStream.getNextJarEntry()) != null) {
			if(entry.getName().toLowerCase().endsWith(".class")) {
				String className = entry.getName().substring(0, entry.getName().length() - 6).replace('/', '.');
				byteArray.reset();
				while(jarInputStream.available() > 0) {
					size = jarInputStream.read(buf);
					if(size > 0) {
						byteArray.write(buf, 0, size);
					}
				}
				this.classes.put(className, byteArray.toByteArray());
			}
			jarInputStream.closeEntry();
		}
		
		//find main class.
		Manifest manifest = jarInputStream.getManifest();
		jarInputStream.close();
		if(manifest != null) {
			String mainClassName = manifest.getMainAttributes().getValue("Main-Class");
			if(mainClassName != null) {
				Thread.currentThread().setContextClassLoader(this);
				return Class.forName(mainClassName, true, this);
			}
		}
		return null;
	}
	
	protected Class findClass(String name) throws ClassNotFoundException {
		byte[] buffer = (byte[])this.classes.get(name);
		if(buffer != null) {
			if(name.indexOf('.') >= 0) {
				String packageName = name.substring(0, name.lastIndexOf('.'));
				if(getPackage(packageName) == null) {
					definePackage(packageName, null, null, null, null, null, null, null);
				}
			}
			return 	defineClass(name, buffer, 0, buffer.length);
		}
		throw new ClassNotFoundException(name);
	}
	
	public URL getResource(String name) {
		try {
			if(URLConnection.resources.containsKey(name)) {
				return new URL("exewrap", "127.0.0.1", -1, name, new URLStreamHandler());
			}
		} catch (MalformedURLException e) {}
		return super.getResource(name);
	}
}
