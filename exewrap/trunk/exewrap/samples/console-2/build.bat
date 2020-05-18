CD /D %~dp0

javac -encoding UTF-8 ConsoleSample2.java
jar cfe ConsoleSample2.jar ConsoleSample2 ConsoleSample2.class
exewrap -e SINGLE ConsoleSample2.jar

