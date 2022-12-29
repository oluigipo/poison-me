@echo off
setlocal

set SOURCES=src/main.c src/os.c src/lang_c.c
set WARNINGS=-Wall -Wno-unused-function -Wno-gnu-alignof-expression -Wno-missing-braces -Wno-logical-op-parentheses
set OPTM=
set DEBUG=-g -DDEBUG

clang -fuse-ld=lld -o aaa.exe %SOURCES% %DEBUG% %WARNINGS% %OPTM%

endlocal
@echo on
