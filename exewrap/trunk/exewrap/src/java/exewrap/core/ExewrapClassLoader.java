package exewrap.core;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.lang.reflect.Field;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.Queue;
import java.util.jar.Attributes.Name;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.jar.Manifest;

public class ExewrapClassLoader extends ClassLoader {
	
	private Map<String, byte[]> classes = new HashMap<String, byte[]>();
	private Map<String, byte[]> resources = new HashMap<String, byte[]>();
	private Queue<JarInputStream> inputs = new LinkedList<JarInputStream>();
	private JarInputStream in;
	private String mainClassName;
	private URL context;
	
	public ExewrapClassLoader(ClassLoader parent, JarInputStream[] inputs) throws MalformedURLException {
		super(parent);
		for(JarInputStream in : inputs) {
			Manifest manifest = in.getManifest();
			if(manifest != null) {
				this.mainClassName = manifest.getMainAttributes().getValue(Name.MAIN_CLASS);
			}
			this.inputs.offer(in);
		}
		this.in = this.inputs.poll();
		
		String path = System.getProperty("java.application.path");
		if(path == null) {
			path = "";
		}
		String name = System.getProperty("java.application.name");
		if(name == null) {
			name = "";
		}
		
		this.context = new URL("jar:file:/" + path.replace('\\', '/') + '/' + name + "!/");
	}
	
	public void registerSystemClassLoader() throws NoSuchFieldException, SecurityException, IllegalArgumentException, IllegalAccessException {
		Field scl = ClassLoader.class.getDeclaredField("scl");
		scl.setAccessible(true);
		scl.set(null, this);
	}
	
	public void loadUtilities(String utilities) throws ClassNotFoundException {
		if(utilities == null) {
			return;
		}
		if(utilities.contains("UncaughtExceptionHandler;")) {
			Class.forName("exewrap.util.UncaughtExceptionHandler", true, this);
		}
		if(utilities.contains("FileLogStream;")) {
			Class.forName("exewrap.util.FileLogStream", true, this);
		}
		if(utilities.contains("EventLogStream;")) {
			Class.forName("exewrap.util.EventLogStream", true, this);
		}
		if(utilities.contains("EventLogHandler;")) {
			Class.forName("exewrap.util.EventLogHandler", true, this);
		}
	}
	
	public Class<?> getMainClass(String mainClassName) throws ClassNotFoundException {
		if(mainClassName != null) {
			return loadClass(mainClassName);
		}
		if(this.mainClassName != null) {
			return loadClass(this.mainClassName);
		}
		return null;
	}

	public void setSplashScreenResource(String name, byte[] image) {
		this.resources.put(name, image);
	}
	
	protected Class<?> findClass(String name) throws ClassNotFoundException {
		String entryName = name.replace('.', '/') + ".class";
		byte[] bytes = this.classes.remove(entryName);
		if(bytes == null) {
			try {
				bytes = find(entryName);
			} catch(IOException ex) {
				throw new ClassNotFoundException(name, ex);
			}
		}
		if(bytes == null) {
			throw new ClassNotFoundException(name);
		}
		
		if(name.indexOf('.') >= 0) {
			String packageName = name.substring(0, name.lastIndexOf('.'));
			if(getPackage(packageName) == null) {
				definePackage(packageName, null, null, null, null, null, null, null);
			}
		}
		return defineClass(name, bytes, 0, bytes.length);
	}
	
	protected URL findResource(String name) {
		byte[] bytes = this.resources.get(name);
		if(bytes == null) {
			try {
				bytes = find(name);
			} catch (IOException ex) {
				throw new RuntimeException(ex);
			}
		}
		if(bytes == null) {
			return null;
		}
		
		try {
			return new URL(this.context, name, new URLStreamHandler(bytes));
		} catch (MalformedURLException e) {
			e.printStackTrace();
			return null;
		}
	}

	private byte[] find(String name) throws IOException {
		while(this.in != null) {
			JarEntry jarEntry;
			while((jarEntry = this.in.getNextJarEntry()) != null) {
				byte[] bytes = readJarEntryBytes(in);
				this.resources.put(jarEntry.getName(), bytes);
				if(jarEntry.getName().endsWith(".class")) {
					if(jarEntry.getName().equals(name)) {
						return bytes;
					} else {
						this.classes.put(jarEntry.getName(), bytes);
					}
				} else {
					if(jarEntry.getName().equals(name)) {
						return bytes;
					}
				}
				in.closeEntry();
			}
			this.in.close();
			this.in = this.inputs.poll();
		}
		return null;
	}
	
	private byte[] readJarEntryBytes(JarInputStream in) throws IOException {
		ByteArrayOutputStream buf = new ByteArrayOutputStream();
		byte[] bytes = new byte[65536];
		int len = 0;
		while(len != -1) {
			len = in.read(bytes);
			if(len > 0) {
				buf.write(bytes, 0, len);
			}
		}
		return buf.toByteArray();
	}

	public static native void WriteEventLog(int type, String message);
	public static native void UncaughtException(String thread, String message, String trace);
}
