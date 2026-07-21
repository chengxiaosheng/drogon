#include <drogon/WebSocketClient.h>
#include <drogon/HttpAppFramework.h>

#include <iostream>

using namespace drogon;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
    std::string server;
    std::string path;
    std::optional<uint16_t> port;
    // Connect to a public echo server
    if (argc > 1 && std::string(argv[1]) == "-p")
    {
        server = "ws://echo.websocket.org/.ws";
        path = "/";
    }
    else
    {
        server = "ws://127.0.0.1";
        port = 8848;
        path = "/chat";
    }
    std::string serverString;
    if (port.value_or(0) != 0)
        serverString = server + ":" + std::to_string(port.value());
    else
        serverString = server;
    auto wsPtr = WebSocketClient::newWebSocketClient(serverString);
    auto req = HttpRequest::newHttpRequest();
    req->setPath(path);
    //
    // wsPtr->setMessageHandler([](const std::string &message,
    //                             const WebSocketClientPtr &,
    //                             const WebSocketMessageType &type) {
    //     std::string messageType = "Unknown";
    //     if (type == WebSocketMessageType::Text)
    //         messageType = "text";
    //     else if (type == WebSocketMessageType::Pong)
    //         messageType = "pong";
    //     else if (type == WebSocketMessageType::Ping)
    //         messageType = "ping";
    //     else if (type == WebSocketMessageType::Binary)
    //         messageType = "binary";
    //     else if (type == WebSocketMessageType::Close)
    //         messageType = "Close";
    //
    //     InfoL << "new message (" << messageType << "): " << message;
    // });
    //
    // wsPtr->setConnectionClosedHandler([](const WebSocketClientPtr &) {
    //     InfoL << "WebSocket connection closed!";
    // });

    InfoL << "Connecting to WebSocket at " << server;
    wsPtr->connectToServer(
        req,
        [](ReqResult r,
           const HttpResponsePtr &,
           const WebSocketClientPtr &wsPtr) {
            if (r != ReqResult::Ok)
            {
                ErrorL << "Failed to establish WebSocket connection!";
                wsPtr->stop();
                return;
            }
            InfoL << "WebSocket connected!";
            wsPtr->getConnection()->setPingMessage("", 2s);
            wsPtr->getConnection()->send("hello!");
        });

    // Quit the application after 15 seconds
    app().getLoop()->doDelayTask(15 * 1000, []() { app().quit(); return 0; });

    app().setLogLevel(toolkit::LDebug);
    app().run();
    InfoL << "bye!";
    return 0;
}
