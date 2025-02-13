const dgram = require('dgram');
const http = require('http');
const { Buffer } = require('buffer');

const BEAGLEBONE_IP = '192.168.7.1';
const BEAGLEBONE_PORT = 5555;
const LOCAL_PORT = 3000;

const server = dgram.createSocket('udp4');

let frameQueue = [];
let frameBuffer = Buffer.alloc(0);

server.on('message', (msg) => {
    // Append the incoming message to the frame buffer
    frameBuffer = Buffer.concat([frameBuffer, msg]);

    // Check if the frame is complete (each frame starts with 0xFFD8 and ends with 0xFFD9)
    const startMarker = Buffer.from([0xFF, 0xD8]);
    const endMarker = Buffer.from([0xFF, 0xD9]);
    let startMarkerIndex, endMarkerIndex;

    while ((startMarkerIndex = frameBuffer.indexOf(startMarker)) !== -1 && 
           (endMarkerIndex = frameBuffer.indexOf(endMarker, startMarkerIndex)) !== -1) {
        // Extract the complete frame
        const completeFrame = frameBuffer.slice(startMarkerIndex, endMarkerIndex + endMarker.length);
        frameQueue.push(completeFrame);

        // Remove the complete frame from the buffer
        frameBuffer = frameBuffer.slice(endMarkerIndex + endMarker.length);
    }
});

server.on('listening', () => {
    const address = server.address();
    console.log(`UDP Server listening on ${address.address}:${address.port}`);
});

server.bind(LOCAL_PORT, BEAGLEBONE_IP);

const httpServer = http.createServer((req, res) => {
    res.writeHead(200, {
        'Content-Type': 'multipart/x-mixed-replace; boundary=--myboundary',
        'Cache-Control': 'no-cache',
        'Connection': 'keep-alive',
        'Pragma': 'no-cache'
    });

    const sendFrame = () => {
        if (frameQueue.length > 0) {
            const mjpegData = frameQueue.shift();
            res.write(`--myboundary\r\n`);
            res.write(`Content-Type: image/jpeg\r\n`);
            res.write(`Content-Length: ${mjpegData.length}\r\n\r\n`);
            res.write(mjpegData);
            res.write('\r\n');
        }
    };

    // Use setInterval to repeatedly call sendFrame based on the frame rate
    setInterval(sendFrame, 30);
});

httpServer.listen(8080, () => {
    console.log('HTTP Server listening on port 8080');
});