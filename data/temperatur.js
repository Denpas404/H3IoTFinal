let jsonData = [];

const mockData = {
  // ... your mock data (optional for testing)
};

function getData() {
  fetch('/getData') // Replace with your endpoint
    .then(response => response.json())
    .then(data => {
      jsonData = data;
      console.log("Data: "+ data);
      console.log("jsonData: " + jsonData);
      console.log(typeof jsonData);

      // Now that data is available, initialize the chart
      Highcharts.chart('chart-temperature', {
        chart: {
          type: 'spline'
        },
        title: {
          text: 'Monthly Average Temperature'
        },
        xAxis: {
          categories: data.map(item => item.date), // Use 'date' property directly
          accessibility: {
            description: 'Months of the year'
          }
        },
        yAxis: {
          title: {
            text: 'Temperature'
          },
          labels: {
            format: '{value}°'
          }
        },
        tooltip: {
          crosshairs: true,
          shared: true
        },
        plotOptions: {
          spline: {
            marker: {
              radius: 4,
              lineWidth: 1,
              lineColor: '#666666'
            }
          }
        },
        series: [{
          name: 'Tokyo',
          marker: {
            symbol: 'square'
          },
          // Initially empty data for placeholder (optional)
          data: [] 
        }]
      });

      // Update data in the chart after fetching (optional)
      data.map(item => item.temperature)  // Extract temperature data
        .forEach((temp, index) => chart.series[0].data[index] = temp); // Update each data point
      chart.redraw(); // Redraw the chart with updated data
    });
}

window.onload = getData; // Fetch data on page load
