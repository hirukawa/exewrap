CC = gcc.exe
CXX = g++.exe
JAVAC   = javac.exe
WINDRES = windres.exe
#LDFLAGS = -static-libgcc -static-libstdc++
LDFLAGS = -nostdlib -Wl,--entry,_mainCRTStartup -lmsvcrt -lkernel32 -luser32 -ladvapi32 -lshell32

OPT = -Os
OPT_CHARSET = -finput-charset=CP932 -fexec-charset=CP932
BIN = ..
OBJ = ..\obj

# 
# gcc hello.c -nostdlib -Os -s -lkernel32 -o hello.exe
# strip hello.exe
# 

.PHONY: all
all: $(BIN)\exewrap.exe

.PHONY: clean
clean:
	- rm -r $(OBJ)
	mkdir $(OBJ)

$(BIN)\exewrap.exe: $(OBJ)\exewrap.o $(OBJ)\startup.o $(OBJ)\message.o $(OBJ)\jvm.o \
	  $(OBJ)\image_console.img \
	  $(OBJ)\image_gui.img \
	  $(OBJ)\image_service.img $(OBJ)\exewrap.res \
	  $(OBJ)\JarOptimizer.class \
	  $(OBJ)\PackLoader.class $(OBJ)\ClassicLoader.class
	
	$(CC) -mthreads -O0 -o $(BIN)\exewrap.exe \
	  $(OBJ)\exewrap.o $(OBJ)\startup.o $(OBJ)\message.o $(OBJ)\jvm.o $(OBJ)\exewrap.res $(LDFLAGS) -lversion $(OPT_CHARSET)
	
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe VERSION_INFO       resources\versioninfo.bin
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe IMAGE_CONSOLE      $(OBJ)\image_console.img
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe IMAGE_GUI          $(OBJ)\image_gui.img
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe IMAGE_SERVICE      $(OBJ)\image_service.img
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe CLASSIC_LOADER     $(OBJ)\ClassicLoader.class
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe PACK_LOADER        $(OBJ)\PackLoader.class
	$(OBJ)\bindres.exe $(BIN)\exewrap.exe JAR_OPTIMIZER      $(OBJ)\JarOptimizer.class

$(OBJ)\exewrap.o: exewrap.c
	$(CC) -o $(OBJ)\exewrap.o -c exewrap.c $(OPT_CHARSET)

$(OBJ)\exewrap.res: resources\exewrap.rc
	$(WINDRES) -o $(OBJ)\exewrap.res -i resources\exewrap.rc -O coff
	
$(OBJ)\image_console.img: $(OBJ)\image_console.o $(OBJ)\image_console.res $(OBJ)\startup.o $(OBJ)\notify.o $(OBJ)\message.o $(OBJ)\jvm.o $(OBJ)\bindres.exe $(OBJ)\URLConnection.class $(OBJ)\URLStreamHandler.class
	$(CC) -mthreads -Os -s -o $(OBJ)\image_console.img $(OBJ)\image_console.o $(OBJ)\image_console.res $(OBJ)\startup.o $(OBJ)\notify.o $(OBJ)\message.o $(OBJ)\jvm.o $(LDFLAGS) $(OPT_CHARSET)
	$(OBJ)\bindres.exe $(OBJ)\image_console.img URL_CONNECTION     $(OBJ)\URLConnection.class
	$(OBJ)\bindres.exe $(OBJ)\image_console.img URL_STREAM_HANDLER $(OBJ)\URLStreamHandler.class

$(OBJ)\image_console.o: image_console.c
	$(CC) -O0 -o $(OBJ)\image_console.o -c image_console.c $(OPT_CHARSET)

$(OBJ)\image_console.res: resources\image_console.rc
	$(WINDRES) -o $(OBJ)\image_console.res -i resources\image_console.rc -O coff 

$(OBJ)\image_gui.img: $(OBJ)\image_gui.o $(OBJ)\image_gui.res $(OBJ)\startup.o $(OBJ)\notify.o $(OBJ)\message.o $(OBJ)\jvm.o $(OBJ)\bindres.exe $(OBJ)\FileLogStream.class $(OBJ)\UncaughtHandler.class $(OBJ)\URLConnection.class $(OBJ)\URLStreamHandler.class
	$(CC) -mwindows -mthreads -g -O0 -o $(OBJ)\image_gui.img $(OBJ)\image_gui.o $(OBJ)\image_gui.res $(OBJ)\startup.o $(OBJ)\notify.o $(OBJ)\message.o $(OBJ)\jvm.o $(LDFLAGS) $(OPT_CHARSET)
	$(OBJ)\bindres.exe $(OBJ)\image_gui.img FILELOG_STREAM     $(OBJ)\FileLogStream.class
	$(OBJ)\bindres.exe $(OBJ)\image_gui.img UNCAUGHT_HANDLER   $(OBJ)\UncaughtHandler.class
	$(OBJ)\bindres.exe $(OBJ)\image_gui.img URL_CONNECTION     $(OBJ)\URLConnection.class
	$(OBJ)\bindres.exe $(OBJ)\image_gui.img URL_STREAM_HANDLER $(OBJ)\URLStreamHandler.class

$(OBJ)\image_gui.o: image_gui.c
	$(CC) -o $(OBJ)\image_gui.o -c image_gui.c $(OPT_CHARSET)

$(OBJ)\image_gui.res: resources\image_gui.rc
	$(WINDRES) -o $(OBJ)\image_gui.res -i resources\image_gui.rc -O coff 

$(OBJ)\image_service.img: $(OBJ)\image_service.o $(OBJ)\image_service.res $(OBJ)\service.o $(OBJ)\startup.o $(OBJ)\eventlog.o $(OBJ)\message.o $(OBJ)\jvm.o $(OBJ)\bindres.exe $(OBJ)\EventLogStream.class $(OBJ)\EventLogHandler.class $(OBJ)\UncaughtHandler.class $(OBJ)\URLConnection.class $(OBJ)\URLStreamHandler.class
	$(CC) -mthreads -Os -s -o $(OBJ)\image_service.img $(OBJ)\image_service.o $(OBJ)\image_service.res $(OBJ)\service.o $(OBJ)\startup.o $(OBJ)\eventlog.o $(OBJ)\message.o $(OBJ)\jvm.o $(LDFLAGS) -lshlwapi $(OPT_CHARSET)
	$(OBJ)\bindres.exe $(OBJ)\image_service.img EVENTLOG_STREAM    $(OBJ)\EventLogStream.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.img EVENTLOG_HANDLER   $(OBJ)\EventLogHandler.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.img UNCAUGHT_HANDLER   $(OBJ)\UncaughtHandler.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.img URL_CONNECTION     $(OBJ)\URLConnection.class
	$(OBJ)\bindres.exe $(OBJ)\image_service.img URL_STREAM_HANDLER $(OBJ)\URLStreamHandler.class

$(OBJ)\image_service.o: image_service.c
	$(CC) -o $(OBJ)\image_service.o -c image_service.c $(OPT_CHARSET)

$(OBJ)\image_service.res: resources\image_service.rc resources\eventlog.bin
	$(WINDRES) -o $(OBJ)\image_service.res -i resources\image_service.rc -O coff

$(OBJ)\service.o: service.c
	$(CC) -c -o $(OBJ)\service.o service.c $(OPT_CHARSET)

$(OBJ)\eventlog.o: eventlog.c
	$(CC) -c -o $(OBJ)\eventlog.o eventlog.c $(OPT_CHARSET)

$(OBJ)\notify.o: notify.c
	$(CC) -c -o $(OBJ)\notify.o notify.c $(OPT_CHARSET)

$(OBJ)\message.o: message.c
	$(CC) -c -o $(OBJ)\message.o message.c $(OPT_CHARSET)

$(OBJ)\jvm.o: jvm.c
	$(CC) -c -o $(OBJ)\jvm.o jvm.c $(OPT_CHARSET)

$(OBJ)\bindres.exe: $(OBJ)\bindres.o $(OBJ)\startup.o
	$(CC) -s -o $(OBJ)\bindres.exe $(OBJ)\bindres.o $(OBJ)\startup.o $(LDFLAGS) $(OPT_CHARSET)

$(OBJ)\bindres.o: bindres.c
	$(CC) -c -o $(OBJ)\bindres.o bindres.c $(OPT_CHARSET)

$(OBJ)\startup.o: startup.c
	$(CC) -c -o $(OBJ)\startup.o startup.c $(OPT_CHARSET)

$(OBJ)\JarOptimizer.class: java\JarOptimizer.java
	$(JAVAC) -Xlint:none -source 1.5 -target 1.5 -sourcepath java -d $(OBJ) java\JarOptimizer.java

$(OBJ)\PackLoader.class: java\PackLoader.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.5 -sourcepath java -d $(OBJ) java\PackLoader.java

$(OBJ)\ClassicLoader.class: java\ClassicLoader.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\ClassicLoader.java

$(OBJ)\FileLogStream.class: java\FileLogStream.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\FileLogStream.java

$(OBJ)\EventLogStream.class: java\EventLogStream.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\EventLogStream.java

$(OBJ)\EventLogHandler.class: java\EventLogHandler.java
	$(JAVAC) -Xlint:none -source 1.4 -target 1.4 -sourcepath java -d $(OBJ) java\EventLogHandler.java

$(OBJ)\UncaughtHandler.class: java\UncaughtHandler.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\UncaughtHandler.java

$(OBJ)\URLConnection.class: java\URLConnection.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\URLConnection.java

$(OBJ)\URLStreamHandler.class: java\URLStreamHandler.java
	$(JAVAC) -Xlint:none -source 1.2 -target 1.2 -sourcepath java -d $(OBJ) java\URLStreamHandler.java

