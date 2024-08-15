import * as dgram from 'dgram';

export class UdpJsonClient {
    private socket: dgram.Socket;

    constructor(private listenPort: number, private cb: (msg: any) => void) {
        this.socket = dgram.createSocket('udp4');

        this.socket.on('message', (msg, rinfo) => {
            try {
                const jsonMessage = JSON.parse(msg.toString());
                if (cb) {
                    cb(jsonMessage);
                }
                console.log(`Received message from ${rinfo.address}:${rinfo.port} -`, jsonMessage);
            } catch (e) {
                console.error('Failed to parse incoming message as JSON:', msg.toString());
            }
        });

        this.socket.bind(this.listenPort, () => {
            console.log(`Listening for UDP packets on port ${this.listenPort}`);
        });
    }

    sendJsonMessage(jsonString: string, targetIp: string, targetPort: number): void {
        const messageBuffer = Buffer.from(jsonString);
        //this.socket.setBroadcast(true);
        this.socket.send(messageBuffer, 0, messageBuffer.length, targetPort, targetIp, (err) => {
            if (err) {
                console.error('Failed to send UDP message:', err);
            } else {
                console.log(`Sent message to ${targetIp}:${targetPort}`);
            }
        });
    }
}