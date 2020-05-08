CD /D %~dp0

javac -encoding UTF-8 SwingSample2.java
jar cfe SwingSample2.jar SwingSample2 SwingSample2.class
exewrap -g -e SHARE SwingSample2.jar

