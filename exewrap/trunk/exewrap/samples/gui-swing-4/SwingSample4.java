import java.awt.Font;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.SwingUtilities;

import static javax.swing.SwingConstants.*;

public class SwingSample4 {

	public static void main(String[] args) {
		JFrame frame = new JFrame("SwingSample4");
		frame.setSize(320, 240);
		frame.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);

		JLabel label = new JLabel("Hello, World!!");
		label.setFont(new Font(Font.SANS_SERIF, Font.PLAIN, 16));
		label.setHorizontalAlignment(CENTER);
		frame.getContentPane().add(label);

		frame.setLocationRelativeTo(null);
		frame.setVisible(true);
	}
}
