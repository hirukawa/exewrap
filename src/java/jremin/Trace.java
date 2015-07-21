package jremin;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Trace {
	
	private static Pattern LOADED_CLASS = Pattern.compile("Loaded (\\S+)", Pattern.CASE_INSENSITIVE);
	private static Pattern LOAD_LIBRARY = Pattern.compile("\\[LoadLibrary (.+)\\]", Pattern.CASE_INSENSITIVE);
	private static Pattern CREATE_FILE = Pattern.compile("\\[CreateFile (.+)\\]", Pattern.CASE_INSENSITIVE);
	
	private File jre = null;
	private Set<String> jars = null;
	private Set<String> files = new HashSet<String>();
	private Set<String> packages = new HashSet<String>();
	private Set<String> requiredClassEntryNames = new HashSet<String>();
	private boolean useAwt;
	private boolean useSwing;
	
	public Trace(File file) throws IOException {
		file = file.getCanonicalFile();
		String s = file.getName();
		int i = s.toUpperCase().indexOf(".TRACE");
		if(i >= 0) {
			s = s.substring(0, i);
		}
		final String basename = s;
		File dir = file.getParentFile();
		File[] files = dir.listFiles(new FilenameFilter() {
			@Override
			public boolean accept(File dir, String name) {
				return name.startsWith(basename) && name.toUpperCase().contains(".TRACE") && name.toLowerCase().endsWith(".log");
			}
		});
		for(File f : files) {
			BufferedReader r = new BufferedReader(new InputStreamReader(new FileInputStream(f), Charset.forName("MS932")));
			String line;
			while((line = r.readLine()) != null) {
				Matcher m;
				m = LOADED_CLASS.matcher(line);
				if(m.find()) {
					String cls = m.group(1);
					if(!this.useAwt && (cls.startsWith("com.sun.awt.") || cls.startsWith("java.awt."))) {
						this.useAwt = true;
					}
					if(!this.useSwing && (cls.startsWith("com.sun.swing.") || cls.startsWith("javax.swing."))) {
						this.useSwing = true;
					}
					if(cls.startsWith("exewrap.")) {
						continue;
					}
					this.requiredClassEntryNames.add(cls.replace('.', '/') + ".class");
					continue;
				}
				m = LOAD_LIBRARY.matcher(line);
				if(m.find()) {
					this.files.add(m.group(1));
					continue;
				}
				m = CREATE_FILE.matcher(line);
				if(m.find()) {
					this.files.add(m.group(1));
					continue;
				}
			}
			r.close();
		}
		for(String cls : this.requiredClassEntryNames) {
			int j = cls.lastIndexOf('/');
			if(j >= 0) {
				this.packages.add(cls.substring(0, j));
			}
		}
	}

	public File getJRE() {
		if(jre == null) {
			for(String s : files) {
				File f = new File(s);
				if(f.exists() && (f.getName().equalsIgnoreCase("jvm.dll") || f.getName().equalsIgnoreCase("rt.jar"))) {
					File dir = f.getParentFile();
					while(dir != null) {
						if(dir.getName().equalsIgnoreCase("bin") || dir.getName().equalsIgnoreCase("lib")) {
							jre = dir.getParentFile();
							return jre;
						}
						dir = dir.getParentFile();
					}
				}
			}
		}
		return jre;
	}
	
	public Set<String> getUsedJars() throws IOException {
		if(jars == null) {
			jars = new HashSet<String>();
			List<File> list = new ArrayList<File>();
			traverseJars(list, getJRE());
			for(File jar : list) {
				boolean used = false;
				JarInputStream jarIn = new JarInputStream(new FileInputStream(jar));
				JarEntry entry;
				while((entry = jarIn.getNextJarEntry()) != null) {
					if(requiredClassEntryNames.contains(entry.getName())) {
						used = true;
						break;
					}
				}
				jarIn.close();
				if(used) {
					jars.add(jar.getCanonicalPath());
				}
			}
		}
		return jars; 
	}
	
	private void traverseJars(List<File> list, File dir) throws IOException {
		for(File file : dir.listFiles()) {
			if(file.isDirectory()) {
				traverseJars(list, file);
			} else {
				if(file.getName().toLowerCase().endsWith(".jar")) {
					list.add(file);
				};
			}
		}
	}
	
	public Set<String> getRequiredClassEntryNames() {
		return this.requiredClassEntryNames;
	}
	
	public Set<String> getPackages() {
		return this.packages;
	}
	
	public Set<String> getFiles() {
		return this.files;
	}
	
	public boolean useAwt() {
		return this.useAwt;
	}
	
	public boolean useSwing() {
		return this.useSwing;
	}
}