#include <HXLibs/log/serialize/ToString.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/db/sqlite3/MakeCreateDbSql.hpp>
#include <HXLibs/db/sql/DataBase.hpp>
#include <HXLibs/db/sql/Param.hpp>
#include <HXLibs/meta/FixedString.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace db;

struct Text {
    inline static constexpr std::string_view TableName = "TextLoLi";
};

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

    [[maybe_unused]] constexpr auto c = Col{&User::id}.as<"userId">();

    // static_assert(c._ptr == nullptr);

    constexpr auto Expr = 
        (Col(&User::id) == db::param<int> && Col(&User::id) == db::param<int>.bind<"sb">())
        || Col(&Role::id).notIn<1, db::param<int>, 3>();

    using ExprParams = internal::GetExprParamsType<Expr>;

    static_assert(sizeof(ExprParams));

    DataBase{}.select<Col{&User::id}.as<"userId">()>()
              .from<User>()
              .join<Role>()
              .on<Col(&User::roleId) == Col(&Role::loliId)>()
              .where<((Col(&User::age) == 18) 
                  && Col(&User::name).like<"loli_%">())
                  || Col(&Role::id).notIn<1, 2, 3, Col(&User::age)>()
                  || (Col(&User::id) == Col(&Role::id) + static_cast<uint64_t>(1))>()
              .groupBy<&User::name>()
              .having<Col(&User::id) == 3>()
              .orderBy<Col(&User::age).asc(), Col(&User::id).desc()>()
              .limit<10, 50>();

    int cinAge{};
    DataBase{}.select<Col(&User::id).as<"userId">(), sum<Col(&User::age)>>()
              .from<User>()
              .join<Role>()
              .on<Col(&User::roleId) == Col(&Role::loliId)>()
              .where<((Col(&User::age) == db::param<int>.bind<"?age">())
                  || Col(&User::age) == 2)>(cinAge)
              .groupBy<&User::name>()
              .orderBy<Col(&User::age).asc()>()
              .limit<db::param<int>>();

    log::hxLog.info("table:", db::getTableName<Text>());
}