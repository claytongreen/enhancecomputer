@echo off

REM

pushd x64\Debug

cl /nologo /Z7 ..\..\main.cpp /link /SUBSYSTEM:CONSOLE

popd
