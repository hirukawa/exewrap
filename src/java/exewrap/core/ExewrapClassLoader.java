package exewrap.core;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.lang.reflect.Field;
import java.net.MalformedURLException;
import java.net.URL;
import java.security.CodeSource;
import java.security.PermissionCollection;
import java.security.Policy;
import java.security.ProtectionDomain;
import java.security.cert.Certificate;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.Queue;
import java.util.jar.Attributes.Name;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.jar.Manifest;

public class ExewrapClassLoader extends ClassLoader {
	
	private static Map<String, byte[]> classes = new HashMap<String, byte[]>();
	private static Map<String, byte[]> resources = new HashMap<String, byte[]>();
	private static Queue<JarInputStream> inputs = new LinkedList<JarInputStream>();
	private static JarInputStream in;
	private static String mainClassName;
	private static String specTitle;
	private static String specVersion;
	private static String specVendor;
	private static String implTitle;
	private static String implVersion;
	private static String implVendor;
	private static URL context;
	private static ProtectionDomain protectionDomain;

	public static void setInputs(JarInputStream[] jars) throws MalformedURLException {
		for(JarInputStream jar : jars) {
			if(jar == null) {
				continue;
			}
			Manifest manifest = jar.getManifest();
			if(manifest != null) {
				mainClassName = manifest.getMainAttributes().getValue(Name.MAIN_CLASS);
				specTitle = manifest.getMainAttributes().getValue(Name.SPECIFICATION_TITLE);
				specVersion = manifest.getMainAttributes().getValue(Name.SPECIFICATION_VERSION);
				specVendor = manifest.getMainAttributes().getValue(Name.SPECIFICATION_VENDOR);
				implTitle = manifest.getMainAttributes().getValue(Name.IMPLEMENTATION_TITLE);
				implVersion = manifest.getMainAttributes().getValue(Name.IMPLEMENTATION_VERSION);
				implVendor = manifest.getMainAttributes().getValue(Name.IMPLEMENTATION_VENDOR);
			}
			inputs.offer(jar);
		}
		in = inputs.poll();
		
		String path = System.getProperty("java.application.path");
		if(path == null) {
			path = "";
		}
		String name = System.getProperty("java.application.name");
		if(name == null) {
			name = "";
		}
		
		ClassLoader systemClassLoader = ClassLoader.getSystemClassLoader();
		URL.setURLStreamHandlerFactory(new URLStreamHandlerFactory(systemClassLoader));
		context = new URL("exewrap:file:/" + path.replace('\\', '/') + '/' + name + "!/");

		URL url = new URL("file:/" + path.replace('\\', '/') + '/' + name);
		CodeSource codesource = new CodeSource(url, (Certificate[])null);
		PermissionCollection permissions = Policy.getPolicy().getPermissions(new CodeSource(null, (Certificate[])null));
		protectionDomain = new ProtectionDomain(codesource, permissions, systemClassLoader, null);
	}
	
	public static void loadUtilities(String utilities) throws ClassNotFoundException {
		if(utilities == null) {
			return;
		}
		if(utilities.contains("UncaughtExceptionHandler;")) {
			Class.forName("exewrap.util.UncaughtExceptionHandler");
		}
		if(utilities.contains("FileLogStream;")) {
			Class.forName("exewrap.util.FileLogStream");
		}
		if(utilities.contains("EventLogStream;")) {
			Class.forName("exewrap.util.EventLogStream");
		}
		if(utilities.contains("EventLogHandler;")) {
			Class.forName("exewrap.util.EventLogHandler");
		}
		if(utilities.contains("ConsoleOutputStream;")) {
			Class.forName("exewrap.util.ConsoleOutputStream");
		}
	}
	
	public static Class<?> getMainClass(String mainClassName) throws ClassNotFoundException {
		ClassLoader systemClassLoader = ClassLoader.getSystemClassLoader();
		
		if(mainClassName != null) {
			return Class.forName(mainClassName, true, systemClassLoader);
		}
		if(ExewrapClassLoader.mainClassName != null) {
			return Class.forName(ExewrapClassLoader.mainClassName, true, systemClassLoader);
		}
		return null;
	}

	public static void setSplashScreenResource(String name, byte[] image) {
		resources.put(name, image);
	}
	
	public static native void WriteConsole(byte[] b, int off, int len);
	public static native void WriteEventLog(int type, String message);
	public static native void UncaughtException(String thread, String message, String trace);
	public static native String SetEnvironment(String key, String value);

	
	public ExewrapClassLoader(ClassLoader parent) {
		super(parent);
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
				definePackage(packageName,
					specTitle, specVersion, specVendor,
					implTitle, implVersion, implVendor,
					null);
			}
		}
		return defineClass(name, bytes, 0, bytes.length, protectionDomain);
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
}
