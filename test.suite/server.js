const WebSocket = require('ws');
const ws = new WebSocket('ws://localhost:9000');
const prompt = require('prompt-sync')({ sigint: true });
const readline = require('readline');

ws.on('message', function incoming(data) {
  console.log('received: %s', data.toString());
});

ws.on('close', () => {
  console.log('Connection closed');
});

ws.on('error', (err) => {
  console.error('WebSocket error:', err);
});

let interval;
ws.on('open', () => {
  interval = setInterval(() => {
    //if (ws.readyState === WebSocket.OPEN) {
      ws.send('ping');
    //}
  }, 100);
});

ws.on('close', () => {
  clearInterval(interval);
});
