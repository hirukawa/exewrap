import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileInputStream;
import java.util.Arrays;
import java.util.jar.JarEntry;
import java.util.jar.JarInputStream;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;
import java.util.jar.Pack200;
import java.util.jar.Pack200.Packer;
import java.util.zip.CRC32;
import java.util.zip.GZIPOutputStream;

public class JarOptimizer extends ClassLoader {
	
	private String relative_classpath = null;
	private ByteArrayOutputStream resource_gz = new ByteArrayOutputStream();
	private ByteArrayOutputStream classes_pack_gz = new ByteArrayOutputStream();
	private String splash_path = null;
	private byte[] splash_image = null;
	
	public JarOptimizer(String filename) throws Exception {
		int resourceCount = 0;
		int classCount = 0;
		ByteArrayOutputStream resources = new ByteArrayOutputStream();
		ByteArrayOutputStream classes = new ByteArrayOutputStream();
		
		JarInputStream in = new JarInputStream(new FileInputStream(filename));
		Manifest manifest = in.getManifest();
		relative_classpath = manifest.getMainAttributes().getValue("Class-Path");
		splash_path = manifest.getMainAttributes().getValue("SplashScreen-Image");
		JarOutputStream classJar = new JarOutputStream(classes, manifest);
		JarOutputStream resourceJar = new JarOutputStream(resources);
		
		JarEntry inEntry;
		byte[] buf = new byte[65536];
		int size;
		while((inEntry = in.getNextJarEntry()) != null) {
			ByteArrayOutputStream baos = new ByteArrayOutputStream();
			while(in.available() > 0) {
				size = in.read(buf);
				if(size > 0) {
					baos.write(buf, 0, size);
				}
			}
			byte[] data = baos.toByteArray();
			if(inEntry.getName().equals(this.splash_path)) {
				this.splash_image = data;
			} else if(inEntry.getName().toLowerCase().endsWith(".class")) {
				CRC32 crc = new CRC32();
				crc.update(data);
				
				JarEntry outEntry = new JarEntry(inEntry.getName());
				outEntry.setMethod(JarEntry.STORED);
				outEntry.setSize(data.length);
				outEntry.setCrc(crc.getValue());
				
				classJar.putNextEntry(outEntry);
				classJar.write(data);
				classJar.closeEntry();
				classCount++;
			} else {
				/*
				String resourceEntryName;
				if(inEntry.getName().length() > 1 && inEntry.getName().charAt(0) == '/' && inEntry.getName().charAt(1) != '/') {
					resourceEntryName = inEntry.getName().substring(1);
				} else {
					resourceEntryName = inEntry.getName();
				}
				*/
				CRC32 crc = new CRC32();
				crc.update(data);
				
				//JarEntry outEntry = new JarEntry(resourceEntryName);
				JarEntry outEntry = new JarEntry(inEntry.getName());
				outEntry.setMethod(JarEntry.STORED);
				outEntry.setSize(data.length);
				outEntry.setCrc(crc.getValue());
				
				resourceJar.putNextEntry(outEntry);
				resourceJar.write(data);
				resourceJar.closeEntry();
				resourceCount++;
			}
			in.closeEntry();
		}
		in.close();
		if(resourceCount > 0) {
			resourceJar.close();
			GZIPOutputStream gzout = new GZIPOutputStream(resource_gz);
			gzout.write(resources.toByteArray());
			gzout.flush();
			gzout.finish();
			gzout.close();
		}
		if(classCount > 0) {
			classJar.close();
			Packer packer = Pack200.newPacker();
			GZIPOutputStream gzout = new GZIPOutputStream(classes_pack_gz);
			in = new JarInputStream(new ByteArrayInputStream(classes.toByteArray()));
			packer.pack(in, gzout);
			gzout.flush();
			gzout.finish();
			gzout.close();
			in.close();
		}
	}
	
	public byte[] getRelativeClassPath() {
		if(this.relative_classpath != null) {
			byte[] buf = this.relative_classpath.replaceAll("/", "\\\\").getBytes();
			return Arrays.copyOf(buf, buf.length + 2);
		}
		return null;
	}
	
	public byte[] getClassesPackGz() {
		if(this.classes_pack_gz.size() > 0) {
			return this.classes_pack_gz.toByteArray();
		}
		return null;
	}
	
	public byte[] getResourcesGz() {
		if(this.resource_gz.size() > 0) {
			return this.resource_gz.toByteArray();
		}
		return null;
	}
	
	public byte[] getSplashPath() {
		if(this.splash_path != null) {
			return this.splash_path.getBytes();
		}
		return null;
	}
	
	public byte[] getSplashImage() {
		return this.splash_image;
	}
}
