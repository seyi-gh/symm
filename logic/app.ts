import cors from 'cors';
import express from 'express';

const app = express();

app.use(cors());
app.use(express.json());

app.get('/', (req, res) => {
  res.send('This is working!!!!');
});

app.listen(5050, () => {
  console.log('Server is running on http://localhost:5050');
});