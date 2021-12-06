#include <Arduino.h>
#include <ArduinoJson.h>
#include <GbMsg.h>
//#include <WebSocketsServer.h>
#include <MessageParser.h>



/* when messages need to be shrunk, thoughts on bin packet header:
0-3     4   uint32  message length
4-7     4   uint32  header size
8-11    4   uint32  message id
12-15   4   uint32  message type
16-19   4   uint32  node id length
20-n    ?   string  node id
*/

#pragma once
#define MAX_PAYLOAD_SIZE 1420
#define DEFAULT_REPLY_TIMEOUT_MS 5000

class MessengerServerCore;
class MessageWrapper {
    public:
        unsigned long num;
        MessengerServerCore* srv;
        GbMsg* message;
        bool reply(GbMsg& msg); 
};
class _MessengerHandlerRef {
    public:
    std::function<void(MessageWrapper&)> handler;
    String filter;
    MessageWrapper wrapMessage(MessengerServerCore* server, GbMsg* msg, IPAddress num) {
        MessageWrapper wrap;
        wrap.message = msg;
        wrap.num = num;
        wrap.srv = server;
        return wrap;
    }
};
class _ReplyHandlerRef {
    public:
        String msgId;
        std::function<void(bool, GbResultMsg*)> handler;
        unsigned long startedAt = 0;
        int timeout;
        _ReplyHandlerRef(String msgId, std::function<void(bool, GbResultMsg*)> handler, int timeout) {
            this->msgId = msgId;
            this->handler = handler;
            this->timeout = timeout;
            this->startedAt = millis();
        }
        void success(GbResultMsg* msg) {
            this->handler(true, msg);
        }

        void fail() {
            this->handler(false, NULL);
        }

        bool isTimedOut() {
            return (millis() - startedAt) > timeout;
        }
};

class MessengerServerCore {
    protected:
        String nodeId;
        String nodeType;
        std::vector<_MessengerHandlerRef*> handlers;
        std::vector<_ReplyHandlerRef*> replyHandlers;
        void handleMessage(char* payload, int length, unsigned long num) {
            GbMsg* msg = parseGbMsg((char*)payload, length);
            if (msg == NULL) {
                dbg.printf("parsing failed, msg: %s\n", payload);
                return;
            }
            if (msg->msgType().equals(NAMEOF(GbResultMsg))) {
                GbResultMsg* res = (GbResultMsg*)msg;
                std::vector<_ReplyHandlerRef*>::iterator iter = replyHandlers.begin();
                while (iter != replyHandlers.end())
                {
                    if  ((*iter)->msgId.equals(res->replyTo())) {
                        (*iter)->success(res);
                        iter = replyHandlers.erase(iter);
                    } else {
                        iter++;
                    }
                }
            }
            for (auto handler: handlers) {
                if (handler->filter.equals(msg->myType())) {
                    MessageWrapper wrappedMsg = handler->wrapMessage(this, msg, num);
                    handler->handler(wrappedMsg);
                }
            }
            delete msg;
        }
        
    public:
        MessengerServerCore() {
            this->nodeId = GB_NODE_ID;
            this->nodeType = GB_NODE_TYPE;
        }

        void onMessage(String typeName, std::function<void(MessageWrapper&)> handler) {
            _MessengerHandlerRef* hr = new _MessengerHandlerRef();
            hr->filter = typeName;
            hr->handler = handler;
            handlers.push_back(hr);
        }

        virtual bool broadcast(GbMsg& msg) = 0;

        bool broadcastWithReply(GbMsg& msg, std::function<void(bool, GbResultMsg*)> replyHandler, int timeoutMs = DEFAULT_REPLY_TIMEOUT_MS) {
            if (!this->broadcast(msg)) {
                return false;
            }
            replyHandlers.push_back(new _ReplyHandlerRef(msg.msgId(), replyHandler, timeoutMs));
        }

        virtual bool connectionSend(unsigned long num, GbMsg& msg) = 0;

        virtual void handle() {
            std::vector<_ReplyHandlerRef*>::iterator iter = replyHandlers.begin();
            while (iter != replyHandlers.end())
            {
                if  ((*iter)->isTimedOut()) {
                    (*iter)->fail();
                    iter = replyHandlers.erase(iter);
                } else {
                    iter++;
                }
            }
        }
};

// class WSMessengerServer : public MessengerServerCore {
//     private:
        
//         WebSocketsServer* webSocket;
//         void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {   
//             if (type == WStype_CONNECTED) {
//                 dbg.println("got a websocket connection");
//                 return;
//             }
//             if (type == WStype_DISCONNECTED) {
//                 dbg.println("a websocket disconnected");
//                 return;
//             }
//             if (type != WStype_TEXT) {
//                 dbg.println("got a non-text websocket message, ignoring it");
//                 return;
//             }
//             if (length < 2) {
//                 dbg.dprintln("too short ws text message, ignoring");
//                 return;
//             }
//             if (length > 500) {
//                 dbg.printf("got too big a websocket message, it was %d bytes\n", length);
//             }
//             handleMessage((char*)payload, length, num);
            
//         }
//     public:
//         WSMessengerServer(int port) : MessengerServerCore() {
//             webSocket = new WebSocketsServer(port);
//             webSocket->onEvent(std::bind(&WSMessengerServer::webSocketEvent, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4));
//             webSocket->begin();
//         }

//         bool broadcast(GbMsg& msg) {
//             String json = msg.toJson();
//             return webSocket->broadcastTXT(json);
//         }

//         bool connectionSend(unsigned long num, GbMsg& msg) {
//             String json = msg.toJson();
//             return webSocket->sendTXT(num, json);
//         }

//         void handle() {
//             webSocket->loop();
//         }

// };

class UdpMessengerServer : public MessengerServerCore {
    private:
        WiFiUDP udp;
        int port;
        unsigned char buffer[MAX_PAYLOAD_SIZE];
    public:
        UdpMessengerServer(int port) : MessengerServerCore() {
            this->port = port;
            
        }
        void init() {
            udp.begin(port);
        }
        bool broadcast(GbMsg& msg) {
            if (WiFi.isConnected()) {
                String json = msg.toJson();
                IPAddress ip = WiFi.localIP();
                IPAddress bcast = IPAddress(ip[0], ip[1], ip[2], 255);
                udp.beginPacket(bcast, port);
                udp.write((const uint8_t *)json.c_str(), json.length());
                if (udp.endPacket() != 0) {
                    return false;
                }
                delay(10);
                return true;
            } else {
                return false;
            }
        }

        bool connectionSend(unsigned long num, GbMsg& msg) {
            if (WiFi.isConnected()) {
                String json = msg.toJson();
                IPAddress toIp = (IPAddress)num;
                udp.beginPacket(toIp, port);
                udp.write((const uint8_t *)json.c_str(), json.length());
                if (udp.endPacket() != 0) {
                    return false;
                }
                delay(10);
                return true;
            } else {
                return false;
            }
        }

        void handle() {
            MessengerServerCore::handle();
            int pktLen = udp.parsePacket();
            if (pktLen == 0) {
                return;
            }
            memset((void*)buffer, 0, MAX_PAYLOAD_SIZE);
            int read = udp.read(buffer, pktLen);
            if (read != pktLen) {
                dbg.printf("Tried to read %d but actually read %d!\n", pktLen, read);
                return;
            }
            handleMessage((char*)buffer, read, udp.remoteIP());            
        }
};

bool MessageWrapper::reply(GbMsg& msg) {
    msg.setReplyToMsgId(message->msgId());
    return srv->connectionSend(num, msg);
}  