@echo off
setlocal enabledelayedexpansion

for %%F in (*.obj) do (
    echo Processing %%F
    ..\..\tools\obj2cmo "%%F" -y -cmo -op
)

echo Done.
