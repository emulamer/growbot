const dgram = require('dgram');
const server = dgram.createSocket('udp4');

server.on('error', (err) => {
  console.log(`server error:\n${err.stack}`);
  server.close();
});

server.on('message', (msg, rinfo) => {
    process.stdout.write(`${msg}`);
 // console.log(`server got: ${msg} from ${rinfo.address}:${rinfo.port}`);
});

server.on('listening', () => {
  const address = server.address();
  console.log(`server listening ${address.address}:${address.port}`);
});
console.log(process.argv);
if (process.argv.length > 2 && process.argv[2] == "ws") {
  var socket = new WebSocket('ws://growbot-doser:8118');
  socket.binaryType = 'arraybuffer';
  socket.onmessage = (msg) => {
    console.log("got message: " + msg.data.toString());
  }
  socket.onopen(e => {
    console.log("onopen");
    var bytes = new Uint8Array(2);
    bytes[0] = 0xB1;
    bytes[1] = 0;
    socket.send(bytes);
  })
}
if (process.argv.length > 2 && process.argv[2] == "doser") {
  server.bind(44445);  
} else {
  server.bind(44444);
}