@echo off
setlocal EnableDelayedExpansion

REM build-Verzeichnis erstellen
if not exist build (
	echo Creating build directory...
	mkdir build
	echo ...done
)
echo.

REM Assembler Dateien kompillieren
for %%p in (source/*.asm) do (
	echo Compiling "%%p"...
	nasm -O0 -f win32 -I"include" -o "build/%%~np.o" "source/%%p"
	echo ...done
)

REM C-Dateien compillieren
for %%p in (source/*.c) do (
	echo Compiling "%%p"...
	gcc -O0 -c "source/%%p" -I"include" -o "build/%%~np.o" -std=c99
	echo ...done
)

REM Cpp-Dateien compillieren
for %%p in (source/*.cpp) do (
	echo Compiling "%%p"...
	g++ -O3 -c "source/%%p" -I"include" -o "build/%%~np.o" --std=gnu++11 -Wall -Wextra
	echo ...done
)

echo.

REM Liste von Object Files kreieren
echo Collecting object files...
set objectFiles=
for %%p in (build/*.o) do set objectFiles=!objectFiles! "build/%%p"
echo ...done

echo.

echo Linking with gcc...
g++ -O3 -m32 -o result.exe %objectFiles% && echo ..done
echo.
pause