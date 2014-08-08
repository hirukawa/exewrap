@ECHO OFF

SETLOCAL
SET VCDIR=C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC

IF "%1" == "clean" (
nmake /nologo clean
IF ERRORLEVEL 1 GOTO ERR
goto SUCCESS
)

call "%VCDIR%\vcvarsall.bat" x86
nmake /nologo IMAGE_X86
IF ERRORLEVEL 1 GOTO ERR

call "%VCDIR%\vcvarsall.bat" x64
nmake /nologo IMAGE_X64
IF ERRORLEVEL 1 GOTO ERR

call "%VCDIR%\vcvarsall.bat" x86
nmake /nologo EXEWRAP_X86
IF ERRORLEVEL 1 GOTO ERR

call "%VCDIR%\vcvarsall.bat" x64
nmake /nologo EXEWRAP_X64
IF ERRORLEVEL 1 GOTO ERR

:SUCCESS
echo 正常に終了しました。
goto END

:ERR
echo エラーが発生しました。
goto END

:END

ENDLOCAL
