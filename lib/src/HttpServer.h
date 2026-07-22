/**
 *
 *  @file HttpServer.h
 *  @author An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <Network/TcpServer.h>       // toolkit::TcpServer
#include <Network/Session.h>         // toolkit::SessionWithTLSPolicy
#include "HttpSession.h"
#include <trantor/net/InetAddress.h>
#include <trantor/net/TLSPolicy.h>
#include <Util/SSLBox.h>             // toolkit::TLSSessionFactory
#include <Util/util.h>
#include <functional>
#include <string>
#include <vector>
#include "impl_forwards.h"

struct CallbackParamPack;

namespace drogon
{
struct ControllerBinderBase;

class HttpServer : toolkit::noncopyable
{
  public:
    HttpServer(const trantor::InetAddress &listenAddr, std::string name);

    ~HttpServer();

    void start();
    void stop();

    void enableSSL(trantor::TLSPolicyPtr policy)
    {
        tlsPolicyPtr_ = std::move(policy);
    }

    void reloadSSL();

    const trantor::InetAddress &address() const
    {
        return listenAddr_;
    }

    std::string ipPort() const
    {
        return listenAddr_.toIpPort();
    }

    const std::string &name() const
    {
        return name_;
    }

    void setBeforeListenSockOptCallback(std::function<void(int)> cb)
    {
        beforeListenSetSockOptCallback_ = std::move(cb);
    }

    void setAfterAcceptSockOptCallback(std::function<void(int)> cb)
    {
        afterAcceptSetSockOptCallback_ = std::move(cb);
    }

    void setConnectionCallback(
        std::function<void(const trantor::TcpConnectionPtr &)> cb)
    {
        connectionCallback_ = std::move(cb);
    }

    void kickoffIdleConnections(size_t timeout)
    {
        idleConnectionTimeout_ = timeout;
    }

  private:
    friend class HttpInternalForwardHelper;

    static void onConnection(const trantor::TcpConnectionPtr &conn);
    static void onMessage(const trantor::TcpConnectionPtr &,
                          trantor::ParseCursor *);
    static void onRequests(const trantor::TcpConnectionPtr &,
                           const std::vector<HttpRequestImplPtr> &,
                           const std::shared_ptr<HttpRequestParser> &);

    struct HttpRequestParamPack
    {
        std::shared_ptr<ControllerBinderBase> binderPtr;
        std::function<void(const HttpResponsePtr &)> callback;
    };

    struct WsRequestParamPack
    {
        std::shared_ptr<ControllerBinderBase> binderPtr;
        std::function<void(const HttpResponsePtr &)> callback;
        WebSocketConnectionImplPtr wsConnPtr;
    };

    // Http request handling steps
    static void onHttpRequest(const HttpRequestImplPtr &,
                              std::function<void(const HttpResponsePtr &)> &&);
    static void httpRequestRouting(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback);
    static void httpRequestHandling(
        const HttpRequestImplPtr &req,
        std::shared_ptr<ControllerBinderBase> &&binderPtr,
        std::function<void(const HttpResponsePtr &)> &&callback);

    // Websocket request handling steps
    static void onWebsocketRequest(
        const HttpRequestImplPtr &,
        std::function<void(const HttpResponsePtr &)> &&,
        WebSocketConnectionImplPtr &&);
    static void websocketRequestRouting(
        const HttpRequestImplPtr &req,
        std::function<void(const HttpResponsePtr &)> &&callback,
        WebSocketConnectionImplPtr &&wsConnPtr);
    static void websocketRequestHandling(
        const HttpRequestImplPtr &req,
        std::shared_ptr<ControllerBinderBase> &&binderPtr,
        std::function<void(const HttpResponsePtr &)> &&callback,
        WebSocketConnectionImplPtr &&wsConnPtr);

    // Http/Websocket shared handling steps
    template <typename Pack>
    static void requestPostRouting(const HttpRequestImplPtr &req, Pack &&pack);
    template <typename Pack>
    static void requestPassMiddlewares(const HttpRequestImplPtr &req,
                                       Pack &&pack);
    template <typename Pack>
    static void requestPreHandling(const HttpRequestImplPtr &req, Pack &&pack);

    // Response buffering and sending
    static void handleResponse(
        const HttpResponsePtr &response,
        const std::shared_ptr<CallbackParamPack> &paramPack,
        bool *respReadyPtr);
    static void sendResponse(const trantor::TcpConnectionPtr &,
                             const HttpResponsePtr &,
                             bool isHeadMethod);
    static void sendResponses(
        const trantor::TcpConnectionPtr &conn,
        const std::vector<std::pair<HttpResponsePtr, bool>> &responses,
        trantor::MsgBuffer &buffer);

    trantor::InetAddress listenAddr_;
    std::string name_;
    std::shared_ptr<toolkit::TcpServer> server_;  // nullptr poller -> 多 poller 抢占式 accept
    trantor::TLSPolicyPtr tlsPolicyPtr_;
    size_t idleConnectionTimeout_{0};

    // 按 useSSL 选择 Session 类型启动 toolkit::TcpServer
    template <typename SessionT>
    void startWith();

    std::function<void(int)> beforeListenSetSockOptCallback_;
    std::function<void(int)> afterAcceptSetSockOptCallback_;
    std::function<void(const trantor::TcpConnectionPtr &)> connectionCallback_;
};

class HttpInternalForwardHelper
{
  public:
    static void forward(const HttpRequestImplPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback)
    {
        return HttpServer::onHttpRequest(req, std::move(callback));
    }
};

}  // namespace drogon
