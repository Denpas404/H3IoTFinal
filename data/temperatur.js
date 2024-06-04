function getData() {
    fetch('/getData') // Replace with your endpoint
      .then(response => response.json())
      .then(data => {
        console.log(data);
        // Do something with your data
      });
  } 

  window.onload = getData; // Fetch data on page load