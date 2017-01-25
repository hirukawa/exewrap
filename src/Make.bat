@ECHO OFF

SETLOCAL
SET DDK=C:\WinDDK
SET PATH_ORIGINAL=%PATH%

REM clean ############################################################

CALL :SETENV_X86
IF "%1" == "clean" (
nmake /nologo clean
IF ERRORLEVEL 1 GOTO ERR
goto SUCCESS
)


REM IMAGE ############################################################

CALL :SETENV_X86
nmake /nologo IMAGE_X86
IF ERRORLEVEL 1 GOTO ERR

CALL :SETENV_X64
nmake /nologo IMAGE_X64
IF ERRORLEVEL 1 GOTO ERR

REM EXEWRAP ##########################################################

CALL :SETENV_X86
nmake /nologo EXEWRAP_X86
IF ERRORLEVEL 1 GOTO ERR

CALL :SETENV_X64
nmake /nologo EXEWRAP_X64
IF ERRORLEVEL 1 GOTO ERR


REM SKIP TRACE VERSION
goto SUCCESS

REM TRACE IMAGE ######################################################

CALL :SETENV_X86
nmake /nologo TRACE_IMAGE_X86
IF ERRORLEVEL 1 GOTO ERR

CALL :SETENV_X64
nmake /nologo TRACE_IMAGE_X64
IF ERRORLEVEL 1 GOTO ERR

REM TRACE EXEWRAP ####################################################

CALL :SETENV_X86
nmake /nologo TRACE_EXEWRAP_X86
IF ERRORLEVEL 1 GOTO ERR

nmake /nologo JREMIN_X86
IF ERRORLEVEL 1 GOTO ERR

CALL :SETENV_X64
nmake /nologo TRACE_EXEWRAP_X64
IF ERRORLEVEL 1 GOTO ERR

nmake /nologo JREMIN_X64
IF ERRORLEVEL 1 GOTO ERR


REM ##################################################################

:SUCCESS
echo 正常に終了しました。
goto END

:ERR
echo エラーが発生しました。
goto END


REM SETENV ###########################################################

:SETENV_X86
SET PATH=%DDK%\bin\x86\x86;%DDK%\bin\x86;%PATH_ORIGINAL%
SET INCLUDE=%DDK%\inc\crt;%DDK%\inc\api
SET LIB=%DDK%\lib\win7\i386;%DDK%\lib\crt\i386
exit /b

:SETENV_X64
SET PATH=%DDK%\bin\x86\amd64;%DDK%\bin\x86;%PATH_ORIGINAL%
SET INCLUDE=%DDK%\inc\crt;%DDK%\inc\api
SET LIB=%DDK%\lib\win7\amd64;%DDK%\lib\crt\amd64
exit /b


REM END ##############################################################

:END
ENDLOCAL
