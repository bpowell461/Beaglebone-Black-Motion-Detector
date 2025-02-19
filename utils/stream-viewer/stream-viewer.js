const dgram = require('dgram');
const http = require('http');
const { Buffer } = require('buffer');
const { Transform, Readable } = require('stream');

const BEAGLEBONE_IP = '192.168.7.1';
const BEAGLEBONE_PORT = 5555;
const LOCAL_PORT = 3000;

const server = dgram.createSocket('udp4');

const startMarker = Buffer.from([0xFF, 0xD8]);
const endMarker = Buffer.from([0xFF, 0xD9]);

class FrameTransform extends Transform {
    constructor() {
        super();
        this.frameBuffer = Buffer.alloc(0);
    }

    _transform(chunk, encoding, callback) {
        this.frameBuffer = Buffer.concat([this.frameBuffer, chunk]);

        let startMarkerIndex, endMarkerIndex;

        while ((startMarkerIndex = this.frameBuffer.indexOf(startMarker)) !== -1 &&
               (endMarkerIndex = this.frameBuffer.indexOf(endMarker, startMarkerIndex)) !== -1) {
            const completeFrame = this.frameBuffer.slice(startMarkerIndex, endMarkerIndex + endMarker.length);
            this.push(completeFrame);

            this.frameBuffer = this.frameBuffer.slice(endMarkerIndex + endMarker.length);
        }

        callback();
    }
}

const frameTransform = new FrameTransform();
frameTransform.setMaxListeners(0); // Remove the limit on the number of listeners

server.on('message', (msg) => {
    frameTransform.write(msg);
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

    const frameStream = new Readable({
        read() {}
    });

    const onData = (frame) => {
        frameStream.push(`--myboundary\r\n`);
        frameStream.push(`Content-Type: image/jpeg\r\n`);
        frameStream.push(`Content-Length: ${frame.length}\r\n\r\n`);
        frameStream.push(frame);
        frameStream.push('\r\n');
    };

    frameTransform.on('data', onData);

    frameStream.pipe(res);

    req.on('close', () => {
        frameStream.unpipe(res);
        frameStream.destroy();
        frameTransform.removeListener('data', onData);
    });
});

httpServer.listen(8080, () => {
    console.log('HTTP Server listening on port 8080');
});

httpServer.on('close', () => {
    console.log('HTTP Server has closed');
});