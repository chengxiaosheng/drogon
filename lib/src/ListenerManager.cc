/**
 *
 *  @file ListenerManager.cc
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

#include "ListenerManager.h"
#include <drogon/config.h>
#include <fcntl.h>
#include <Util/logger.h>
#include <Poller/EventPoller.h>
#include "HttpAppFrameworkImpl.h"
#include "HttpServer.h"
#ifndef _WIN32
#include <sys/file.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

namespace drogon
{
#ifndef _WIN32
class DrogonFileLocker : public toolkit::noncopyable
{
  public:
    DrogonFileLocker()
    {
        fd_ = open("/tmp/drogon.lock", O_TRUNC | O_CREAT, 0666);
        flock(fd_, LOCK_EX);
    }

    ~DrogonFileLocker()
    {
        close(fd_);
    }

  private:
    int fd_{0};
};

#endif
}  // namespace drogon

using namespace trantor;
using namespace drogon;

void ListenerManager::addListener(
    const std::string &ip,
    uint16_t port,
    bool useSSL,
    const std::string &certFile,
    const std::string &keyFile,
    bool useOldTLS,
    const std::vector<std::pair<std::string, std::string>> &sslConfCmds)
{
    if (useSSL && !utils::supportsTls())
        ErrorL << "Can't use SSL without OpenSSL found in your system";
    listeners_.emplace_back(
        ip, port, useSSL, certFile, keyFile, useOldTLS, sslConfCmds);
}

std::vector<trantor::InetAddress> ListenerManager::getListeners() const
{
    std::vector<trantor::InetAddress> listeners;
    for (auto &server : servers_)
    {
        listeners.emplace_back(server->address());
    }
    return listeners;
}

void ListenerManager::createListeners(
    const std::string &globalCertFile,
    const std::string &globalKeyFile,
    const std::vector<std::pair<std::string, std::string>> &sslConfCmds)
{
    TraceL << "poller thread num="
           << toolkit::EventPollerPool::Instance().getExecutorSize();
    for (auto const &listener : listeners_)
    {
        auto const &ip = listener.ip_;
        bool isIpv6 = (ip.find(':') != std::string::npos);
        InetAddress listenAddress(ip, listener.port_, isIpv6);
        if (listenAddress.isUnspecified())
        {
            ErrorL << "Failed to parse IP address '" << ip
                      << "'. (Note: FQDN/domain names/hostnames are not "
                         "supported. Including 'localhost')";
            abort();
        }
// #ifndef _WIN32
//         if (!app().reusePort())
//         {
//             DrogonFileLocker lock;
//             // 跨实例协调（端口占用由 toolkit::TcpServer::start 检测并报错）
//         }
// #endif
        auto serverPtr = std::make_shared<HttpServer>(listenAddress, "drogon");
        if (beforeListenSetSockOptCallback_)
        {
            serverPtr->setBeforeListenSockOptCallback(
                beforeListenSetSockOptCallback_);
        }
        if (afterAcceptSetSockOptCallback_)
        {
            serverPtr->setAfterAcceptSockOptCallback(
                afterAcceptSetSockOptCallback_);
        }
        if (connectionCallback_)
        {
            serverPtr->setConnectionCallback(connectionCallback_);
        }

        if (listener.useSSL_ && utils::supportsTls())
        {
            auto cert = listener.certFile_;
            auto key = listener.keyFile_;
            if (cert.empty())
                cert = globalCertFile;
            if (key.empty())
                key = globalKeyFile;
            if (cert.empty() || key.empty())
            {
                std::cerr
                    << "You can't use https without cert file or key file"
                    << std::endl;
                exit(1);
            }
            auto cmds = sslConfCmds;
            std::copy(listener.sslConfCmds_.begin(),
                      listener.sslConfCmds_.end(),
                      std::back_inserter(cmds));
            auto policy =
                trantor::TLSPolicy::defaultServerPolicy(cert, key);
            policy->setConfCmds(cmds).setUseOldTLS(listener.useOldTLS_);
            serverPtr->enableSSL(std::move(policy));
        }
        servers_.push_back(serverPtr);
    }
}

void ListenerManager::startListening()
{
    for (auto &server : servers_)
    {
        server->start();
    }
}

void ListenerManager::stopListening()
{
    for (auto &serverPtr : servers_)
    {
        serverPtr->stop();
    }
}

void ListenerManager::reloadSSLFiles()
{
    for (auto &server : servers_)
    {
        server->reloadSSL();
    }
}
