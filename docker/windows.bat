@echo off
cd /root
premake5 --os=windows --arch=x86 gmake2

cd projects\windows\gmake2

set LIBRARY_PATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.17763.0\ucrt\x86
set CPLUS_INCLUDE_PATH=C:\Program Files (x86)\Windows Kits\10\Include\10.0.17763.0\ucrt

msbuild.exe -t:Rebuild -p:Configuration=Release -p:Platform=Win32
xcopy /Y **\*.dll /output
dir /output

