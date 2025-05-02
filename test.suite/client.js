const axios = require('axios');

// get localhost:3000
const url = 'http://localhost:3000';
const axiosInstance = axios.create({
  baseURL: url
});

axiosInstance
  .get('/')
  .then((response) => {
    console.log('Response:', response.data);
  })
  .catch((error) => {
    console.error('Error:', error);
  });