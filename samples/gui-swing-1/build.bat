CD /D %~dp0

javac -encoding UTF-8 SwingSample1.java
jar cfe SwingSample1.jar SwingSample1 SwingSample1.class
exewrap -g SwingSample1.jar

