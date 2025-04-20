const axios = require('axios');

axios.get('http://localhost:3000')
  .then(response => {
    // Handle successful response
    console.log('Data:', response.data);
    console.log('Status:', response.status);
  })
  .catch(error => {
    // Handle errors
    console.error('Error:', error);
  });