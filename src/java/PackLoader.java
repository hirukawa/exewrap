import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;
import java.util.jar.Pack200;
import java.util.jar.Pack200.Unpacker;
import java.util.zip.GZIPInputStream;

public class PackLoader extends ClassLoader {
	
	private Map classes = new HashMap();
	
	public Class initialize(byte[] classesPackGz, byte[] resourcesGz, byte[] splashPath, byte[] splashImage) throws IOException, ClassNotFoundException {
		ByteArrayOutputStream byteArray = new ByteArrayOutputStream();
		JarEntry entry;
		byte[] buf = new byte[65536];
		int size;
		
		//initialize resources.
		if(resourcesGz != null) {
			JarInputStream resourcesJarInputStream = new JarInputStream(new GZIPInputStream(new ByteArrayInputStream(resourcesGz)));
			while((entry = resourcesJarInputStream.getNextJarEntry()) != null) {
				byteArray.reset();
				while(resourcesJarInputStream.available() > 0) {
					size = resourcesJarInputStream.read(buf);
					if(size > 0) {	
						byteArray.write(buf, 0, size);
					}
				}
				URLConnection.resources.put(entry.getName(), byteArray.toByteArray());
				resourcesJarInputStream.closeEntry();
			}
			resourcesJarInputStream.close();
		}
		
		//add splash image.
		if(splashPath != null && splashImage != null) {
			URLConnection.resources.put(new String(splashPath), splashImage);
		}
		
		//initialize classes.
		InputStream classesPackGzInputStream = new GZIPInputStream(new ByteArrayInputStream(classesPackGz));
		ByteArrayOutputStream classesJar = new ByteArrayOutputStream();
		JarOutputStream classesJarOutputStream = new JarOutputStream(classesJar);
		Unpacker unpacker = Pack200.newUnpacker();
		unpacker.unpack(classesPackGzInputStream, classesJarOutputStream);
		classesPackGzInputStream.close();
		classesJarOutputStream.close();
		classesJar.close();
		JarInputStream classesJarInputStream = new JarInputStream(new ByteArrayInputStream(classesJar.toByteArray()));
		while((entry = classesJarInputStream.getNextJarEntry()) != null) {
			String className = entry.getName().substring(0, entry.getName().length() - 6).replace('/', '.');
			byteArray.reset();
			while(classesJarInputStream.available() > 0) {
				size = classesJarInputStream.read(buf);
				if(size > 0) {
					byteArray.write(buf, 0, size);
				}
			}
			this.classes.put(className, byteArray.toByteArray());
			classesJarInputStream.closeEntry();
		}

		//find main class.
		Manifest manifest = classesJarInputStream.getManifest();
		classesJarInputStream.close();
		if(manifest != null) {
			String mainClassName = classesJarInputStream.getManifest().getMainAttributes().getValue("Main-Class");
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
				return new URL("exewrap", "", -1, name, new URLStreamHandler());
			}
		} catch (MalformedURLException e) {}
		return super.getResource(name);
	}
}
