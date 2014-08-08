INC     = /I ..\include /I..\include\win32
LIBS    = kernel32.lib user32.lib advapi32.lib shell32.lib

CC      = cl.exe
LINK    = link.exe
RC      = RC.exe
JAVAC   = javac.exe

!if ([@cl.exe 2>&1| @findstr "x64" > nul] == 0)
BIN=..\bin\x64
OBJ=..\obj\x64
!else
BIN=..\bin\x86
OBJ=..\obj\x86
!endif

CFLAGS=\
	/nologo\
	/W3\
	/O1\
	/GS-\
	/GL\
	/c

LDFLAGS=\
	/nologo\
	/LTCG\
	/NODEFAULTLIB\
	/ENTRY:mainCRTStartup

RCFLAGS=\
	/nologo

ALL :
	Make.bat

EXEWRAP_X86 : $(BIN)\exewrap.exe

EXEWRAP_X64 : $(BIN)\exewrap.exe

IMAGE_X86 : $(OBJ)\image_console.img $(OBJ)\image_gui.img $(OBJ)\image_service.img

IMAGE_X64 : $(OBJ)\image_console.img $(OBJ)\image_gui.img $(OBJ)\image_service.img

CLEAN :
	-@erase /Q $(OBJ)\..\x86\*
	-@erase /Q $(OBJ)\..\x86
	-@erase /Q $(OBJ)\..\x64\*
	-@erase /Q $(OBJ)\..\x64
	-@erase /Q $(BIN)\..\x86\*
	-@erase /Q $(BIN)\..\x86
	-@erase /Q $(BIN)\..\x64\*
	-@erase /Q $(BIN)\..\x64

$(BIN) :
	@if not exist $(BIN)/$(NULL) mkdir $(BIN)

$(OBJ) :
	@if not exist $(OBJ)/$(NULL) mkdir $(OBJ)

$(BIN)\exewrap.exe : $(BIN) $(OBJ) $(OBJ)\exewrap.obj $(OBJ)\startup.obj $(OBJ)\message.obj $(OBJ)\jvm.obj $(OBJ)\libc.obj $(OBJ)\image_console.img $(OBJ)\image_gui.img $(OBJ)\image_service.img $(OBJ)\exewrap.res  $(OBJ)\ClassicLoader.class $(OBJ)\PackLoader.class $(OBJ)\JarOptimizer.class
	$(LINK) $(LDFLAGS) /SUBSYSTEM:CONSOLE /OUT:$(BIN)\exewrap.exe $(OBJ)\exewrap.obj $(OBJ)\startup.obj $(OBJ)\message.obj $(OBJ)\jvm.obj $(OBJ)\libc.obj $(OBJ)\exewrap.res $(LIBS)
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe VERSION_INFO       resources\versioninfo.bin
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe IMAGE_CONSOLE_32   $(OBJ)\..\x86\image_console.img
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe IMAGE_CONSOLE_64   $(OBJ)\..\x64\image_console.img
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe IMAGE_GUI_32       $(OBJ)\..\x86\image_gui.img
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe IMAGE_GUI_64       $(OBJ)\..\x64\image_gui.img
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe IMAGE_SERVICE_32   $(OBJ)\..\x86\image_service.img
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe IMAGE_SERVICE_64   $(OBJ)\..\x64\image_service.img
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe CLASSIC_LOADER     $(OBJ)\ClassicLoader.class
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe PACK_LOADER        $(OBJ)\PackLoader.class
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe JAR_OPTIMIZER      $(OBJ)\JarOptimizer.class

$(OBJ)\exewrap.obj : $(OBJ) exewrap.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\exewrap.obj exewrap.c

$(OBJ)\exewrap.res : $(OBJ) resources\exewrap.rc
	$(RC) $(RCFLAGS) /fo$(OBJ)\exewrap.res  resources\exewrap.rc

$(OBJ)\image_console.img : $(OBJ) $(OBJ)\image_console.obj $(OBJ)\image_console.res $(OBJ)\startup.obj $(OBJ)\notify.obj $(OBJ)\message.obj $(OBJ)\jvm.obj $(OBJ)\libc.obj $(OBJ)\bindres.exe $(OBJ)\URLConnection.class $(OBJ)\URLStreamHandler.class
	$(LINK) $(LDFLAGS) /SUBSYSTEM:CONSOLE /OUT:$(OBJ)\image_console.img $(OBJ)\image_console.obj $(OBJ)\image_console.res $(OBJ)\startup.obj $(OBJ)\notify.obj $(OBJ)\message.obj $(OBJ)\jvm.obj $(OBJ)\libc.obj $(LIBS)
	$(OBJ)\bindres.exe $(OBJ)\image_console.img URL_CONNECTION     $(OBJ)\URLConnection.class
	$(OBJ)\bindres.exe $(OBJ)\image_console.img URL_STREAM_HANDLER $(OBJ)\URLStreamHandler.class

$(OBJ)\image_console.obj : $(OBJ) image_console.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\image_console.obj image_console.c

$(OBJ)\image_console.res : $(OBJ) resources\image_console.rc
	$(RC) $(RCFLAGS) /fo$(OBJ)\image_console.res resources\image_console.rc

$(OBJ)\image_gui.img : $(OBJ) $(OBJ)\image_gui.obj $(OBJ)\image_gui.res $(OBJ)\startup.obj $(OBJ)\notify.obj $(OBJ)\message.obj $(OBJ)\jvm.obj $(OBJ)\libc.obj $(OBJ)\bindres.exe $(OBJ)\FileLogStream.class $(OBJ)\UncaughtHandler.class $(OBJ)\URLConnection.class $(OBJ)\URLStreamHandler.class
	$(LINK) $(LDFLAGS) /SUBSYSTEM:WINDOWS /OUT:$(OBJ)\image_gui.img $(OBJ)\image_gui.obj $(OBJ)\image_gui.res $(OBJ)\startup.obj $(OBJ)\notify.obj $(OBJ)\message.obj $(OBJ)\jvm.obj $(OBJ)\libc.obj $(LIBS)
	$(OBJ)\bindres.exe $(OBJ)\image_gui.img FILELOG_STREAM     $(OBJ)\FileLogStream.class
	$(OBJ)\bindres.exe $(OBJ)\image_gui.img UNCAUGHT_HANDLER   $(OBJ)\UncaughtHandler.class
	$(OBJ)\bindres.exe $(OBJ)\image_gui.img URL_CONNECTION     $(OBJ)\URLConnection.class
	$(OBJ)\bindres.exe $(OBJ)\image_gui.img URL_STREAM_HANDLER $(OBJ)\URLStreamHandler.class

$(OBJ)\image_gui.obj : $(OBJ) image_gui.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\image_gui.obj image_gui.c

$(OBJ)\image_gui.res : $(OBJ) resources\image_gui.rc
	$(RC) $(RCFLAGS) /fo$(OBJ)\image_gui.res resources\image_gui.rc

$(OBJ)\image_service.img : $(OBJ) $(OBJ)\image_service.obj $(OBJ)\image_service.res $(OBJ)\service.obj $(OBJ)\startup.obj $(OBJ)\eventlog.obj $(OBJ)\message.obj $(OBJ)\jvm.obj $(OBJ)\libc.obj $(OBJ)\bindres.exe $(OBJ)\EventLogStream.class $(OBJ)\EventLogHandler.class $(OBJ)\UncaughtHandler.class $(OBJ)\URLConnection.class $(OBJ)\URLStreamHandler.class
	$(LINK) $(LDFLAGS) /SUBSYSTEM:CONSOLE /OUT:$(OBJ)\image_service.img $(OBJ)\image_service.obj $(OBJ)\image_service.res $(OBJ)\service.obj $(OBJ)\startup.obj $(OBJ)\eventlog.obj $(OBJ)\message.obj $(OBJ)\jvm.obj $(OBJ)\libc.obj $(LIBS)
	$(OBJ)\bindres.exe $(OBJ)\image_service.img EVENTLOG_STREAM    $(OBJ)\EventLogStream.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.img EVENTLOG_HANDLER   $(OBJ)\EventLogHandler.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.img UNCAUGHT_HANDLER   $(OBJ)\UncaughtHandler.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.img URL_CONNECTION     $(OBJ)\URLConnection.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.img URL_STREAM_HANDLER $(OBJ)\URLStreamHandler.class

$(OBJ)\image_service.obj : $(OBJ) image_service.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\image_service.obj image_service.c

$(OBJ)\image_service.res : $(OBJ) resources\image_service.rc resources\eventlog.bin
	$(RC) $(RCFLAGS) /fo$(OBJ)\image_service.res resources\image_service.rc

$(OBJ)\service.obj : $(OBJ) service.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\service.obj service.c

$(OBJ)\eventlog.obj : $(OBJ) eventlog.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\eventlog.obj eventlog.c

$(OBJ)\notify.obj : $(OBJ) notify.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\notify.obj notify.c

$(OBJ)\message.obj : $(OBJ) message.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\message.obj message.c

$(OBJ)\jvm.obj : $(OBJ) jvm.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\jvm.obj jvm.c

$(OBJ)\bindres.exe : $(OBJ) $(OBJ)\bindres.obj $(OBJ)\startup.obj $(OBJ)\libc.obj
	$(LINK) $(LDFLAGS) /SUBSYSTEM:CONSOLE /OUT:$(OBJ)\bindres.exe $(OBJ)\bindres.obj $(OBJ)\startup.obj $(OBJ)\libc.obj $(LIBS)

$(OBJ)\bindres.obj : $(OBJ) bindres.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\bindres.obj bindres.c

$(OBJ)\startup.obj : $(OBJ) startup.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\startup.obj startup.c

$(OBJ)\libc.obj : $(OBJ) libc.c
	$(CC) $(CFLAGS) $(INC) /Fo$(OBJ)\libc.obj libc.c

$(OBJ)\JarOptimizer.class : java\JarOptimizer.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\JarOptimizer.java

$(OBJ)\PackLoader.class : java\PackLoader.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.5 -sourcepath java -d $(OBJ) java\PackLoader.java

$(OBJ)\ClassicLoader.class : java\ClassicLoader.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\ClassicLoader.java

$(OBJ)\FileLogStream.class : java\FileLogStream.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\FileLogStream.java

$(OBJ)\EventLogStream.class : java\EventLogStream.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\EventLogStream.java

$(OBJ)\EventLogHandler.class : java\EventLogHandler.java
	$(JAVAC) -Xlint:none -source 1.4 -target 1.4 -sourcepath java -d $(OBJ) java\EventLogHandler.java

$(OBJ)\UncaughtHandler.class : java\UncaughtHandler.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\UncaughtHandler.java

$(OBJ)\URLConnection.class : java\URLConnection.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\URLConnection.java

$(OBJ)\URLStreamHandler.class : java\URLStreamHandler.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\URLStreamHandler.java

