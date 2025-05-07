/*
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
*/

import axios from 'axios';
import { WebSocket } from 'ws';

const ws = new WebSocket('ws://localhost:9000');


async function get_request(url: string) {
  return axios.get(url).then((response) => {
    console.log('Response:', response.data);
    return response.data;
  }).catch((error) => {
    console.error('Error:', error.message);
    return 'Error: Bad request';
  });
}
function get_request_path(request: string) {
  // fin the request host "Host: localhost:3000" and the path "GET (/) HTTP/1.1"
  const host = request.match(/Host: (.+)/);
  const path = request.match(/GET (.+) HTTP/);
  if (host && path) {
    //const host_url = host[1].trim();
    const host_url = 'localhost:5050';
    const path_url = path[1].trim();
    return `http://${host_url}${path_url}`;
  }
  return '';
}
async function process_request(request: string) {
  const url = get_request_path(request);
  console.log('Request URL:', url);
  const response = await get_request(url);
  return response;
}


ws.on('open', () => {
  console.log('WebSocket connection opened');
});

ws.on('message', async (data) => {
  console.log('Received:', data.toString());
  const response = await process_request(data.toString());
  ws.send(response);
});
ws.on('close', () => {
  console.log('WebSocket connection closed');
});
ws.on('error', (err) => {
  console.error('WebSocket error:', err);
});