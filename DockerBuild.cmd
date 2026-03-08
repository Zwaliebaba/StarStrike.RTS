docker rm starstrike
docker build -t starstrike-server:latest .
docker run -d --name starstrike -p 7777:7777/udp -v "%cd%\Config:C:\StarStrike\config" starstrike-server:latest --config config\server-docker.yaml
docker ps --filter name=starstrike
docker logs starstrike
PAUSE
