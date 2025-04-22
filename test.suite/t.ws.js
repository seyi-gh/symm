const WebSocket = require('ws');

// Connect to the WebSocket server
const ws = new WebSocket('ws://localhost:9000');

// When the connection is open, send a message
ws.on('open', function open() {
  console.log('[Test] Connected to WebSocket server');
  ws.send('Hello, WebSocket!');
});

// When a message is received, log it to the console
ws.on('message', function incoming(data) {
  console.log('[Test] Received message from server:', data);

  // Close the connection after receiving the message
  ws.close();
});

// Handle errors
ws.on('error', function error(err) {
  console.error('[Test] WebSocket error:', err);
});

// Handle connection close
ws.on('close', function close() {
  console.log('[Test] Connection closed');
});