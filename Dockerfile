# StarStrike.RTS — Game Server Container
# Build: docker build -t starstrike-server:latest .
# Run:   docker run -d --name starstrike -p 7777:7777/udp -v "%cd%\Config:C:\StarStrike\config" starstrike-server:latest --config config\server-docker.yaml
#
# Requires: Docker Desktop in Windows containers mode.
# Database: NOT included — connects to host SQL Server via host.docker.internal.

# Full Windows Server image — must be close to the host OS build to avoid
# API mismatch crashes under Hyper-V isolation.
# Host build 10.0.26200 → use ltsc2025 (build 26100), NOT ltsc2022 (build 20348).
FROM mcr.microsoft.com/windows/server:ltsc2025

SHELL ["powershell", "-Command", "$ErrorActionPreference = 'Stop';"]

# Install ODBC Driver 18 for SQL Server (required by Database.cpp ODBC layer)
ADD https://go.microsoft.com/fwlink/?linkid=2266337 C:/temp/msodbcsql.msi
RUN Start-Process msiexec.exe -ArgumentList '/i', 'C:\temp\msodbcsql.msi', \
    'IACCEPTMSODBCSQLLICENSETERMS=YES', '/qn' -Wait -NoNewWindow; \
    Remove-Item C:\temp -Recurse -Force

WORKDIR C:/StarStrike

# Copy Debug build artifacts + MSVC debug CRT (not included in Server Core)
COPY x64/Debug/Server.exe          .
COPY x64/Debug/yaml-cppd.dll       .
COPY x64/Debug/ucrtbased.dll       .
COPY x64/Debug/vcruntime140d.dll   .
COPY x64/Debug/vcruntime140_1d.dll .
COPY x64/Debug/msvcp140d.dll       .
COPY x64/Debug/concrt140d.dll      .

# Copy default config + schema
COPY Config/server.yaml        config/
COPY Config/schema.sql         config/

# Game traffic (UDP)
EXPOSE 7777/udp

# Graceful shutdown: Docker sends CTRL_SHUTDOWN_EVENT to Windows console apps
# on `docker stop`. SetConsoleCtrlHandler in main.cpp already handles this.
# CMD provides the default config path; override at runtime with:
#   docker run ... starstrike-server:latest --config config\server-docker.yaml

ENTRYPOINT ["C:\\StarStrike\\Server.exe"]
CMD ["--config", "config\\server.yaml"]
