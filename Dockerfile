# escape=`

# Stage 1: Build
FROM mcr.microsoft.com/dotnet/framework/sdk:4.8-windowsservercore-ltsc2022 AS build

SHELL ["cmd", "/S", "/C"]

# Install Visual Studio Build Tools with C++ workload
RUN curl -SL --output vs_buildtools.exe https://aka.ms/vs/17/release/vs_buildtools.exe `
    && (start /w vs_buildtools.exe --quiet --wait --norestart --nocache `
        --add Microsoft.VisualStudio.Workload.VCTools `
        --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        --add Microsoft.VisualStudio.Component.Windows11SDK.26100 `
        || IF "%ERRORLEVEL%"=="3010" EXIT 0) `
    && del /q vs_buildtools.exe

WORKDIR C:\src
COPY . .

# Build Server only (no DirectX needed for headless server)
RUN "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" `
    Server\Server.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /verbosity:minimal

# Stage 2: Runtime
FROM mcr.microsoft.com/windows/servercore:ltsc2022

WORKDIR C:\app
COPY --from=build C:\src\x64\Release\Server.exe .

EXPOSE 27015/udp

ENTRYPOINT ["Server.exe"]
