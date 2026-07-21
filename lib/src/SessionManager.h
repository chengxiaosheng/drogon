/**
 *
 *  @file SessionManager.h
 *  An Tao
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

#include <drogon/Session.h>
#include <drogon/drogon_callbacks.h>
#include <drogon/CacheMap.h>
#include <Util/util.h>
#include <Poller/EventPoller.h>
#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <vector>

namespace drogon
{
class SessionManager : public toolkit::noncopyable
{
  public:
    using IdGeneratorCallback = std::function<std::string()>;

    SessionManager(
        const std::shared_ptr<toolkit::EventPoller> &loop,
        size_t timeout,
        const std::vector<AdviceStartSessionCallback> &startAdvices,
        const std::vector<AdviceDestroySessionCallback> &destroyAdvices,
        IdGeneratorCallback idGeneratorCallback);

    ~SessionManager()
    {
        sessionMapPtr_.reset();
    }

    SessionPtr getSession(const std::string &sessionID, bool needToSet);
    void changeSessionId(const SessionPtr &sessionPtr);

  private:
    std::unique_ptr<CacheMap<std::string, SessionPtr>> sessionMapPtr_;
    std::weak_ptr<toolkit::EventPoller> loop_;
    size_t timeout_;
    const std::vector<AdviceStartSessionCallback> &sessionStartAdvices_;
    const std::vector<AdviceDestroySessionCallback> &sessionDestroyAdvices_;
    IdGeneratorCallback idGeneratorCallback_;
};
}  // namespace drogon
