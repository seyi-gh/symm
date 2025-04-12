import dotenv from 'dotenv';
dotenv.config();

export default {
  websocketportserver: Number(process.env.websocketportserver)
};
