/**
 *
 *  RedisClientSkipped.cc
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

#include "drogon/nosql/RedisResult.h"
#include <Util/logger.h>

namespace drogon
{
namespace nosql
{
std::string RedisResult::getStringForDisplaying() const noexcept
{
    ErrorL << "Redis is not supported by drogon, please install the "
                 "hiredis library first.";
    abort();
}

std::string RedisResult::getStringForDisplayingWithIndent(
    size_t /*indent*/) const noexcept
{
    ErrorL << "Redis is not supported by drogon, please install the "
                 "hiredis library first.";
    abort();
}

std::string RedisResult::asString() const noexcept(false)
{
    ErrorL << "Redis is not supported by drogon, please install the "
                 "hiredis library first.";
    abort();
}

RedisResultType RedisResult::type() const noexcept
{
    ErrorL << "Redis is not supported by drogon, please install the "
                 "hiredis library first.";
    abort();
}

std::vector<RedisResult> RedisResult::asArray() const noexcept(false)
{
    ErrorL << "Redis is not supported by drogon, please install the "
                 "hiredis library first.";
    abort();
}

long long RedisResult::asInteger() const noexcept(false)
{
    ErrorL << "Redis is not supported by drogon, please install the "
                 "hiredis library first.";
    abort();
}

bool RedisResult::isNil() const noexcept
{
    ErrorL << "Redis is not supported by drogon, please install the "
                 "hiredis library first.";
    abort();
}
}  // namespace nosql
}  // namespace drogon
