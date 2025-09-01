#include <HXLibs/utils/TimeNTTP.hpp>

using namespace HX;

int main() {
    constexpr meta::FixedString s{"123"};
    using _ = meta::ToCharPack<s>;
    [[maybe_unused]] constexpr auto res 
        = utils::internal::constexprCharToNum(_{});

    utils::operator""_us<"123">();
    return 0;
}