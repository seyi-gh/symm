import cors from 'cors';
import { v4 as uuidv4 } from 'uuid';
import bodyParser from 'body-parser';
import { sck, responses } from './c.wss';
import express, { Request, Response } from 'express';

const app = express();

//! temporal for testing purposes
app.use(cors());
app.use(bodyParser.json()); 

const proxyController = (basePath: string) => {
  app.all('*', async (req: Request, res: Response) => {
    if (!sck || sck.readyState !== sck.OPEN)
      return res.status(503).json({ error: 'error connecting the proxy' });

    const reqPayload = { uuid: uuidv4(), target: basePath + req.path, pldata: req };

    sck.send(JSON.stringify(reqPayload));

    const iresponse = await new Promise(resolve => {
      responses.set(reqPayload.uuid, resolve);
      setTimeout(() => {
        if (responses.has(reqPayload.uuid)) {
          responses.delete(reqPayload.uuid);
          resolve({ error: true, message: 'Timeout' });
        }
      }, 5000);
    });
  
    res.json(iresponse);
  });

  return app;
};

export default proxyController;