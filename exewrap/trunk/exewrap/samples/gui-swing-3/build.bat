CD /D %~dp0

javac -encoding UTF-8 SwingSample3.java
jar cfe SwingSample3.jar SwingSample3 SwingSample3.class
exewrap -g -e SHARE SwingSample3.jar

