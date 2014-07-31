import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;

public class FileLogStream extends OutputStream {
	static private String filename = null;
	static private FileOutputStream out = null;
	
	public static void initialize(String moduleName) throws FileNotFoundException {
		File path = new File(moduleName);
		String name = path.getName().substring(0, path.getName().length() - 4) + ".log";
		if(!path.getParent().endsWith(":\\") && new File(path.getParentFile().getParent() + "\\log").exists()) {
			filename = path.getParentFile().getParent() + "\\log\\" + name;
		}
		if(filename == null && new File(path.getParent() + "\\log").exists()) {
			filename = path.getParent() + "\\log\\" + name;
		}
		if(filename == null) {
			filename = path.getParent() + "\\" + name;
		}
		FileLogStream stream = new FileLogStream();
		System.setOut(new PrintStream(stream));
		System.setErr(new PrintStream(stream));
	}
	
	private void open() {
		try {
			out = new FileOutputStream(filename);
		} catch(FileNotFoundException e) {}
	}
	
	public void close() throws IOException {
		if(out != null) {
			out.close();
		}
	}
	
	public void flush() throws IOException {
	}
	
	public void write(byte[] b, int off, int len) throws IOException {
		if(out == null) open();
		out.write(b, off, len);
		out.flush();
	}
	
	public void write(byte[] b) throws IOException {
		if(out == null) open();
		out.write(b);
		out.flush();
	}
	
	public void write(int b) throws IOException {
		if(out == null) open();
		out.write(b);
		out.flush();
	}
}
