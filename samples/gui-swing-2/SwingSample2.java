import java.awt.Font;
import javax.swing.JFrame;
import javax.swing.JLabel;
import static javax.swing.SwingConstants.*;

public class SwingSample2 {

	private static int count = 0;

	public static void main(String[] args) {

		StringBuilder text = new StringBuilder();
		if(args.length == 0) {
			text.append("Hello, World!!");
		} else {
			for(String arg : args) {
				text.append(arg + "  ");
			}
		}

		count++;

		JFrame frame = new JFrame("SwingSample2 #" + count);
		frame.setSize(320, 240);
		frame.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);

		JLabel label = new JLabel(text.toString());
		label.setFont(new Font(Font.SANS_SERIF, Font.PLAIN, 16));
		label.setHorizontalAlignment(CENTER);
		frame.getContentPane().add(label);

		frame.setLocationRelativeTo(null);
		frame.setVisible(true);
	}
}
