#include <Arduino.h>
#include <ArduinoJson.h>
#include <GbMsg.h>
#include <WebSocketsServer.h>
#include <MessageParser.h>
#pragma once
class MessengerServer;
class MessageWrapper {
    public:
        int num;
        MessengerServer* srv;
        GbMsg* message;
        bool reply(GbMsg& msg); 
};
class _MessengerHandlerRef {
    public:
    std::function<void(MessageWrapper&)> handler;
    String filter;
    MessageWrapper wrapMessage(MessengerServer* server, GbMsg* msg, int num) {
        MessageWrapper wrap;
        wrap.message = msg;
        wrap.num = num;
        wrap.srv = server;
        return wrap;
    }
};

class MessengerServer {
    private:
        String nodeId;
        String nodeType;
        WebSocketsServer* webSocket;
        std::vector<_MessengerHandlerRef*> handlers;
        void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {   
            if (type == WStype_CONNECTED) {
                dbg.println("got a websocket connection");
                return;
            }
            if (type == WStype_DISCONNECTED) {
                dbg.println("a websocket disconnected");
                return;
            }
            if (type != WStype_TEXT) {
                dbg.println("got a non-text websocket message, ignoring it");
                return;
            }
            if (length < 2) {
                dbg.dprintln("too short ws text message, ignoring");
                return;
            }
            if (length > 500) {
                dbg.printf("got too big a websocket message, it was %d bytes\n", length);
            }
            //dbg.println("about to try parsing");
            GbMsg* msg = parseGbMsg((char*)payload, length);
            if (msg == NULL) {
                dbg.println("parsing failed, returned null");
                return;
            } else {
                //dbg.printf("parsing success, type is %s\n", msg->myType().c_str());
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
        MessengerServer(String nodeid, int port) {
            this->nodeId = nodeid;
            this->nodeType = nodeType;
            webSocket = new WebSocketsServer(port);
            webSocket->onEvent(std::bind(&MessengerServer::webSocketEvent, this, std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4));
            webSocket->begin();
        }

        void onMessage(String typeName, std::function<void(MessageWrapper&)> handler) {
            _MessengerHandlerRef* hr = new _MessengerHandlerRef();
            hr->filter = typeName;
            hr->handler = handler;
            handlers.push_back(hr);
        }

        bool broadcast(GbMsg& msg) {
            String json = msg.toJson();
            return webSocket->broadcastTXT(json);
        }

        bool connectionSend(int num, GbMsg& msg) {
            String json = msg.toJson();
            return webSocket->sendTXT(num, json);
        }

        void handle() {
            webSocket->loop();
        }

};

bool MessageWrapper::reply(GbMsg& msg) {
    return srv->connectionSend(num, msg);
}  