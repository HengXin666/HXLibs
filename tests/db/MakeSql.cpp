#include <HXLibs/log/serialize/ToString.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/db/sqlite3/MakeCreateDbSql.hpp>
#include <HXLibs/db/sql/DataBase.hpp>
#include <HXLibs/meta/FixedString.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace db;

TEST_CASE("sqlite3/MakeCreateDbSql") {
    using namespace meta::fixed_string_literals;

    struct Role {
        uint64_t id;
        int loliId;

        struct UnionAttr {
            attr::PrimaryKey<&Role::id, &Role::loliId> pk;
        };
    };
    
    struct User {
        Constraint<int,
            attr::PrimaryKey<>,
            attr::AutoIncrement,
            attr::NotNull
        > id;

        Constraint<std::string,
            attr::NotNull,
            attr::Unique,
            attr::DescIndex,
            attr::DefaultVal<"loli"_fs>
        > name;

        Constraint<int,
            attr::Foreign<&Role::loliId>,
            attr::DefaultVal<18>
        > age;

        Constraint<int,
            attr::Index<>,
            attr::NotNull
        > roleId;

        struct UnionAttr {
            attr::Index<
                attr::IndexOrder<&User::age, attr::Order::Desc>,
                attr::IndexOrder<&User::roleId>
            > index_1;

            // attr::Index<> idx_2;

            attr::UnionForeign<attr::ForeignMap<&User::id, &Role::loliId>> f_1;
        };
    } user{};

    log::hxLog.warning("Role", sqlite3::CreateDbSql::createDatabase<Role>({}));

    auto sql = sqlite3::CreateDbSql::createDatabase(user);
    log::hxLog.info(sql);
    auto indexSql = sqlite3::CreateDbSql::createIndex(user);
    log::hxLog.info(indexSql);

    using db::Col;

    [[maybe_unused]] auto c = Col{&User::id}.as<"userId">();

    DateBase{}.select<Col{&User::id}>()
              .from<User>()
              .join<Role>()
              .on<Col(&User::roleId) == Col(&Role::id)>()
              .where<(Col(&User::age) == meta::NumberNTTP<18>{}) 
                  && Col(&User::name).like<"loli_%">()>()
              .groupBy<&User::name>()
              .orderBy<Col(&User::age).asc()>()
              .limit<10, 50>();
}