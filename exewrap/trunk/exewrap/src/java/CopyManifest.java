import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class CopyManifest {
	
	private static Pattern pattern = Pattern.compile("<requestedExecutionLevel\\s+.*level=([^\\s/>]+)", Pattern.MULTILINE | Pattern.DOTALL);
	
	public static void main(String[] args) throws IOException {
		if(args.length == 0) {
			return;
		}
		
		Path input = Paths.get(args[0]);
		if(!Files.exists(input)) {
			return;
		}
		
		String manifest = new String(Files.readAllBytes(input), StandardCharsets.UTF_8);
		Files.write(input.resolveSibling(input.getFileName().toString() + ".asInvoker"), replace(manifest, "asInvoker"));
		Files.write(input.resolveSibling(input.getFileName().toString() + ".highestAvailable"), replace(manifest, "highestAvailable"));
		Files.write(input.resolveSibling(input.getFileName().toString() + ".requireAdministrator"), replace(manifest, "requireAdministrator"));
	}
	
	private static byte[] replace(String manifest, String executionLevel) {
		StringBuffer sb = new StringBuffer();
		Matcher m = pattern.matcher(manifest);
		while(m.find()) {
			m.appendReplacement(sb, m.group(0).replace(m.group(1), "'" + executionLevel + "'"));
		}
		m.appendTail(sb);
		return sb.toString().getBytes(StandardCharsets.UTF_8);
	}
}
