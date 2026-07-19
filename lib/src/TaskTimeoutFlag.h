/**
 *
 *  @file TaskTimeoutFlag.h
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

#include <Util/util.h>
#include <Poller/EventPoller.h>
#include <chrono>
#include <functional>
#include <atomic>
#include <memory>

namespace drogon
{
class TaskTimeoutFlag : public toolkit::noncopyable,
                        public std::enable_shared_from_this<TaskTimeoutFlag>
{
  public:
    TaskTimeoutFlag(const std::shared_ptr<toolkit::EventPoller> &loop,
                    const std::chrono::duration<double> &timeout,
                    std::function<void()> timeoutCallback);
    bool done();
    void runTimer();

  private:
    std::atomic<bool> isDone_{false};
    std::shared_ptr<toolkit::EventPoller> loop_;
    std::chrono::duration<double> timeout_;
    std::function<void()> timeoutFunc_;
};
}  // namespace drogon
