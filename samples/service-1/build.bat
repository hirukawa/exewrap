CD /D %~dp0

mkdir bin
javac -encoding UTF-8 *.java
jar cfe ServiceSample1.jar ServiceSample1 *.class
exewrap -s ServiceSample1.jar

