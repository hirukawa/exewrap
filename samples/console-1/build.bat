CD /D %~dp0

javac -encoding UTF-8 ConsoleSample1.java
jar cfe ConsoleSample1.jar ConsoleSample1 ConsoleSample1.class
exewrap ConsoleSample1.jar

