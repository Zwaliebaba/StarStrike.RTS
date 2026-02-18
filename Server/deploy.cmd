@echo off
echo === StarStrike Server Deploy ===

pushd %~dp0..

set CONFIG=Release
if /i "%~1"=="DEBUG" set CONFIG=Debug

if not exist "x64\%CONFIG%\Server.exe" (
    echo Error: x64\%CONFIG%\Server.exe not found. Build the Server project in %CONFIG%|x64 first.
    popd
    exit /b 1
)

echo Creating Docker image (%CONFIG%)...
set CONFIG=%CONFIG%&& docker compose -f Server/docker-compose.yml build
if %ERRORLEVEL% neq 0 (
    echo Docker build failed.
    popd
    exit /b %ERRORLEVEL%
)

echo Starting server container...
set CONFIG=%CONFIG%&& docker compose -f Server/docker-compose.yml up -d
if %ERRORLEVEL% neq 0 (
    echo Failed to start container.
    popd
    exit /b %ERRORLEVEL%
)

echo.
echo Server deployed. Listening on UDP port 27015.
echo.
echo View logs:
echo   docker compose -f Server/docker-compose.yml logs -f
echo.
echo Stop:
echo   docker compose -f Server/docker-compose.yml down
popd
