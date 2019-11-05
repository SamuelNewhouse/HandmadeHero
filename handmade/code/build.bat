@echo off

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
cl -D HANDMADE_INTERNAL=1 -D HANDMADE_SLOW=1 -D HANDMADE_WIN32=1 -FC -Zi w:\handmade\code\win32_handmade.cpp user32.lib Gdi32.lib
popd