#include <HXLibs/log/serialize/ToString.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/db/sqlite3/MakeCreateDbSql.hpp>
#include <HXLibs/meta/FixedString.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace db;

TEST_CASE("sqlite3/MakeCreateDbSql") {
    struct Role {
        Constraint<uint64_t, attr::PrimaryKey> id;
    };
    
    struct User {
        Constraint<int,
            attr::PrimaryKey,
            attr::AutoIncrement,
            attr::NotNull
        > id;

        Constraint<std::string,
            attr::NotNull,
            attr::Unique,
            attr::DefaultVal<meta::FixedString{"loli"}>
        > name;

        Constraint<int,
            attr::DefaultVal<18>
        > age;

        Constraint<int,
            attr::Foreign<&Role::id>,
            attr::NotNull,
            attr::Index
        > roleId;
    } user{};

    auto sql = sqlite3::CreateDbSql::createDatabase(user);
    log::hxLog.info(sql);
}