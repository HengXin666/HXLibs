#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/db/sqlite3/SqliteDB.hpp>
#include <HXLibs/log/Log.hpp>

using namespace HX;
using namespace db;
using namespace log;

struct TestSelect {
    db::AutoIncrementPrimaryKey<int64_t> id;
    std::string name;
    int32_t loliCnt;

    inline static constexpr std::string_view TableName = "TestSelectLoLi";
};

using namespace meta::fixed_string_literals;

auto test_init = []() {
    hxLog.warning("initing...");
    auto db = db::DataBase<DbType::Sqlite3>();
    db.open("./select_test.db");
    if (auto res = db.createDatabase<TestSelect>(); !res) [[unlikely]] {
        hxLog.error(res.what());
        res.rethrow();
    }
    if (db.sqlTemplate<"test_check"_fs>()
          .select<Col(&TestSelect::id)>()
          .from<TestSelect>()
          .exec()
          .empty()
    ) {
      db.sqlTemplate<"test_init"_fs>()
        .insert<&TestSelect::name, &TestSelect::loliCnt>(std::string{"loli"}, 2233)
        .exec();
    }
    hxLog.warning("inited...");
    return 0;
}();

TEST_CASE("sqlite3/select: *id") {
    auto db = db::DataBase<DbType::Sqlite3>();
    db.open("./select_test.db");
    auto res = db.sqlTemplate<"sqlite3/select: *id"_fs>()
      .select<Col(&TestSelect::id)>()
      .from<TestSelect>()
      .exec();
    hxLog.info(res);
}

TEST_CASE("sqlite3/select: bind") {
    auto db = db::DataBase<DbType::Sqlite3>();
    db.open("./select_test.db");

    auto tp = std::make_tuple(bind<"?loli">.link(2233));
    auto&& tpRes = db::internal::atParamBindName(
        param<int32_t>.bind<"?loli">(),
        tp
    );
    CHECK(tpRes == 2233);

    auto res = db.sqlTemplate<"sqlite3/select: bind 1"_fs>()
      .select<Col(&TestSelect::id)>()
      .from<TestSelect>()
      .where<Col(&TestSelect::loliCnt) == param<int32_t>.bind<"?loli">()
          && Col(&TestSelect::name) == param<std::string>.bind<"?name">()
      >(
        bind<"?name">.link(std::string{"loli"}),
        bind<"?loli">.link(2233)
     ).exec();
    hxLog.info(res);
}