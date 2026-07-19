/**
 *
 *  ConfigLoader.h
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

#include "Util/util.h"

#include <json/json.h>
#include <string>

namespace drogon
{
class ConfigLoader : public toolkit::noncopyable
{
  public:
    explicit ConfigLoader(const std::string &configFile) noexcept(false);
    explicit ConfigLoader(const Json::Value &data);
    explicit ConfigLoader(Json::Value &&data);
    ~ConfigLoader();

    const Json::Value &jsonValue() const
    {
        return configJsonRoot_;
    }

    void load() noexcept(false);

  private:
    std::string configFile_;
    Json::Value configJsonRoot_;
};
}  // namespace drogon
