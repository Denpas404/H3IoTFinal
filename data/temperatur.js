//Create a const button on Id: refreshButton
const refreshButton = document.getElementById("refreshButton");
const deleteNetworkBtn = document.getElementById("deleteNetworkBtn");
const deleteDataLogBtn = document.getElementById("deleteDataLogBtn");

//Add an event listener to the button
refreshButton.addEventListener("click", function () {
  window.location.reload();
});

deleteNetworkBtn.addEventListener("click", function () {
  deleteNetworkSetting();
});

deleteDataLogBtn.addEventListener("click", function () {
  deleteDataLog();
});



function getData() {
  fetch("/getData") // Replace with your endpoint
    .then((response) => response.json())
    .then((data) => {
      console.log("Fetched data:", data); // Log the parsed data

      const temperatures = [];
      const dates = [];

      // Loop through each data object
      for (const item of data.data) {
        const roundedTemperature = Math.round(item.temperature * 100) / 100;
        temperatures.push(roundedTemperature);
        //temperatures.push(item.temperature);
        dates.push(item.date);
      }

      console.log("Temperatures:", temperatures);
      console.log("Dates:", dates);

      // Use these arrays in Highcharts
      Highcharts.chart("chart-temperature", {
        chart: {
          backgroundColor: "#212529",
          type: "spline",
        },
        title: {
          text: "Average Temperature",
          style: {
            color: "white",
          },
        },
        xAxis: {
          categories: dates,
          accessibility: {
            description: "Months of the year",
          },
          labels: {
            style: {
              color: "white",
            },
          },
        },
        yAxis: {
          gridLineColor: "yellow",
          gridLineWidth: 0.1,
          title: {
            text: "Temperature",
            style: {
              color: "white",
            },
          },
          labels: {
            format: "{value}°",
            style: {
              color: "white",
            },
          },
        },
        tooltip: {
          crosshairs: true,
          shared: true,
        },
        plotOptions: {
          spline: {
            marker: {
              radius: 4,
              lineWidth: 1,
              lineColor: "white",
              fillColor: "white",
              symbol: "circle",
            },
          },
        },
        series: [
          {
            name: "Temperature (°C)",
            data: temperatures,
            color: "white",
          },
        ],
        legend: {
          itemStyle: {
            color: "white",
          },
        },
      });
    })
    .catch((error) => {
      console.error("Error processing data:", error);
      // Handle errors here (e.g., display error message)
    });
}

function deleteNetworkSetting() {
  fetch("/deleteNetwork") // Replace with your endpoint
    .then((data) => {
      console.log("Data deleted:", data);
      window.location.reload();
    })
    .catch((error) => {
      console.error("Error processing data:", error);
      // Handle errors here (e.g., display error message)
    });
    console.log("Data deleted");
}


function deleteDataLog(){
  fetch("/deleteDataLog") // Replace with your endpoint
  .then((data) => {
    console.log("Data deleted:", data);
    window.location.reload();
  })
  .catch((error) => {
    console.error("Error processing data:", error);
    // Handle errors here (e.g., display error message)
  });
  console.log("Data deleted");
  window.location.reload();
}


window.onload = getData; // Fetch data on page load
//Failed to open file for reading