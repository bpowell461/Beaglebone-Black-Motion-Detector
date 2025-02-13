const dgram = require('dgram');
const http = require('http');
const { Buffer } = require('buffer');

const BEAGLEBONE_IP = '192.168.7.1';
const BEAGLEBONE_PORT = 5555;
const LOCAL_PORT = 3000;

const server = dgram.createSocket('udp4');

let frameQueue = [];
let frameCount = 0;

server.on('message', (msg) => {
    frameQueue.push(msg);
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
        'Connection': 'close',
        'Pragma': 'no-cache'
    });

    const sendFrame = () => {
        if (frameQueue.length > 0) {
            const mjpegData = frameQueue.shift();
            console.log(`Sending frame: ${mjpegData.length} bytes`);
            res.write(`--myboundary\r\n`);
            res.write(`Content-Type: image/jpeg\r\n`);
            res.write(`Content-Length: ${mjpegData.length}\r\n\r\n`);
            res.write(mjpegData);
            res.write('\r\n');
        } else {
            console.log('No frames in queue');
        }
    };

    // Use setInterval to repeatedly call sendFrame based on the frame rate
    setInterval(sendFrame, 10);
});

httpServer.listen(8080, () => {
    console.log('HTTP Server listening on port 8080');
});