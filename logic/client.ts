import axios from 'axios';

const request_url = 'http://localhost:3000/';
const axiosInstance = axios.create({ baseURL: request_url });

function main() {
    axiosInstance
      .get('/')
      .then((response) => {
        console.log('Response:', response.data);
      })
      .catch((error) => {
        console.error('Error: Connection refused', error.message);
      });
}

main();