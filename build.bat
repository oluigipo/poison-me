@echo off

set SOURCES=src/main.c src/os.c src/compiler.c
set WARNINGS=-Wall -Wno-unused-function -Wno-gnu-alignof-expression
set OPTM=
set DEBUG=-g

clang %SOURCES% -o aaa.exe -fuse-ld=lld %DEBUG% %WARNINGS% %OPTM%

@echo on
