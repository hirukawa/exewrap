INC     = /I ..\include /I..\include\win32
LIBS    = kernel32.lib user32.lib advapi32.lib shell32.lib

CC      = cl.exe
LINK    = link.exe
RC      = RC.exe
JAVAC   = javac.exe
JAR     = jar.exe

!if ([@cl.exe 2>&1| @findstr "x64" > nul] == 0)
ARCHITECTURE=x64
BIN=..\bin\x64
OBJ=..\obj\x64
CRT=..\lib\crt\amd64
!else
ARCHITECTURE=x86
BIN=..\bin\x86
OBJ=..\obj\x86
CRT=..\lib\crt
!endif

CFLAGS=\
	/nologo\
	/MD\
	/W3\
	/O1\
	/GS-\
	/GL\
	/c

LDFLAGS=\
	/nologo\
	/LTCG\
	/OPT:REF\
	/LIBPATH:$(CRT)

RCFLAGS=\
	/nologo

ALL :
	Make.bat

EXEWRAP_X86 : $(BIN)\exewrap.exe
EXEWRAP_X64 : $(BIN)\exewrap.exe

IMAGE_X86 : $(OBJ)\image_console.exe $(OBJ)\image_gui.exe $(OBJ)\image_service.exe
IMAGE_X64 : $(OBJ)\image_console.exe $(OBJ)\image_gui.exe $(OBJ)\image_service.exe

CLEAN :
	@if exist $(OBJ)\..\x86 RMDIR /S /Q $(OBJ)\..\x86
	@if exist $(OBJ)\..\x64 RMDIR /S /Q $(OBJ)\..\x64

$(BIN) :
	@if not exist $(BIN)/$(NULL) mkdir $(BIN)

$(OBJ) :
	@if not exist $(OBJ)/$(NULL) mkdir $(OBJ)

$(BIN)\exewrap.exe : $(BIN) $(OBJ) $(OBJ)\exewrap.res $(OBJ)\exewrap.obj \
		$(OBJ)\jvm.obj $(OBJ)\loader.obj $(OBJ)\message.obj $(OBJ)\eventlog.obj \
		$(OBJ)\image_console.exe $(OBJ)\image_gui.exe $(OBJ)\image_service.exe \
		$(OBJ)\bindres.exe $(OBJ)\exewrap\core\ByteBufferInputStream.class $(OBJ)\exewrap\core\ExewrapClassLoader.class $(OBJ)\tool.jar
	$(LINK) $(LDFLAGS) /SUBSYSTEM:CONSOLE /MANIFEST:EMBED /OUT:$(BIN)\exewrap.exe $(OBJ)\exewrap.res $(OBJ)\exewrap.obj \
		$(OBJ)\jvm.obj $(OBJ)\loader.obj $(OBJ)\message.obj $(OBJ)\eventlog.obj $(LIBS)
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe VERSION_INFO             resources\versioninfo.bin
	$(OBJ)\bindres.exe -r $(BIN)\exewrap.exe IMAGE_CONSOLE_32      $(OBJ)\..\x86\image_console.exe
	$(OBJ)\bindres.exe -r $(BIN)\exewrap.exe IMAGE_CONSOLE_64      $(OBJ)\..\x64\image_console.exe
	$(OBJ)\bindres.exe -r $(BIN)\exewrap.exe IMAGE_GUI_32          $(OBJ)\..\x86\image_gui.exe
	$(OBJ)\bindres.exe -r $(BIN)\exewrap.exe IMAGE_GUI_64          $(OBJ)\..\x64\image_gui.exe
	$(OBJ)\bindres.exe -r $(BIN)\exewrap.exe IMAGE_SERVICE_32      $(OBJ)\..\x86\image_service.exe
	$(OBJ)\bindres.exe -r $(BIN)\exewrap.exe IMAGE_SERVICE_64      $(OBJ)\..\x64\image_service.exe
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe BYTE_BUFFER_INPUT_STREAM $(OBJ)\exewrap\core\ByteBufferInputStream.class
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe PACK_INPUT_STREAM        $(OBJ)\exewrap\core\PackInputStream.class
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe URL_CONNECTION           $(OBJ)\exewrap\core\URLConnection.class
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe URL_STREAM_HANDLER       $(OBJ)\exewrap\core\URLStreamHandler.class
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe EXEWRAP_CLASS_LOADER     $(OBJ)\exewrap\core\ExewrapClassLoader.class
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe UTIL_JAR                 $(OBJ)\util.jar
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe JAR                      $(OBJ)\tool.jar

$(OBJ)\exewrap.obj : $(OBJ) exewrap.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\exewrap.obj exewrap.c

$(OBJ)\exewrap.res : $(OBJ) resources\exewrap.rc
	$(RC) $(RCFLAGS) /fo$(OBJ)\exewrap.res  resources\exewrap.rc

$(OBJ)\image_console.exe : $(OBJ) $(OBJ)\image_console.res $(OBJ)\image_console.obj \
		$(OBJ)\jvm.obj $(OBJ)\loader.obj $(OBJ)\message.obj $(OBJ)\notify.obj $(OBJ)\eventlog.obj \
		$(OBJ)\bindres.exe $(OBJ)\exewrap\core\ByteBufferInputStream.class $(OBJ)\exewrap\core\PackInputStream.class $(OBJ)\exewrap\core\ExewrapClassLoader.class $(OBJ)\exewrap\core\URLConnection.class $(OBJ)\exewrap\core\URLStreamHandler.class $(OBJ)\util.jar
	$(LINK) $(LDFLAGS) /SUBSYSTEM:CONSOLE /MANIFEST:EMBED /OUT:$(OBJ)\image_console.exe $(OBJ)\image_console.obj \
		$(OBJ)\jvm.obj $(OBJ)\loader.obj $(OBJ)\message.obj $(OBJ)\notify.obj $(OBJ)\eventlog.obj $(LIBS)
	$(OBJ)\bindres.exe $(OBJ)\image_console.exe BYTE_BUFFER_INPUT_STREAM $(OBJ)\exewrap\core\ByteBufferInputStream.class
	$(OBJ)\bindres.exe $(OBJ)\image_console.exe PACK_INPUT_STREAM        $(OBJ)\exewrap\core\PackInputStream.class
	$(OBJ)\bindres.exe $(OBJ)\image_console.exe URL_CONNECTION           $(OBJ)\exewrap\core\URLConnection.class
	$(OBJ)\bindres.exe $(OBJ)\image_console.exe URL_STREAM_HANDLER       $(OBJ)\exewrap\core\URLStreamHandler.class
	$(OBJ)\bindres.exe $(OBJ)\image_console.exe EXEWRAP_CLASS_LOADER     $(OBJ)\exewrap\core\ExewrapClassLoader.class
	$(OBJ)\bindres.exe $(OBJ)\image_console.exe UTIL_JAR                 $(OBJ)\util.jar
#	$(OBJ)\bindres.exe $(OBJ)\image_console.exe PACK_GZ ..\helloworld.pack.gz

$(OBJ)\image_console.obj : $(OBJ) image_console.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\image_console.obj image_console.c

$(OBJ)\image_console.res : $(OBJ) resources\image_console.rc resources\eventlog.bin
	$(RC) $(RCFLAGS) /fo$(OBJ)\image_console.res resources\image_console.rc
	
$(OBJ)\image_gui.exe : $(OBJ) $(OBJ)\image_gui.res $(OBJ)\image_gui.obj \
		$(OBJ)\jvm.obj $(OBJ)\loader.obj $(OBJ)\message.obj $(OBJ)\notify.obj $(OBJ)\eventlog.obj \
		$(OBJ)\bindres.exe $(OBJ)\exewrap\core\ByteBufferInputStream.class $(OBJ)\exewrap\core\PackInputStream.class $(OBJ)\exewrap\core\ExewrapClassLoader.class $(OBJ)\exewrap\core\URLConnection.class $(OBJ)\exewrap\core\URLStreamHandler.class $(OBJ)\util.jar
	$(LINK) $(LDFLAGS) /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /OUT:$(OBJ)\image_gui.exe $(OBJ)\image_gui.res $(OBJ)\image_gui.obj \
		$(OBJ)\jvm.obj $(OBJ)\loader.obj $(OBJ)\message.obj $(OBJ)\notify.obj $(OBJ)\eventlog.obj $(LIBS)
	$(OBJ)\bindres.exe $(OBJ)\image_gui.exe BYTE_BUFFER_INPUT_STREAM $(OBJ)\exewrap\core\ByteBufferInputStream.class
	$(OBJ)\bindres.exe $(OBJ)\image_gui.exe PACK_INPUT_STREAM        $(OBJ)\exewrap\core\PackInputStream.class
	$(OBJ)\bindres.exe $(OBJ)\image_gui.exe URL_CONNECTION           $(OBJ)\exewrap\core\URLConnection.class
	$(OBJ)\bindres.exe $(OBJ)\image_gui.exe URL_STREAM_HANDLER       $(OBJ)\exewrap\core\URLStreamHandler.class
	$(OBJ)\bindres.exe $(OBJ)\image_gui.exe EXEWRAP_CLASS_LOADER     $(OBJ)\exewrap\core\ExewrapClassLoader.class
	$(OBJ)\bindres.exe $(OBJ)\image_gui.exe UTIL_JAR                 $(OBJ)\util.jar
	$(OBJ)\bindres.exe $(OBJ)\image_gui.exe JAR ..\samples\Sample04.jar

$(OBJ)\image_gui.obj : $(OBJ) image_gui.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\image_gui.obj image_gui.c

$(OBJ)\image_gui.res : $(OBJ) resources\image_gui.rc resources\eventlog.bin
	$(RC) $(RCFLAGS) /fo$(OBJ)\image_gui.res resources\image_gui.rc

$(OBJ)\image_service.exe : $(OBJ) $(OBJ)\image_service.res $(OBJ)\image_service.obj \
		$(OBJ)\jvm.obj $(OBJ)\loader.obj $(OBJ)\message.obj $(OBJ)\eventlog.obj \
		$(OBJ)\bindres.exe $(OBJ)\exewrap\core\ByteBufferInputStream.class $(OBJ)\exewrap\core\PackInputStream.class $(OBJ)\exewrap\core\ExewrapClassLoader.class $(OBJ)\exewrap\core\URLConnection.class $(OBJ)\exewrap\core\URLStreamHandler.class $(OBJ)\util.jar
	$(LINK) $(LDFLAGS) /SUBSYSTEM:CONSOLE /MANIFEST:EMBED /OUT:$(OBJ)\image_service.exe $(OBJ)\image_service.res $(OBJ)\image_service.obj \
		$(OBJ)\jvm.obj $(OBJ)\loader.obj $(OBJ)\message.obj $(OBJ)\eventlog.obj $(LIBS)
	$(OBJ)\bindres.exe $(OBJ)\image_service.exe BYTE_BUFFER_INPUT_STREAM $(OBJ)\exewrap\core\ByteBufferInputStream.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.exe PACK_INPUT_STREAM        $(OBJ)\exewrap\core\PackInputStream.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.exe URL_CONNECTION           $(OBJ)\exewrap\core\URLConnection.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.exe URL_STREAM_HANDLER       $(OBJ)\exewrap\core\URLStreamHandler.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.exe EXEWRAP_CLASS_LOADER     $(OBJ)\exewrap\core\ExewrapClassLoader.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.exe UTIL_JAR                 $(OBJ)\util.jar
	$(OBJ)\bindres.exe $(OBJ)\image_service.exe JAR ..\samples\ServiceSample.jar

$(OBJ)\image_service.obj : $(OBJ) image_service.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\image_service.obj image_service.c

$(OBJ)\image_service.res : $(OBJ) resources\image_service.rc resources\eventlog.bin
	$(RC) $(RCFLAGS) /fo$(OBJ)\image_service.res resources\image_service.rc

$(OBJ)\eventlog.obj : $(OBJ) eventlog.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\eventlog.obj eventlog.c

$(OBJ)\notify.obj : $(OBJ) notify.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\notify.obj notify.c

$(OBJ)\message.obj : $(OBJ) message.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\message.obj message.c

$(OBJ)\loader.obj : $(OBJ) loader.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\loader.obj loader.c

$(OBJ)\jvm.obj : $(OBJ) jvm.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\jvm.obj jvm.c

$(OBJ)\bindres.exe : $(OBJ) $(OBJ)\bindres.obj
	$(LINK) $(LDFLAGS) /SUBSYSTEM:CONSOLE /OUT:$(OBJ)\bindres.exe $(OBJ)\bindres.obj $(LIBS)

$(OBJ)\bindres.obj : $(OBJ) bindres.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\bindres.obj bindres.c

###
### Java
###

### exewrap.core

$(OBJ)\exewrap\core\ExewrapClassLoader.class : $(OBJ) java\exewrap\core\ExewrapClassLoader.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\exewrap\core\ExewrapClassLoader.java

$(OBJ)\exewrap\core\PackInputStream.class : $(OBJ) java\exewrap\core\PackInputStream.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\exewrap\core\PackInputStream.java

$(OBJ)\exewrap\core\ByteBufferInputStream.class : $(OBJ) java\exewrap\core\ByteBufferInputStream.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\exewrap\core\ByteBufferInputStream.java

$(OBJ)\exewrap\core\URLConnection.class : $(OBJ) java\exewrap\core\URLConnection.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\exewrap\core\URLConnection.java

$(OBJ)\exewrap\core\URLStreamHandler.class : $(OBJ) java\exewrap\core\URLStreamHandler.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\exewrap\core\URLStreamHandler.java

### exewrap.util

$(OBJ)\util.jar : $(OBJ) $(OBJ)\exewrap\util\EventLogHandler.class $(OBJ)\exewrap\util\EventLogStream.class $(OBJ)\exewrap\util\FileLogStream.class $(OBJ)\exewrap\util\UncaughtExceptionHandler.class
	$(JAR) cvf $(OBJ)\util.jar -C $(OBJ) exewrap\util

$(OBJ)\exewrap\util\EventLogHandler.class : $(OBJ) java\exewrap\util\EventLogHandler.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\exewrap\util\EventLogHandler.java

$(OBJ)\exewrap\util\EventLogStream.class : $(OBJ) java\exewrap\util\EventLogStream.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\exewrap\util\EventLogStream.java

$(OBJ)\exewrap\util\FileLogStream.class : $(OBJ) java\exewrap\util\FileLogStream.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\exewrap\util\FileLogStream.java

$(OBJ)\exewrap\util\UncaughtExceptionHandler.class : $(OBJ) java\exewrap\util\UncaughtExceptionHandler.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\exewrap\util\UncaughtExceptionHandler.java

### exewrap.tool

$(OBJ)\tool.jar : $(OBJ) $(OBJ)\exewrap\tool\JarProcessor.class $(OBJ)\exewrap\tool\PackOutputStream.class
	$(JAR) cvfe $(OBJ)\tool.jar exewrap.tool.JarProcessor -C $(OBJ) exewrap\tool

$(OBJ)\exewrap\tool\PackOutputStream.class : $(OBJ) java\exewrap\tool\PackOutputStream.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\exewrap\tool\PackOutputStream.java

$(OBJ)\exewrap\tool\JarProcessor.class : $(OBJ) java\exewrap\tool\JarProcessor.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\exewrap\tool\JarProcessor.java

