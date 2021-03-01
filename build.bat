@echo off
set CC="C:\Program Files\LLVM\bin\clang-cl"
if "%1" == "debug" (
	%CC% -Zi windows.c -o aseprite_ssd1306
	exit /b 0
)
%CC% -Wno-deprecated-declarations -DRELEASE=1 -O2 windows.c -o aseprite_ssd1306

