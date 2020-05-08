import java.awt.Font;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.SwingUtilities;

import static javax.swing.SwingConstants.*;

public class SwingSample3 {

	private static int count = 0;
	private static JFrame frame;
	private static JLabel label;

	public static void main(String[] args) {
		final StringBuilder text = new StringBuilder();
		if(args.length > 0) {
			for(String arg : args) {
				text.append(arg + "  ");
			}
		}

		if(count++ > 0) {
			SwingUtilities.invokeLater(new Runnable() {
				@Override
				public void run() {
					if(frame != null) {
						if(text.length() > 0) {
							label.setText(text.toString());
						}
						frame.toFront();
					}
				}
			});
			return;
		}

		SwingUtilities.invokeLater(new Runnable() {
			@Override
			public void run() {
				frame = new JFrame("SwingSample3");
				frame.setSize(320, 240);
				frame.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);

				label = new JLabel(text.length() > 0 ? text.toString() : "Hello, World!!");
				label.setFont(new Font(Font.SANS_SERIF, Font.PLAIN, 16));
				label.setHorizontalAlignment(CENTER);
				frame.getContentPane().add(label);

				frame.setLocationRelativeTo(null);
				frame.setVisible(true);
			}
		});
	}
}
