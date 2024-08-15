import { UdpJsonClient } from './UdpJsonClient';


export interface GbMsgInterface {
    msgId?: string;
    msgType?: string;
    nodeType?: string;
    nodeId?: string;
    replyTo?: string;
    [key: string]: any;
}

const listenPort = 45678;
const targetIp = '192.168.1.255';
const targetPort = 45678;

const client = new UdpJsonClient(listenPort, m => {
    console.log('Received message:', m);
    //return;
    if (m?.msgType === 'DoserEndedMsg') {
        console.log('Received DoserDosePortMsg:', m);
        let p = m.port+1;
        if (p > 6) { p = 1; }
        const startDose2 : GbMsgInterface = {
            msgId: '123',
            msgType: 'DoserDosePortMsg',
            nodeType: 'growbot-accusquirt',
            nodeId: 'growbot-accusquirt',
            port: p,
            targetMl: 50
        }
        console.log("Sending next dose to port: ", p);
        const jsonString = JSON.stringify(startDose2);

        client.sendJsonMessage(jsonString, targetIp, targetPort);
    }
});

const startDose : GbMsgInterface = {
    msgId: '123',
    msgType: 'DoserDosePortMsg',
    nodeType: 'growbot-accusquirt',
    nodeId: 'growbot-accusquirt',
    port: 5,
    targetMl: 50
}
const jsonString = JSON.stringify(startDose);

client.sendJsonMessage(jsonString, targetIp, targetPort);
// setTimeout(() => {
//     process.exit();
// }, 2000);