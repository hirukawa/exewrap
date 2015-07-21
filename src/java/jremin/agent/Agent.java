package jremin.agent;

import java.lang.instrument.Instrumentation;

public class Agent {
	public static void premain(String agentArgs, Instrumentation instrumentation) {
		Class<?>[] classes = instrumentation.getAllLoadedClasses();
		for (Class<?> cls : classes) {
			String name = cls.getName();
			if(name.length() >= 1 && name.charAt(0) != '[' && !name.startsWith("jremin.")) {
				System.out.println(name);
			}
		}
	}
	
	public static void main(String[] args) {
	}
}
