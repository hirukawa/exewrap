package jremin;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.channels.FileChannel;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.jar.JarOutputStream;

public class JreMin {
	
	private static Set<String> excluded = new HashSet<String>(Arrays.asList(new String[] {
		"classes.jsa",
		"meta-index"
	}));
	
	private static Set<String> requiredFiles = new HashSet<String>(Arrays.asList(new String[] {
		"copyright",
		"license",
		"readme.txt",
		"thirdpartylicensereadme.txt",
		"thirdpartylicensereadme-javafx.txt",
		"welcome.html"
	}));

	private static Set<String> fonts = new HashSet<String>(Arrays.asList(new String[] {
		"lucidabrightdemibold.ttf",
		"lucidabrightdemiitalic.ttf",
		"lucidabrightitalic.ttf",
		"lucidabrightregular.ttf",
		"lucidasansdemibold.ttf",
		"lucidasansregular.ttf",
		"lucidatypewriterbold.ttf",
		"lucidatypewriterregular.ttf"
	}));
	
	private static Map<String, Set<String>> map = new HashMap<String, Set<String>>();
	
	private static Set<String> requiredPackages = new HashSet<String>(Arrays.asList(new String[] {
		"java/lang/*"
	}));
	
	public static void main(String[] args) throws Exception {
		if(args.length == 0) {
			showUsage();
			return;
		}
		
		map.put("awt", new HashSet<String>(Arrays.asList(new String[] {
			"com/sun/awt/*",
			"java/text/*",
			"java/awt/*",
			"sun/awt/*",
			"sun/dc/*",
			"sun/font/*",
			"sun/util/locale/*",
			//--------
			"java/util",
			"sun/util",
			"sun/util/calendar"
		})));
		
		map.put("swing", new HashSet<String>(Arrays.asList(new String[] {
			"com/sun/beans/util/*",
			"com/sun/java/swing/*",
			"com/sun/media/*",
			"com/sun/swing/*",
			"java/applet/*",
			"java/awt/*",
			"java/beans/*",
			"java/io/*",
			"java/math/*",
			"java/net/*",
			"java/nio/*",
			"java/security/*",
			"java/text/*",
			"java/util/*",
			"javax/accessibility/*",
			"javax/print/*",
			"javax/sound/*",
			"javax/swing/*",
			"jdk/internal/org/objectweb/asm/*",
			"sun/awt/*",
			"sun/dc/*",
			"sun/font/*",
			"sun/invoke/*",
			"sun/io/*",
			"sun/java2d/*",
			"sun/misc/*",
			"sun/net/*",
			"sun/nio/*",
			"sun/print/*",
			"sun/reflect/*",
			"sun/security/*",
			"sun/swing/*",
			"sun/text/*",
			"sun/util/*"
		})));
		
		String filename = args[0];
		
		boolean debug = false;
		if(args.length >= 2 && args[0].equals("-d")) {
			debug = true;
			filename = args[1];
		}
		
		if(!new File(filename).exists()) {
			System.out.println();
		}
		
		Trace trace = new Trace(new File(filename));
		if(debug) {
			debugOut(trace);
		}
		
		File jre = trace.getJRE();
		if(jre == null) {
			System.out.println("JRE not found.");
			return;
		}
		File target = new File(jre.getName() + "_min");

		Set<String> appends = new HashSet<String>();
		
		if(trace.useAwt()) {
			for(String pkg : map.get("awt")) {
				requiredPackages.add(pkg);
			}
			appends.addAll(fonts);
		}
		if(trace.useSwing()) {
			for(String pkg : map.get("swing")) {
				requiredPackages.add(pkg);
			}
			appends.addAll(fonts);
		}
		
		Set<File> files = new HashSet<File>();
		for(File file : getRequiredFiles(jre, appends)) {
			files.add(file);
		}
		for(String s : trace.getFiles()) {
			File file = new File(s).getCanonicalFile();
			files.add(file);
		}
		for(String s : trace.getUsedJars()) {
			File file = new File(s).getCanonicalFile();
			files.add(file);
		}
		
		Set<String> requiredClassEntryNames = getRequiredClassEntryNames();
		for(String name : trace.getRequiredClassEntryNames()) {
			requiredClassEntryNames.add(name);
		}
		
		for(File file : files) {
			if(!file.exists()) {
				continue;
			}
			if(!file.getPath().toLowerCase().startsWith(jre.getPath().toLowerCase())) {
				continue;
			}
			if(excluded.contains(file.getName().toLowerCase())) {
				continue;
			}
			File dst = new File(target, file.getPath().substring(jre.getPath().length()));
			File dir = dst.getParentFile();
			if(!dir.exists()) {
				dir.mkdirs();
			}
			System.out.println(dst.getPath());
			if(file.getPath().toLowerCase().endsWith(".jar")) {
				copyJAR(file, dst, requiredClassEntryNames, requiredPackages);
			} else {
				copy(file, dst);
			}
		}
	}
	
	public static void showUsage() {
		System.out.println(
			"jremin 1.1.0\r\n" + 
			"Java Runtime Environment (JRE) minimizer.\r\n" + 
			"Copyright (C) 2015 HIRUKAWA Ryo. All rights reserved.\r\n" +
			"\r\n" + 
			"Usage: jremin.exe <trace-file>\r\n"
		);
	}
	
	public static void copy(File src, File dst) throws IOException {
		FileChannel sc = null;
		FileChannel dc = null;
		try {
			sc = new FileInputStream(src).getChannel();
			dc = new FileOutputStream(dst).getChannel();
			dc.transferFrom(sc, 0, sc.size());
		} finally {
			if (dc != null) try { dc.close(); } catch (Exception e) {}
			if (sc != null) try { sc.close(); } catch (Exception e) {}
		}
	}
	
	public static void copyJAR(File src, File dst, Set<String> requiredClassEntryNames, Set<String> requirePackages) throws FileNotFoundException, IOException {
		JarInputStream jarIn = new JarInputStream(new FileInputStream(src));
		JarOutputStream jarOut = new JarOutputStream(new FileOutputStream(dst), jarIn.getManifest());
		JarEntry entryIn;
		while((entryIn = jarIn.getNextJarEntry()) != null) {
			String name = entryIn.getName();
			String pkg = null;
			int i = name.lastIndexOf('/');
			if(i > 0) {
				pkg = name.substring(0, i);
			}
			boolean required = false;;
			for(String p : requirePackages) {
				if(p.endsWith("/*")) {
					p = p.substring(0, p.length() - 2);
					if(pkg.startsWith(p)) {
						required = true;
						break;
					}
				} else if(p.equals(pkg)) {
					required = true;
					break;
				}
			}
			
			if(!required && name.endsWith(".class") && !requiredClassEntryNames.contains(name)) {
				continue;
			}
			JarEntry entryOut = new JarEntry(name);
			entryOut.setMethod(JarEntry.DEFLATED);
			jarOut.putNextEntry(entryOut);
			byte[] data = getBytes(jarIn);
			jarIn.closeEntry();
			jarOut.write(data);
			jarOut.flush();
			jarOut.closeEntry();
		}
		jarIn.close();
		jarOut.flush();
		jarOut.finish();
		jarOut.close();
	}

	private static byte[] getBytes(InputStream in) throws IOException {
		ByteArrayOutputStream out = new ByteArrayOutputStream();
		byte[] buf = new byte[65536];
		int size;
		while((size = in.read(buf)) != -1) {
			out.write(buf, 0, size);
		}
		return out.toByteArray();
	}
	
	public static List<File> getRequiredFiles(File jre, Set<String> appends) {
		List<File> list = new ArrayList<File>();
		traverseRequiredFiles(list, jre, appends);
		return list;
	}
	
	private static void traverseRequiredFiles(List<File> list, File dir, Set<String> appends) {
		for(File file : dir.listFiles()) {
			if(file.isDirectory()) {
				traverseRequiredFiles(list, file, appends);
			} else {
				if(appends != null && appends.contains(file.getName().toLowerCase())) {
					list.add(file);
					continue;
				}
				if(requiredFiles.contains(file.getName().toLowerCase())) {
					list.add(file);
					continue;
				}
			}
		}
	}
	
	public static Set<String> getRequiredClassEntryNames() throws IOException {
		Set<String> set = new HashSet<String>();
		InputStream in = JreMin.class.getResourceAsStream("classes.txt");
		if(in != null) {
			BufferedReader r = new BufferedReader(new InputStreamReader(in, Charset.forName("UTF-8")));
			String line;
			while((line = r.readLine()) != null) {
				String name = line.replace('.', '/') + ".class";
				set.add(name);
			}
			r.close();
		}
		in.close();
		return set;
	}
	
	public static void debugOut(Trace trace) throws Exception {
		Writer w;
		List<String> list;
		
		w = new BufferedWriter(new OutputStreamWriter(new FileOutputStream("classes.txt"), Charset.forName("UTF-8")));
		list = new ArrayList<String>();
		for(String s : trace.getRequiredClassEntryNames()) {
			if(s.endsWith(".class")) {
				list.add(s.substring(0, s.length() - 6).replace('/', '.'));
			}
		};
		Collections.sort(list);
		for(String line : list) {
			w.write(line);
			w.write("\r\n");
		}
		w.close();
		
		w = new BufferedWriter(new OutputStreamWriter(new FileOutputStream("packages.txt"), Charset.forName("UTF-8")));
		list = new ArrayList<String>();
		for(String s : trace.getPackages()) {
			list.add(s.replace('/', '.'));
		};
		Collections.sort(list);
		for(String line : list) {
			w.write(line);
			w.write("\r\n");
		}
		w.close();
	}
}
