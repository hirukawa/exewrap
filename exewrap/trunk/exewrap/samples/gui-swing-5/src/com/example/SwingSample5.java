package com.example;

import javax.swing.JFrame;
import javax.swing.JLabel;
import java.awt.Font;
import java.io.InputStream;

import static javax.swing.SwingConstants.*;

public class SwingSample5 {

	public static void main(String[] args) {

		JFrame frame = new JFrame("SwingSample5");
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
