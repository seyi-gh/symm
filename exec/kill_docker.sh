DOCKER_PID=$(docker ps --format '{{.ID}} {{.Ports}}' | grep ":${PORT}->" | awk '{print $1}')
if [ -n "$DOCKER_PID" ]; then
  docker stop "$DOCKER_PID"
  sleep 1
else
  echo "[✓] No Docker container using port $PORT."
fi