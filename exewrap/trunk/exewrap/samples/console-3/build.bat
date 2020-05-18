CD /D %~dp0

javac -encoding UTF-8 ConsoleSample3.java
jar cfe ConsoleSample3.jar ConsoleSample3 ConsoleSample3.class
exewrap ConsoleSample3.jar

