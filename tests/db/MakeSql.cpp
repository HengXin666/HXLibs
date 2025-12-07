#include <HXLibs/log/serialize/ToString.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/db/sqlite3/MakeCreateDbSql.hpp>
#include <HXLibs/meta/FixedString.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace db;

TEST_CASE("sqlite3/MakeCreateDbSql") {
    using namespace meta::fixed_string_literals;

    struct Role {
        Constraint<uint64_t, attr::PrimaryKey> id;
        Constraint<int, attr::PrimaryKey> loliId;
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

            attr::UnionForeign<attr::ForeignMap<&User::id, &Role::loliId>> f_1;
        };
    } user{};

    struct UnionAttr {
        attr::Index<
            attr::IndexOrder<&User::age, attr::Order::Desc>,
            attr::IndexOrder<&User::roleId>
        > index_1;

        attr::UnionForeign<attr::ForeignMap<&Role::id, &Role::loliId>, attr::ForeignMap<&Role::id, &Role::loliId>> f_1;
    };

    [[maybe_unused]] constexpr auto N
         = reflection::membersCountVal<UnionAttr>;

    // std::is_aggregate_v<UnionAttr>; // true

    static_assert(db::attr::HasUnionAttr<User>);

    auto sql = sqlite3::CreateDbSql::createDatabase(user);
    log::hxLog.info(sql);
    // auto indexSql = sqlite3::CreateDbSql::createIndex(user);
    // log::hxLog.info(indexSql);
}