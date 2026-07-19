/**
 *
 *  @file TaskTimeoutFlag.cc
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

#include "TaskTimeoutFlag.h"
using namespace drogon;

TaskTimeoutFlag::TaskTimeoutFlag(const std::shared_ptr<toolkit::EventPoller> &loop,
                                 const std::chrono::duration<double> &timeout,
                                 std::function<void()> timeoutCallback)
    : loop_(loop), timeout_(timeout), timeoutFunc_(timeoutCallback)
{
}

void TaskTimeoutFlag::runTimer()
{
    std::weak_ptr<TaskTimeoutFlag> weakPtr = shared_from_this();
    loop_->doDelayTask(timeout_.count() * 1000, [weakPtr]() {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return 0;
        if (thisPtr->done())
            return 0;
        thisPtr->timeoutFunc_();
        return 0;
    });
}

bool TaskTimeoutFlag::done()
{
    return isDone_.exchange(true);
}
