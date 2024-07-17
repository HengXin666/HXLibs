#include <HXJson/HXJson.h>
#include <HXprint/HXprint.h>

namespace HXJson {

void JsonObject::print() const {
    HXprint::printnl(inner);
}

char unescaped_char(char c) {
    switch (c) {
    case 'n': return '\n';
    case 'r': return '\r';
    case '0': return '\0';
    case 't': return '\t';
    case 'v': return '\v';
    case 'f': return '\f';
    case 'b': return '\b';
    case 'a': return '\a';
    default: return c;
    }
}

std::size_t skipTail(std::string_view json, std::size_t i, char ch) {
    if (json[i] == ch)
        return 1;
    // 正向查找在原字符串中第一个与指定字符串(或字符)中的任一字符都不匹配的字符, 返回它的位置. 若查找失败, 则返回npos.
    if (std::size_t off = json.find_first_not_of(" \n\r\t\v\f\0", i); off != i && off != json.npos) {
        return off - i + (json[off] == ch);
    }
    return 0;
}

}
