
set SWF_FILE=Fonts

p4 edit textures\*.ddx
p4 edit *.gfx
p4 edit %SWF_FILE%.dep

REM ..\..\..\..\..\tools\gfxexport\gfxexport.exe -nomipmap -i DDS -c -lwr -d textures -fonts -fntlst -fts 1024 -fns 36 -fi DDS -gradients -gi DDS -tlist %SWF_FILE%.swf
..\..\..\..\..\tools\gfxexport\gfxexport.exe -nomipmap -i DDS -c -lwr -d textures -gradients -gi DDS -tlist %SWF_FILE%.swf

..\..\..\..\..\tools\ddx\ddxconv.exe -usenativedata -forceCompress32BitTextures -format DXT1 -alphaFormat DXT5 -outsamedir -xbox -miplevels 0 -checkout -file textures\*.dds

xcopy /Y .\textures\%SWF_FILE%.gfx .

xcopy /Y .\textures\%SWF_FILE%.dep .

del /Q .\textures\*.dep
del /Q .\textures\*.dds
del /Q .\textures\*.fnt
del /Q .\textures\*.gfx

p4 add textures\*.ddx
p4 add *.gfx
p4 add %SWF_FILE%.dep

set SWF_FILE=