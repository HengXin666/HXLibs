#include <HXJson/Json.h>
#include <HXprint/print.h>

#if 0
#include <variant>
#include <iostream>
#include <memory>

// 类型擦除接口
class IValue {
public:
    virtual ~IValue() = default;
    virtual void print() const = 0; // 用于展示值
};

// 类型擦除实现
template <typename T>
class ValueImpl : public IValue {
public:
    ValueImpl(T value) : value_(std::move(value)) {}
    void print() const override {
        std::cout << value_ << std::endl;
    }
private:
    T value_;
};

// 获取 std::variant 的当前值
std::unique_ptr<IValue> getVal(const std::variant<int, float, std::string>& v) {
    return std::visit([](const auto& arg) -> std::unique_ptr<IValue> {
        return std::make_unique<ValueImpl<decltype(arg)>>(arg);
    }, v);
}

int main() { // 使用类型擦除, 从而输出 variant 的实时的值
    std::variant<int, float, std::string> v = std::string("Hello World");
    
    auto valuePtr = getVal(v);
    valuePtr->print(); // 输出: Hello World

    return 0;
}
#endif


#ifdef JSON_TEST_MAIN

int main () {
    auto json = HX::Json::parse(R"(
[
  {
    "name": "Molecule Man",
    "age": 29,
    "secretIdentity": "Dan Jukes",
    "powers": ["Radiation resistance", "Turning tiny", "Radiation blast"]
  },
  {
    "name": "Madame Uppercut",
    "age": 39.0000009,
    "secretIdentity": "Jane Wilson",
    "powers": [
      "Million tonne punch",
      "Damage resistance",
      "Superhuman reflexes"
    ],
    sb: null
  }
]
)").first;

    json.print();
    std::cout << '\n';
    
    json.get<HX::Json::JsonList>()[1].get<HX::Json::JsonDict>()["sb"].print();
    std::cout << '\n';

    json[1]["sb"].print();
    std::cout << '\n';

    json[0]["powers"][0].print();
    std::cout << '\n';

    auto str = json.toString();
    std::cout << str << '\n';

    return 0;
}

#endif // JSON_TEST_MAIN