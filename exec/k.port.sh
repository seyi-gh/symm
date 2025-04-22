if lsof -i tcp:$PORT &>/dev/null; then
  fuser -k ${PORT}/tcp 2>/dev/null || true
  sleep 1
else
  echo "[✓] No local process is using port $PORT."
fi