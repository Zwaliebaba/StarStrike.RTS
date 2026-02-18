# escape=`
FROM mcr.microsoft.com/windows/nanoserver:ltsc2022

ARG CONFIG=Release

WORKDIR C:\app

COPY x64\${CONFIG}\Server.exe .

EXPOSE 27015/udp

ENTRYPOINT ["C:\\app\\Server.exe"]
