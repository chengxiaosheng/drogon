#include "Groups.h"
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <iostream>
#include <Util/logger.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <thread>
#include <chrono>

using namespace std::chrono_literals;
using namespace drogon::orm;

int main()
{
    trantor::Logger::setLogLevel(trantor::Logger::kTrace);
    auto clientPtr = DbClient::newSqlite3Client("filename=test.db", 1);
    std::this_thread::sleep_for(1s);

    DebugL << "start!";
    // *clientPtr << "Drop table groups;" << Mode::Blocking >>
    //     [](const Result &r) {
    //         DebugL << "dropped";
    //     } >>
    //     [](const DrogonDbException &e) {
    //         std::cout << e.base().what() << std::endl;
    //     };
    // ;
    *clientPtr << "CREATE TABLE IF NOT EXISTS GROUPS (GROUP_ID INTEGER PRIMARY "
                  "KEY autoincrement,"
                  "GROUP_NAME TEXT,"
                  "CREATER_ID INTEGER,"
                  "CREATE_TIME TEXT,"
                  "INVITING INTEGER,"
                  "INVITING_USER_ID INTEGER,"
                  "AVATAR_ID TEXT, uuu double, text VARCHAR(255),avatar "
                  "blob,is_default bool)"
               << Mode::Blocking >>
        [](const Result &r) { DebugL << "created"; } >>
        [](const DrogonDbException &e) {
            std::cout << e.base().what() << std::endl;
        };
    *clientPtr << "insert into GROUPS (group_name) values(?)"
               << "test_group" << Mode::Blocking >>
        [](const Result &r) {
            DebugL << "inserted:" << r.affectedRows();
            DebugL << "id:" << r.insertId();
        } >>
        [](const DrogonDbException &e) {
            std::cout << e.base().what() << std::endl;
        };
    *clientPtr << "insert into GROUPS (group_name) values(?)"
               << "test_group" << Mode::Blocking >>
        [](const Result &r) {
            DebugL << "inserted:" << r.affectedRows();
            DebugL << "id:" << r.insertId();
        } >>
        [](const DrogonDbException &e) {
            std::cout << e.base().what() << std::endl;
        };
    *clientPtr << "select * from GROUPS " >> [](const Result &r) {
        DebugL << "affected rows:" << r.affectedRows();
        DebugL << "select " << r.size() << " rows";
        DebugL << "id:" << r.insertId();
        for (auto const &row : r)
        {
            DebugL << "group_id:" << row["group_id"].as<size_t>();
        }
    } >> [](const DrogonDbException &e) {
        std::cout << e.base().what() << std::endl;
    };
    {
        auto trans = clientPtr->newTransaction([](bool success) {
            DebugL << (success ? "commit success!" : "commit failed!");
        });
        Mapper<drogon_model::sqlite3::Groups> mapper(trans);
        mapper.limit(2).offset(1).findAll(
            [trans](const std::vector<drogon_model::sqlite3::Groups> &v) {
                Mapper<drogon_model::sqlite3::Groups> mapper(trans);
                for (auto group : v)
                {
                    DebugL << "group_id=" << group.getValueOfGroupId();
                    std::cout << group.toJson() << std::endl;
                    std::cout << "avatar:" << group.getValueOfAvatarAsString()
                              << std::endl;
                    group.setAvatarId("xixi");
                    mapper.update(
                        group,
                        [](const size_t count) {
                            DebugL << "update " << count << " rows";
                        },
                        [](const DrogonDbException &e) {
                            ErrorL << e.base().what();
                        });
                }
            },
            [](const DrogonDbException &e) { ErrorL << e.base().what(); });
        drogon_model::sqlite3::Groups group;
        group.setAvatar("hahahaha,xixixixix");
        try
        {
            mapper.insert(group);
        }
        catch (const DrogonDbException &e)
        {
            std::cerr << e.base().what() << std::endl;
        }
        *clientPtr << "select is_default from groups" >> [](const Result &r) {
            for (auto row : r)
            {
                std::cout << "is_default: "
                          << (row[0].isNull() ? "is null, " : "is not null, ")
                          << "bool value:" << row[0].as<bool>() << "("
                          << row[0].as<std::string>() << ")" << std::endl;
            }
        } >> [](const DrogonDbException &e) {
            std::cerr << e.base().what() << std::endl;
        };
    }
    getchar();
}
