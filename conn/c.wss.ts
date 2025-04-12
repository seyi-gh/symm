import chalk from 'chalk';
import config from '../config';
import { WebSocketServer, WebSocket } from 'ws';

var sck: WebSocket|null = null;
const responses = new Map<string, (data: object) => void>();
const wss = new WebSocketServer({ port: config.websocketportserver });

wss.on('connection', socket => {
  console.log(chalk.cyan('[wss.on] connection successful'));

  sck = socket;

  socket.on('message', message => {
    //? test this
    const response = JSON.parse(message.toString());
    const resolve = responses.get(response.id);
    if (resolve) {
      resolve(response.data || response);
      responses.delete(response.id);
    }
  });

  socket.on('close', () => {
    console.log(chalk.magenta('[wss.on] identity closed'));
    sck = null;
  });
});

export default wss;
export { sck, wss, responses };