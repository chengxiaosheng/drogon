/**
 *
 *  @file HttpSession.h
 *
 *  服务端 Session 适配器：toolkit::TcpServer 收到连接后创建，
 *  内部创建并持有 trantor::TcpConnection，经 set_session 绑定自身。
 *  协议无关（HTTP/WebSocket 等由 drogon 通过回调处理）。
 *
 *  TLS 变体：toolkit::SessionWithTLSPolicy<HttpSession>
 */

#pragma once
#include <Network/Session.h>      // toolkit::Session
#include <Network/Socket.h>       // toolkit::Socket
#include <trantor/net/TcpConnection.h>
#include <trantor/net/callbacks.h>
#include <memory>
#include <functional>

namespace drogon
{
class HttpSession : public toolkit::Session
{
  public:
    explicit HttpSession(const toolkit::Socket::Ptr &sock)
        : toolkit::Session(sock)
    {
    }
    ~HttpSession() override = default;

    /**
     * @brief 创建 TcpConnection 并绑定自身（shared_from_this）。
     * 必须在 setXxxCallback/enableKickingOff 之后、由
     * toolkit::TcpServer::start 的 cb 调用一次。
     */
    void initConnection()
    {
        conn_ = std::make_shared<trantor::TcpConnection>();
        conn_->set_session(shared_from_this());
        conn_->setConnectionCallback(connectionCallback_);
        conn_->setRecvMsgCallback(messageCallback_);
        conn_->setWriteCompleteCallback(writeCompleteCallback_);
        conn_->setSSLErrorCallback(sslErrorCallback_);
        if (idleTimeout_ > 0)
            conn_->enableKickingOff(idleTimeout_);
        conn_->connectEstablished();  // 触发 connectionCallback_(connected)
    }

    // ---- 回调接线（由 drogon HttpServer 在 cb 内设置）----
    void setConnectionCallback(trantor::ConnectionCallback cb)
    {
        connectionCallback_ = std::move(cb);
    }
    void setRecvMessageCallback(trantor::RecvMessageCallback cb)
    {
        messageCallback_ = std::move(cb);
    }
    void setWriteCompleteCallback(trantor::WriteCompleteCallback cb)
    {
        writeCompleteCallback_ = std::move(cb);
    }
    void setSSLErrorCallback(trantor::SSLErrorCallback cb)
    {
        sslErrorCallback_ = std::move(cb);
    }
    void enableKickingOff(size_t timeout) { idleTimeout_ = timeout; }

    trantor::TcpConnectionPtr connection() const { return conn_; }

    // ---- toolkit::Session 转发到 TcpConnection ----
    void onRecv(const toolkit::Buffer::Ptr &buf) override
    {
        if (conn_)
            conn_->handleRecv(buf);
    }
    void onError(const toolkit::SockException &ex) override
    {
        if (conn_)
            conn_->handleClose(ex);
        // 连接关闭后释放 conn_ 引用（drogon 持有的 TcpConnectionPtr 仍存活）
        conn_.reset();
    }
    void onFlush() override
    {
        if (conn_)
            conn_->handleWriteComplete();
    }
    void onManager() override
    {
        if (conn_)
            conn_->handleManagerTick();
    }

  private:
    std::shared_ptr<trantor::TcpConnection> conn_;
    trantor::ConnectionCallback connectionCallback_;
    trantor::RecvMessageCallback messageCallback_;
    trantor::WriteCompleteCallback writeCompleteCallback_;
    trantor::SSLErrorCallback sslErrorCallback_;
    size_t idleTimeout_{0};
};

}  // namespace trantor
