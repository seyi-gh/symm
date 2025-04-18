#!/bin/bash
PORT=9001

#echo "[+] Checking if port $PORT is in use..."

# Kill local processes using port 9000
if lsof -i tcp:$PORT &>/dev/null; then
  #echo "[!] Port $PORT is in use by a local process. Killing it..."
  fuser -k ${PORT}/tcp 2>/dev/null || true
  #echo "[…] Waiting 2 seconds to ensure port is released..."
  sleep 2
else
  echo "[✓] No local process is using port $PORT."
fi

# Stop any Docker container using port 9000
DOCKER_PID=$(docker ps --format '{{.ID}} {{.Ports}}' | grep ":${PORT}->" | awk '{print $1}')
if [ -n "$DOCKER_PID" ]; then
  #echo "[!] Docker container using port $PORT detected: $DOCKER_PID. Stopping it..."
  docker stop "$DOCKER_PID"
  #echo "[…] Waiting 2 seconds to ensure Docker port is released..."
  sleep 2
else
  echo "[✓] No Docker container using port $PORT."
fi

# Double-check if port 9000 is still in use
while lsof -i tcp:$PORT &>/dev/null; do
  #echo "[!] Port $PORT is still in use. Retrying after 2 seconds..."
  fuser -k ${PORT}/tcp 2>/dev/null || true
  sleep 2
done

# Clean up previous build files if necessary
#echo "[+] Cleaning previous build..."
make clean 2>/dev/null

# Build the proxy server
#echo "[+] Building proxy..."
make

if [ ! -f "./proxy" ]; then
  #echo "[!] Build failed. Exiting."
  exit 1
fi

#echo "[✓] Build successful. Running proxy locally on port $PORT..."
#echo "[ℹ] Ensure your WebSocket server is running at ws://localhost:5000"

./proxy


#docker build -t cpp-proxy .
# Kill anything using port 9000 (just in case)
#sudo fuser -k 9000/tcp 2>/dev/null || true
# Stop any container already using port 9000
#running_container=$(docker ps --format '{{.ID}} {{.Ports}}' | grep '0.0.0.0:9000' | awk '{print $1}')
#if [ -n "$running_container" ]; then
#  echo "Stopping container using port 9000: $running_container"
#  docker stop "$running_container"
#  sleep 1
#fi
#echo "Waiting for 5 seconds before starting the container..."
#sleep 5
# Run with host networking (don't use -p with --network host)
#docker run --rm --network host cpp-proxy