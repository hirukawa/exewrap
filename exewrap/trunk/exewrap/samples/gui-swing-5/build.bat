CD /D %~dp0

mkdir bin
javac -encoding UTF-8 -d bin -sourcepath src src\com\example\*.java
copy /Y /B src\splash-screen-sample.png bin\splash-screen-sample.png
jar cfm SwingSample5.jar SwingSample5.manifest -C bin .
exewrap -g SwingSample5.jar

