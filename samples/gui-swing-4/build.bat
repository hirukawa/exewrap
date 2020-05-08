CD /D %~dp0

javac -encoding UTF-8 SwingSample4.java
jar cfm SwingSample4.jar SwingSample4.manifest SwingSample4.class splash-screen-sample.png
exewrap -g SwingSample4.jar

