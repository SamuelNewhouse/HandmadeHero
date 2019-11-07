@echo off

set CommonCompilerFlags= -MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -D HANDMADE_INTERNAL=1 -D HANDMADE_SLOW=1 -D HANDMADE_WIN32=1 -FC -Z7 -Fmwin32_handmade.map
set CommonLinkerFlags= -opt:ref user32.lib Gdi32.lib
set SixtyFourBitLink= -subsystem:windows,5.02
set ThirtyTwoBitBitLink= -subsystem:windows,5.01

REM TODO - can we just build both with one exe?

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

cl %CommonCompilerFlags% w:\handmade\code\win32_handmade.cpp /link %CommonLinkerFlags% %SixtyFourBitLink%
REM cl %CommonCompilerFlags% w:\handmade\code\win32_handmade.cpp /link %CommonLinkerFlags% %ThirtyTwoBitBitLink%

popd