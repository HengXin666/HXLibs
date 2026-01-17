#include <HXLibs/log/serialize/ToString.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/db/sqlite3/MakeCreateDbSql.hpp>
#include <HXLibs/db/sqlite3/SqliteDB.hpp>
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
        int64_t id;
        int loliId;

        struct UnionAttr {
            attr::PrimaryKey<&Role::id, &Role::loliId> pk;
        };
    };

    // attr::IsPrimaryKeyVal<attr::PrimaryKey<&Role::id, &Role::loliId>>;
    // auto _ = &Role::id == &Role::loliId;

    auto mp = reflection::makeMemberPtrToNameMap<Role>();
    CHECK(mp.at(&Role::id) == "id");
    
    struct User {
        Constraint<std::string,
            attr::NotNull,
            // attr::Unique,
            attr::DescIndex,
            attr::DefaultVal<"loli"_fs>
        > name;

        Constraint<int,
            attr::PrimaryKey<>,
            attr::AutoIncrement,
            attr::NotNull
        > id;

        Constraint<int,
            attr::DefaultVal<18>
        > age;

        Constraint<int,
            attr::Index<>
            // ,
            // attr::NotNull
        > roleId;

        struct UnionAttr {
            attr::Index<
                attr::IndexOrder<&User::age, attr::Order::Desc>,
                attr::IndexOrder<&User::roleId>
            > index_1;

            // attr::Index<> idx_2;

            // attr::UnionForeign<attr::ForeignMap<&User::id, &Role::loliId>> f_1;
        };
    } user{};

    using Key1 = GetTablePrimaryKeyIdxType<Role>;
    using Key2 = GetTablePrimaryKeyIdxType<User>;
    static_assert(!std::is_same_v<Key1, Key2>);

    log::hxLog.warning("Role", sqlite::CreateDbSql::createDatabase<Role>({}));

    auto sql = sqlite::CreateDbSql::createDatabase(user);
    log::hxLog.info(sql);
    auto indexSql = sqlite::CreateDbSql::createIndex(user);
    log::hxLog.info(indexSql);

    using db::Col;

    [[maybe_unused]] constexpr auto c = Col{&User::id}.as<"userId">();

    // static_assert(c._ptr == nullptr);

    constexpr auto Expr = 
        (Col(&User::id) == db::param<int> && Col(&User::id) == db::param<int>.bind<"sb">())
        || Col(&Role::id).notIn<1, db::param<int>, 3>();

    using ExprParams = internal::GetExprParamsType<Expr>;

    static_assert(sizeof(ExprParams));

    db::sqlite::SqliteDB db{"./test.db"};

    db.sqlTemplate<1>()
      .select<Col{&User::id}.as<"userId">()>()
      .from<User>()
      .join<Role>()
      .on<Col(&User::roleId) == Col(&Role::loliId)>()
      .where<((Col(&User::age) == 18) 
          && Col(&User::name).like<"loli_%">())
          || Col(&Role::id).notIn<1, 2, 3, Col(&User::age)>()
          || (Col(&User::id) == Col(&Role::id) + static_cast<int64_t>(1))>()
      .groupBy<&User::name>()
      .having<Col(&User::id) == 3>()
      .orderBy<Col(&User::age).asc(), Col(&User::id).desc()>()
      .limit<10, 50>();

    int cinAge{};
    db.sqlTemplate<2>()
      .select<Col(&User::id).as<"userId">(), sum<Col(&User::age)>.as<"1">()>()
      .from<User>()
      .join<Role>()
      .on<Col(&User::roleId) == Col(&Role::loliId)>()
      .where<((Col(&User::age) == db::param<int>.bind<"?age">())
          || Col(&User::age) == 2)>(cinAge)
      .groupBy<&User::name>()
      .orderBy<Col(&User::age).asc()>()
      .limit<db::param<int>>(50);

    log::hxLog.info("table:", db::getTableName<Text>());

    db::ColumnResult<meta::ValueWrap<
        Col(&User::id).as<"userId">(),
        Col(&User::name).as<"user1d">(),
        sum<Col(&User::age)>.as<"the sum">()
    >> res{1, "2", 3};
    [[maybe_unused]] auto& [_1, _2, _3] = res.gets();
    [[maybe_unused]] auto& g1 = res.get<"userId">();
    [[maybe_unused]] auto& g2 = res.get<"user1d">();
    [[maybe_unused]] auto& g3 = res.get<"the sum">();

    db.createDatabase<User>();
    db.createDatabase<Role>();

    auto insertRes1 =
        db.sqlTemplate<"插入1"_fs>()
          .insert<Role>({})
          .returning<&Role::id, &Role::loliId>()
          .exec();

    log::hxLog.info("insertRes1: data =", insertRes1.gets());
    
    auto insertRes2 = db.sqlTemplate<"插入2"_fs>()
      .insert<User>({{"张三"}, {}, {24}, {1}})
      .returning<&User::id>()
      .exec();

    log::hxLog.info("insertRes2: data =", insertRes2.gets());

    auto resArr = db.sqlTemplate<"仅注册一次, 一般使用 &本函数, 即函数指针实例化一次"_fs>()
                    .select<Col(&User::name).as<"userId">(), 
                            sum<Col(&User::age)>.as<"sum">()>()
                    .from<User>()
                    .where<((Col(&User::age) == db::param<int>.bind<"?age">())
                          || Col(&User::age) > 2)>(cinAge)
                    .exec();

    for (auto&& v : resArr) {
        log::hxLog.info("UserId:", v.get<"userId">(),
                        "AgeSum:", v.get<"sum">());
    }
}