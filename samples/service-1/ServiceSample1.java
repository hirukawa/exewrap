import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.concurrent.atomic.AtomicBoolean;

public class ServiceSample1 {

	private static AtomicBoolean isStopRequested = new AtomicBoolean(false);
	private static ServerSocket serverSocket;

	public static void start(String[] args) throws IOException {
		serverSocket = new ServerSocket(80, 50, InetAddress.getLoopbackAddress());
		for(;;) {
			Socket socket;
			try {
				socket = serverSocket.accept();
			} catch(SocketException e) {
				if(isStopRequested.get()) {
					return;
				}
				throw e;
			}
			new Worker(socket).start();
		}
	}

	public static void stop() throws IOException {
		isStopRequested.set(true);
		if(serverSocket != null) {
			serverSocket.close();
		}
	}

	private static class Worker extends Thread {

		private Socket socket;
		private BufferedReader reader;
		private BufferedWriter writer;

		public Worker(Socket socket) throws IOException {
			this.socket = socket;
			this.reader = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
			this.writer = new BufferedWriter(new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8));
		}

		@Override
		public void run() {
			try {
				for(;;) {
					// HTTPリクエストヘッダー部のみを読み取ります。（2重改行=空行が出現するまで）
					String line = reader.readLine();
					if(line == null || line.length() == 0) {
						break;
					}
				}
				writer.write("HTTP/1.1 200 OK\r\n");
				writer.write("Content-Type: text/html; charset=utf-8\r\n");
				writer.write("\r\n");
				writer.write("Hello, World!! (");
				writer.write(new SimpleDateFormat("yyyy-MM-dd HH:mm:ss").format(new Date()));
				writer.write(")\r\n");
				writer.close();
			} catch(Exception e) {
				throw new RuntimeException(e);
			}
		}
	}
}
