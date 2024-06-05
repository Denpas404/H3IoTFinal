// WebSocket connection to /wsdev
var socket = new WebSocket("ws://ddev-esp32.local/wsden");


// WebSocket message event handler
socket.onmessage = function (event) {
  document.getElementById("liveData").innerText = event.data; // Update display with received data
};