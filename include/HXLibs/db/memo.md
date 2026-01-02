# 临时草搞

```cpp
struct User {
    int64_t id;
    std::string name;
};

struct Cat {
    int64_t id;
    int64_t userId;
    std::string name;
    Color color;
};

auto db = db::connect<Sqlite3>("ip:port or path(sqlite3)");

std::string userName{};
std::cin >> userName; // 外界传入的

std::vector<std::string> notInArr{};
for (...)
    notInArr.push_back(getData());

uint64_t offset{};
std::cin >> offset; // 外界传入的

// api (1)
auto res = db.select<col(&User::name), col(&Cat::name).as<"catName">()>()
             .from<User>()
             .join<Cat>() // leftJoin, rightJoin
             .on<col(&User::id) == col(&Cat::userId)>() // 编译期
             .where<col(&User::name) == db::param<std::string> // 此处是占位参数, 供 where() 传入
                 && col(&Cat::color) > 1 // 内部计算 param 个数, 限制 where(Ts&&...) 参数个数
                 && col(&Cat::name).notIn<"小猫", "大猫", "猫咪", db::params<std::string>>(userName, db::binds(notInArr))
             .groupBy<&Cat::color>()
             .orderBy<col(&Cat::id).asc()>()
             .limit</* offset */ db::param<uint64_t>, /* maxCnt */ 50>(offset);

for (auto const& v : res) {
    std::println("UserName: {}, CatName: {}", v.at<&User::name>(), v.at<"catName">());
    // 注意 v.at<"name">() 可能会导致歧义, 因为多个表可能重名
}
```