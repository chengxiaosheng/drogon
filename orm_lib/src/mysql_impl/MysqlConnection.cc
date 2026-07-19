/**
 *
 *  @file MysqlConnection.cc
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

#include "MysqlConnection.h"
#include "MysqlResultImpl.h"
#include <algorithm>
#include <exception>
#include <drogon/orm/DbTypes.h>
#include <drogon/utils/Utilities.h>
#include <string_view>
#include <errmsg.h>
#ifndef _WIN32
#include <poll.h>
#else
#define POLLIN (1U << 0)
#define POLLPRI (1U << 1)
#define POLLOUT (1U << 2)
#endif
#include <regex>

using namespace drogon;
using namespace drogon::orm;

namespace drogon
{
namespace orm
{
Result makeResult(std::shared_ptr<MYSQL_RES> &&r = nullptr,
                  Result::SizeType affectedRows = 0,
                  unsigned long long insertId = 0)
{
    return Result{std::make_shared<MysqlResultImpl>(std::move(r),
                                                    affectedRows,
                                                    insertId)};
}

}  // namespace orm
}  // namespace drogon

MysqlConnection::MysqlConnection(const std::shared_ptr<toolkit::EventPoller> &loop,
                                 const std::string &connInfo)
    : DbConnection(loop),
      mysqlPtr_(std::shared_ptr<MYSQL>(new MYSQL, [](MYSQL *p) {
          mysql_close(p);
          delete p;
      }))
{
    static MysqlEnv env;
    static thread_local MysqlThreadEnv threadEnv;
    mysql_init(mysqlPtr_.get());
    mysql_options(mysqlPtr_.get(), MYSQL_OPT_NONBLOCK, nullptr);
#ifdef HAS_MYSQL_OPTIONSV
    mysql_optionsv(mysqlPtr_.get(), MYSQL_OPT_RECONNECT, &reconnect_);
#endif
    // Get the key and value
    auto connParams = parseConnString(connInfo);
    for (auto const &kv : connParams)
    {
        auto key = kv.first;
        auto value = kv.second;

        std::transform(key.begin(),
                       key.end(),
                       key.begin(),
                       [](unsigned char c) { return tolower(c); });
        // TraceL << key << "=" << value;
        if (key == "host")
        {
            host_ = value;
        }
        else if (key == "user")
        {
            user_ = value;
        }
        else if (key == "dbname")
        {
            // DebugL << "database:[" << value << "]";
            dbname_ = value;
        }
        else if (key == "port")
        {
            port_ = value;
        }
        else if (key == "password")
        {
            passwd_ = value;
        }
        else if (key == "client_encoding")
        {
            characterSet_ = value;
        }
    }
}

void MysqlConnection::init()
{
    loop_->async([this]() {
        MYSQL *ret;
        status_ = ConnectStatus::Connecting;
        waitStatus_ =
            mysql_real_connect_start(&ret,
                                     mysqlPtr_.get(),
                                     host_.empty() ? nullptr : host_.c_str(),
                                     user_.empty() ? nullptr : user_.c_str(),
                                     passwd_.empty() ? nullptr
                                                     : passwd_.c_str(),
                                     dbname_.empty() ? nullptr
                                                     : dbname_.c_str(),
                                     port_.empty() ? 3306 : atol(port_.c_str()),
                                     nullptr,
                                     0);
        // DebugL << ret;
        auto fd = mysql_get_socket(mysqlPtr_.get());
        if (fd < 0)
        {
            ErrorL << "Connection with MySQL could not be established";
            if (closeCallback_)
            {
                auto thisPtr = shared_from_this();
                closeCallback_(thisPtr);
            }
            return;
        }
        channelPtr_ = std::make_unique<trantor::Channel>(loop_, fd);
        channelPtr_->setEventCallback([this]() { handleEvent(); });
        setChannel();
    });
}

void MysqlConnection::setChannel()
{
    if ((waitStatus_ & MYSQL_WAIT_READ) || (waitStatus_ & MYSQL_WAIT_EXCEPT))
    {
        if (!channelPtr_->isReading())
            channelPtr_->enableReading();
    }
    if (waitStatus_ & MYSQL_WAIT_WRITE)
    {
        if (!channelPtr_->isWriting())
            channelPtr_->enableWriting();
    }
    else
    {
        if (channelPtr_->isWriting())
            channelPtr_->disableWriting();
    }
    if (waitStatus_ & MYSQL_WAIT_TIMEOUT)
    {
        auto timeout = mysql_get_timeout_value_ms(mysqlPtr_.get());
        auto thisPtr = shared_from_this();
        loop_->doDelayTask(timeout, [thisPtr]() { thisPtr->handleTimeout(); return 0; });
    }
}

void MysqlConnection::handleClosed()
{
    // loop_->assertInLoopThread();
    if (status_ == ConnectStatus::Bad)
        return;
    status_ = ConnectStatus::Bad;
    channelPtr_->disableAll();
    channelPtr_->remove();
    assert(closeCallback_);
    auto thisPtr = shared_from_this();
    closeCallback_(thisPtr);
}

void MysqlConnection::disconnect()
{
    auto thisPtr = shared_from_this();
    std::promise<int> pro;
    auto f = pro.get_future();
    loop_->async([thisPtr, &pro]() {
        thisPtr->status_ = ConnectStatus::Bad;
        thisPtr->channelPtr_->disableAll();
        thisPtr->channelPtr_->remove();
        thisPtr->mysqlPtr_.reset();
        pro.set_value(1);
    });
    f.get();
}

void MysqlConnection::handleTimeout()
{
    int status = 0;
    status |= MYSQL_WAIT_TIMEOUT;
    MYSQL *ret;
    if (status_ == ConnectStatus::Connecting)
    {
        waitStatus_ = mysql_real_connect_cont(&ret, mysqlPtr_.get(), status);
        if (waitStatus_ == 0)
        {
            auto errorNo = mysql_errno(mysqlPtr_.get());
            if (!ret && errorNo)
            {
                ErrorL << "Error(" << errorNo << ") \""
                          << mysql_error(mysqlPtr_.get()) << "\"";
                ErrorL << "Failed to mysql_real_connect()";
                handleClosed();
                return;
            }
            // I don't think the programe can run to here.
            if (characterSet_.empty())
            {
                status_ = ConnectStatus::Ok;
                if (okCallback_)
                {
                    auto thisPtr = shared_from_this();
                    okCallback_(thisPtr);
                }
            }
            else
            {
                startSetCharacterSet();
                return;
            }
        }
        setChannel();
    }
    else if (status_ == ConnectStatus::SettingCharacterSet)
    {
        continueSetCharacterSet(status);
    }
    else if (status_ == ConnectStatus::Ok)
    {
    }
}

void MysqlConnection::handleCmd(int status)
{
    switch (execStatus_)
    {
        case ExecStatus::RealQuery:
        {
            int err = 0;
            waitStatus_ = mysql_real_query_cont(&err, mysqlPtr_.get(), status);
            TraceL << "real_query:" << waitStatus_;
            if (waitStatus_ == 0)
            {
                if (err)
                {
                    execStatus_ = ExecStatus::None;
                    ErrorL << "error:" << err << " status:" << status;
                    outputError();
                    return;
                }
                startStoreResult(false);
            }
            setChannel();
            break;
        }
        case ExecStatus::StoreResult:
        {
            MYSQL_RES *ret;
            waitStatus_ =
                mysql_store_result_cont(&ret, mysqlPtr_.get(), status);
            TraceL << "store_result:" << waitStatus_;
            if (waitStatus_ == 0)
            {
                if (!ret && mysql_errno(mysqlPtr_.get()))
                {
                    execStatus_ = ExecStatus::None;
                    ErrorL << "error";
                    outputError();
                    return;
                }
                getResult(ret);
            }
            setChannel();
            break;
        }
        case ExecStatus::NextResult:
        {
            int err;
            waitStatus_ = mysql_next_result_cont(&err, mysqlPtr_.get(), status);
            if (waitStatus_ == 0)
            {
                if (err)
                {
                    execStatus_ = ExecStatus::None;
                    ErrorL << "error:" << err << " status:" << status;
                    outputError();
                    return;
                }
                startStoreResult(false);
            }
            setChannel();
            break;
        }
        case ExecStatus::None:
        {
            // Connection closed!
            if (waitStatus_ == 0)
                handleClosed();
            break;
        }
        default:
            return;
    }
}

void MysqlConnection::handleEvent()
{
    int status = 0;
    auto revents = channelPtr_->revents();
    if (revents & POLLIN)
        status |= MYSQL_WAIT_READ;
    if (revents & POLLOUT)
        status |= MYSQL_WAIT_WRITE;
    if (revents & POLLPRI)
        status |= MYSQL_WAIT_EXCEPT;
    status = (status & waitStatus_);
    MYSQL *ret;
    if (status_ == ConnectStatus::Connecting)
    {
        waitStatus_ = mysql_real_connect_cont(&ret, mysqlPtr_.get(), status);
        if (waitStatus_ == 0)
        {
            auto errorNo = mysql_errno(mysqlPtr_.get());
            if (!ret && errorNo)
            {
                ErrorL << "Error(" << errorNo << ") \""
                          << mysql_error(mysqlPtr_.get()) << "\"";
                ErrorL << "Failed to mysql_real_connect()";
                handleClosed();
                return;
            }
            if (characterSet_.empty())
            {
                status_ = ConnectStatus::Ok;
                if (okCallback_)
                {
                    auto thisPtr = shared_from_this();
                    okCallback_(thisPtr);
                }
            }
            else
            {
                startSetCharacterSet();
                return;
            }
        }
        setChannel();
    }
    else if (status_ == ConnectStatus::Ok)
    {
        handleCmd(status);
    }
    else if (status_ == ConnectStatus::SettingCharacterSet)
    {
        continueSetCharacterSet(status);
    }
}

void MysqlConnection::continueSetCharacterSet(int status)
{
    int err;
    waitStatus_ = mysql_set_character_set_cont(&err, mysqlPtr_.get(), status);
    if (waitStatus_ == 0)
    {
        if (err)
        {
            ErrorL << "Error(" << err << ") \""
                      << mysql_error(mysqlPtr_.get()) << "\"";
            ErrorL << "Failed to mysql_set_character_set_cont()";
            handleClosed();
            return;
        }
        status_ = ConnectStatus::Ok;
        if (okCallback_)
        {
            auto thisPtr = shared_from_this();
            okCallback_(thisPtr);
        }
    }
    setChannel();
}

void MysqlConnection::startSetCharacterSet()
{
    int err;
    waitStatus_ = mysql_set_character_set_start(&err,
                                                mysqlPtr_.get(),
                                                characterSet_.data());
    if (waitStatus_ == 0)
    {
        if (err)
        {
            ErrorL << "Error(" << err << ") \""
                      << mysql_error(mysqlPtr_.get()) << "\"";
            ErrorL << "Failed to mysql_set_character_set_start()";
            handleClosed();
            return;
        }
        status_ = ConnectStatus::Ok;
        if (okCallback_)
        {
            auto thisPtr = shared_from_this();
            okCallback_(thisPtr);
        }
    }
    else
    {
        status_ = ConnectStatus::SettingCharacterSet;
    }
    setChannel();
}

void MysqlConnection::execSqlInLoop(
    std::string_view &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    TraceL << sql;
    assert(paraNum == parameters.size());
    assert(paraNum == length.size());
    assert(paraNum == format.size());
    assert(rcb);
    assert(!isWorking_);
    assert(!sql.empty());
    if (status_ != ConnectStatus::Ok)
    {
        ErrorL << "Connection is not ready";
        auto exceptPtr =
            std::make_exception_ptr(drogon::orm::BrokenConnection());
        exceptCallback(exceptPtr);
        return;
    }
    callback_ = std::move(rcb);
    isWorking_ = true;
    exceptionCallback_ = std::move(exceptCallback);
    sql_.clear();
    if (paraNum > 0)
    {
        std::string::size_type pos = 0;
        std::string::size_type seekPos = std::string::npos;
        for (size_t i = 0; i < paraNum; ++i)
        {
            seekPos = sql.find('?', pos);
            if (seekPos == std::string::npos)
            {
                auto sub = sql.substr(pos);
                sql_.append(sub.data(), sub.length());
                pos = seekPos;
                break;
            }
            else
            {
                auto sub = sql.substr(pos, seekPos - pos);
                sql_.append(sub.data(), sub.length());
                pos = seekPos + 1;
                switch (format[i])
                {
                    case internal::MySqlTiny:
                        sql_.append(std::to_string(*((char *)parameters[i])));
                        break;
                    case internal::MySqlShort:
                        sql_.append(std::to_string(*((short *)parameters[i])));
                        break;
                    case internal::MySqlLong:
                        sql_.append(
                            std::to_string(*((int32_t *)parameters[i])));
                        break;
                    case internal::MySqlLongLong:
                        sql_.append(
                            std::to_string(*((int64_t *)parameters[i])));
                        break;
                    case internal::MySqlUTiny:
                        sql_.append(
                            std::to_string(*((unsigned char *)parameters[i])));
                        break;
                    case internal::MySqlUShort:
                        sql_.append(
                            std::to_string(*((unsigned short *)parameters[i])));
                        break;
                    case internal::MySqlULong:
                        sql_.append(
                            std::to_string(*((uint32_t *)parameters[i])));
                        break;
                    case internal::MySqlULongLong:
                        sql_.append(
                            std::to_string(*((uint64_t *)parameters[i])));
                        break;
                    case internal::MySqlNull:
                        sql_.append("NULL");
                        break;
                    case internal::MySqlString:
                    {
                        sql_.append("'");
                        std::string to(length[i] * 2, '\0');
                        auto len = mysql_real_escape_string(mysqlPtr_.get(),
                                                            (char *)to.c_str(),
                                                            parameters[i],
                                                            length[i]);
                        to.resize(len);
                        sql_.append(to);
                        sql_.append("'");
                    }
                    break;
                    case internal::DrogonDefaultValue:
                        sql_.append("default");
                        break;
                    default:
                        ErrorL
                            << "MySQL does not recognize the parameter type";
                        abort();
                        break;
                }
            }
        }
        if (pos < sql.length())
        {
            auto sub = sql.substr(pos);
            sql_.append(sub.data(), sub.length());
        }
    }
    else
    {
        sql_ = std::string(sql.data(), sql.length());
    }
    startQuery();
    setChannel();
}

void MysqlConnection::outputError()
{
    channelPtr_->disableAll();
    auto errorNo = mysql_errno(mysqlPtr_.get());
    ErrorL << "Error(" << errorNo << ") [" << mysql_sqlstate(mysqlPtr_.get())
              << "] \"" << mysql_error(mysqlPtr_.get()) << "\"";
    ErrorL << "sql:" << sql_;
    if (isWorking_)
    {
        // TODO: exception type
        auto exceptPtr = std::make_exception_ptr(
            SqlError(mysql_error(mysqlPtr_.get()), sql_, errorNo, 0));
        exceptionCallback_(exceptPtr);
        exceptionCallback_ = nullptr;

        callback_ = nullptr;
        isWorking_ = false;
        if (errorNo != CR_SERVER_GONE_ERROR && errorNo != CR_SERVER_LOST)
        {
            idleCb_();
        }
    }
    if (errorNo == CR_SERVER_GONE_ERROR || errorNo == CR_SERVER_LOST)
    {
        handleClosed();
    }
}

void MysqlConnection::startQuery()
{
    int err;
    // int mysql_real_query_start(int *ret, MYSQL *mysql, const char *q,
    // unsigned long length)
    waitStatus_ = mysql_real_query_start(&err,
                                         mysqlPtr_.get(),
                                         sql_.c_str(),
                                         sql_.length());
    TraceL << "real_query:" << waitStatus_;
    execStatus_ = ExecStatus::RealQuery;
    if (waitStatus_ == 0)
    {
        if (err)
        {
            ErrorL << "error";
            loop_->async(
                [thisPtr = shared_from_this()] { thisPtr->outputError(); });
            return;
        }
        startStoreResult(true);
    }
}

void MysqlConnection::startStoreResult(bool queueInLoop)
{
    MYSQL_RES *ret;
    execStatus_ = ExecStatus::StoreResult;
    waitStatus_ = mysql_store_result_start(&ret, mysqlPtr_.get());
    TraceL << "store_result:" << waitStatus_;
    if (waitStatus_ == 0)
    {
        execStatus_ = ExecStatus::None;
        if (!ret && mysql_errno(mysqlPtr_.get()))
        {
            if (queueInLoop)
            {
                loop_->async(
                    [thisPtr = shared_from_this()] { thisPtr->outputError(); }, false);
            }
            else
            {
                outputError();
            }
            return;
        }
        if (queueInLoop)
        {
            loop_->async([thisPtr = shared_from_this(), ret] {
                thisPtr->getResult(ret);
            }, false);
        }
        else
        {
            getResult(ret);
        }
    }
}

void MysqlConnection::getResult(MYSQL_RES *res)
{
    auto resultPtr = std::shared_ptr<MYSQL_RES>(res, [](MYSQL_RES *r) {
        mysql_free_result(r);
    });
    auto Result = makeResult(std::move(resultPtr),
                             mysql_affected_rows(mysqlPtr_.get()),
                             mysql_insert_id(mysqlPtr_.get()));
    if (isWorking_)
    {
        callback_(Result);
        if (!mysql_more_results(mysqlPtr_.get()))
        {
            callback_ = nullptr;
            exceptionCallback_ = nullptr;
            isWorking_ = false;
            idleCb_();
        }
        else
        {
            execStatus_ = ExecStatus::NextResult;
            int err;
            waitStatus_ = mysql_next_result_start(&err, mysqlPtr_.get());
            if (waitStatus_ == 0)
            {
                if (err)
                {
                    execStatus_ = ExecStatus::None;
                    ErrorL << "error:" << err;
                    outputError();
                    return;
                }
                startStoreResult(false);
            }
        }
    }
}
