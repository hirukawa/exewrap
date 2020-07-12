import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.net.URLEncoder;
import java.net.URLStreamHandler;
import java.net.URLStreamHandlerFactory;
import java.security.CodeSource;
import java.security.PermissionCollection;
import java.security.Policy;
import java.security.ProtectionDomain;
import java.security.cert.Certificate;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.Map;
import java.util.Queue;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.jar.Manifest;
import java.util.jar.Attributes.Name;

public class Loader extends URLClassLoader {
	
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

	public static Class<?> initialize(JarInputStream[] jars, URLStreamHandlerFactory factory, String utilities, String classPath, String mainClassName, int consoleCodePage) throws MalformedURLException, ClassNotFoundException {
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
			if(System.getProperty("exewrap.console.encoding") == null)
			{
				if(consoleCodePage == 65001) {
					System.setProperty("exewrap.console.encoding", "UTF-8");
				}
			}
			if(System.getProperty("exewrap.log.encoding") == null)
			{
				if(consoleCodePage == 65001) {
					System.setProperty("exewrap.log.encoding", "UTF-8");
				}
			}
			if(utilities.contains("ENCODING-FIX;")) {
				String encoding = System.getProperty("exewrap.console.encoding");
				if(encoding != null) {
					try {
					    System.setOut(new PrintStream(new FileOutputStream(FileDescriptor.out), true, encoding));
					    System.setErr(new PrintStream(new FileOutputStream(FileDescriptor.err), true, encoding));
					} catch(UnsupportedEncodingException ignore) {}
				}
			}
			if(utilities.contains("UncaughtExceptionHandler;")) {
				Class.forName("exewrap.util.UncaughtExceptionHandler", true, systemClassLoader);
			}
			if(utilities.contains("FileLogStream;")) {
				Class.forName("exewrap.util.FileLogStream", true, systemClassLoader);
			}
			if(utilities.contains("EventLogHandler;")) {
				Class.forName("exewrap.util.EventLogHandler", true, systemClassLoader);
			}
		}

		if(systemClassLoader instanceof Loader) {
			Loader loader = (Loader)systemClassLoader;

			try {
				// カレントディレクトリをCLASS_PATHに追加します。
				classPath = new File(".").getCanonicalPath() + ";" + classPath;
			} catch(Exception ignore) {}

			Set<String> paths = new HashSet<String>();
			for(String path : classPath.split(";")) {
				try {
					if(path.length() > 0) {
						File file = new File(path).getCanonicalFile();
						String s = file.toString().toLowerCase();
						if(!paths.contains(s)) {
							URL url = file.toURI().toURL();
							loader.addURL(url);
							paths.add(s);
						}
					}
				} catch(Exception ignore) {}
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
		super(new URL[0], parent);
		
		String path = System.getProperty("java.application.path");
		if(path == null) {
			path = "";
		}
		String name = System.getProperty("java.application.name");
		if(name == null) {
			name = "";
		}

		try {
			String s = path.replace('\\', '/') + '/' + name;
			String urlEncoded = URLEncoder.encode(s.replace(":", "*:").replace("/", "*/").replace(" ", "*|"), "UTF-8")
					.replace("*%3A", ":").replace("*%2F", "/").replace("*%7C", "%20");
			CONTEXT_PATH = "file:/" + urlEncoded;
		} catch(UnsupportedEncodingException e) {
			CONTEXT_PATH = "file:/" + path.replace('\\', '/') + '/' + name;
		}
		URL url = new URL(CONTEXT_PATH);
		CodeSource codesource = new CodeSource(url, (Certificate[])null);
		PermissionCollection permissions = Policy.getPolicy().getPermissions(new CodeSource(null, (Certificate[])null));
		protectionDomain = new ProtectionDomain(codesource, permissions, this, null);
	}
	
	protected Class<?> findClass(String name) throws ClassNotFoundException {
		// 外部ライブラリ（JAR）からクラスを探します。見つからなければEXEリソースからクラスを探します。
		try {
			return super.findClass(name);
		} catch(ClassNotFoundException ignore) {}

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
	
	public URL findResource(String name) {
		// 外部ライブラリ（JAR）からリソースを探します。見つからなければEXEリソースからリソースを探します。
		URL url = super.findResource(name);
		if(url != null) {
			return url;
		}

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