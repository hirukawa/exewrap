import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLStreamHandler;
import java.net.URLStreamHandlerFactory;
import java.security.CodeSource;
import java.security.PermissionCollection;
import java.security.Policy;
import java.security.ProtectionDomain;
import java.security.cert.Certificate;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.Queue;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.jar.Manifest;
import java.util.jar.Attributes.Name;

public class Loader extends ClassLoader {
	
	private static String CONTEXT_PATH;
	private static Map<String, byte[]> classes = new HashMap<String, byte[]>();
	public  static Map<String, byte[]> resources = new HashMap<String, byte[]>();
	private static Queue<JarInputStream> inputs = new LinkedList<JarInputStream>();
	private static JarInputStream jar;
	private static String MAIN_CLASS;
	private static String SPECIFICATION_TITLE;
	private static String SPECIFICATION_VERSION;
	private static String SPECIFICATION_VENDOR;
	private static String IMPLEMENTATION_TITLE;
	private static String IMPLEMENTATION_VERSION;
	private static String IMPLEMENTATION_VENDOR;
	private static boolean SEALED = true;
	private static URL context;
	private static URLStreamHandler handler;

	public static Class<?> initialize(JarInputStream[] jars, URLStreamHandlerFactory factory, String utilities, String mainClassName) throws MalformedURLException, ClassNotFoundException {
		URL.setURLStreamHandlerFactory(factory);
		handler = factory.createURLStreamHandler("exewrap");
		context = new URL("exewrap:" + CONTEXT_PATH + "!/");
		
		for(int i = 0; i < jars.length; i++) {
			JarInputStream jar = jars[i];
			if(jar == null) {
				continue;
			}
			if(i == jars.length - 1) {
				Manifest manifest = jar.getManifest();
				if(manifest != null) {
					MAIN_CLASS = manifest.getMainAttributes().getValue(Name.MAIN_CLASS);
					SPECIFICATION_TITLE = manifest.getMainAttributes().getValue(Name.SPECIFICATION_TITLE);
					SPECIFICATION_VERSION = manifest.getMainAttributes().getValue(Name.SPECIFICATION_VERSION);
					SPECIFICATION_VENDOR = manifest.getMainAttributes().getValue(Name.SPECIFICATION_VENDOR);
					IMPLEMENTATION_TITLE = manifest.getMainAttributes().getValue(Name.IMPLEMENTATION_TITLE);
					IMPLEMENTATION_VERSION = manifest.getMainAttributes().getValue(Name.IMPLEMENTATION_VERSION);
					IMPLEMENTATION_VENDOR = manifest.getMainAttributes().getValue(Name.IMPLEMENTATION_VENDOR);
					SEALED = "true".equalsIgnoreCase(manifest.getMainAttributes().getValue(Name.SEALED));
				}
			}
			inputs.offer(jar);
		}
		jar = inputs.poll();
		
		ClassLoader systemClassLoader = ClassLoader.getSystemClassLoader();
		
		if(utilities != null) {
			if(utilities.contains("UncaughtExceptionHandler;")) {
				Class.forName("exewrap.util.UncaughtExceptionHandler", true, systemClassLoader);
			}
			if(utilities.contains("FileLogStream;")) {
				Class.forName("exewrap.util.FileLogStream", true, systemClassLoader);
			}
			if(utilities.contains("EventLogStream;")) {
				Class.forName("exewrap.util.EventLogStream", true, systemClassLoader);
			}
			if(utilities.contains("EventLogHandler;")) {
				Class.forName("exewrap.util.EventLogHandler", true, systemClassLoader);
			}
			if(utilities.contains("ConsoleOutputStream;")) {
				Class.forName("exewrap.util.ConsoleOutputStream", true, systemClassLoader);
			}
		}
		
		if(mainClassName != null) {
			return Class.forName(mainClassName, true, systemClassLoader);
		} else {
			return Class.forName(MAIN_CLASS, true, systemClassLoader);
		}
	}
	
	
	private ProtectionDomain protectionDomain;

	public Loader(ClassLoader parent) throws MalformedURLException {
		super(parent);
		
		String path = System.getProperty("java.application.path");
		if(path == null) {
			path = "";
		}
		String name = System.getProperty("java.application.name");
		if(name == null) {
			name = "";
		}
		
		CONTEXT_PATH = "file:/" + path.replace('\\', '/') + '/' + name;
		URL url = new URL(CONTEXT_PATH);
		CodeSource codesource = new CodeSource(url, (Certificate[])null);
		PermissionCollection permissions = Policy.getPolicy().getPermissions(new CodeSource(null, (Certificate[])null));
		protectionDomain = new ProtectionDomain(codesource, permissions, this, null);
	}
	
	protected Class<?> findClass(String name) throws ClassNotFoundException {
		String entryName = name.replace('.', '/').concat(".class");
		byte[] bytes = classes.remove(entryName);
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
					SPECIFICATION_TITLE,
					SPECIFICATION_VERSION,
					SPECIFICATION_VENDOR,
					IMPLEMENTATION_TITLE,
					IMPLEMENTATION_VERSION,
					IMPLEMENTATION_VENDOR,
					SEALED ? protectionDomain.getCodeSource().getLocation() : null);
			}
		}
		return defineClass(name, bytes, 0, bytes.length, protectionDomain);
	}
	
	protected URL findResource(String name) {
		byte[] bytes = resources.get(name);
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
			return new URL(context, name, handler);
		} catch (MalformedURLException e) {
			e.printStackTrace();
			return null;
		}
	}

	private byte[] find(String name) throws IOException {
		while(jar != null) {
			JarEntry jarEntry;
			while((jarEntry = jar.getNextJarEntry()) != null) {
				byte[] bytes = readJarEntryBytes(jar);
				resources.put(jarEntry.getName(), bytes);
				if(jarEntry.getName().endsWith(".class")) {
					if(jarEntry.getName().equals(name)) {
						return bytes;
					} else {
						classes.put(jarEntry.getName(), bytes);
					}
				} else {
					if(jarEntry.getName().equals(name)) {
						return bytes;
					}
				}
				jar.closeEntry();
			}
			jar.close();
			jar = inputs.poll();
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