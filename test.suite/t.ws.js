// create a simple ws with basic debug console.logs, in port 5000
const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 5000 });

wss.on('connection', function connection(ws) {
  console.log('Client connected');
  ws.on('message', function incoming(message) {
    console.log('received: %s', message);
    try {
      ws.send('Hello world!');
      console.log('sent: echo: %s', message);
    } catch (err) {
      console.error('Error sending message:', err);
    }
  });
  ws.on('close', function close() {
    console.log('Client disconnected');
  });
});