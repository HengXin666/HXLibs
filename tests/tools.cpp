#include <HXLibs/log/Log.hpp>

using namespace HX;

template <std::size_t N, bool PopBack = false>
constexpr std::string makeF0ToFn(std::string const& l, std::string const& r) {
    if constexpr (N > 0) {
        auto res = [&] <std::size_t... Idx> (std::index_sequence<Idx...>) {
            return ((l + std::to_string(Idx) + r) += ...);
        }(std::make_index_sequence<N>{});
        if constexpr (PopBack) {
            for (char _ : r)
                res.pop_back();
        }
        return res;
    } else {
        return "";
    }
}

#include <HXLibs/reflection/ReflectionMacro.hpp>

int main() {
    constexpr auto N = 125U;
    [] <std::size_t... Idx> (std::index_sequence<Idx...>) {
        log::hxLog.debug((const char *)(std::to_string((N - Idx)) + ",").data()...);
    }(std::make_index_sequence<N>{});

    [] <std::size_t... Idx> (std::index_sequence<Idx...>) {
        log::hxLog.debug((const char *)("f" + std::to_string(Idx) + ",").data()...);
    }(std::make_index_sequence<N>{});
#if 0
    /*
#define __HX_PP_FOR_IMPL_1(__FUNC__, f1) __FUNC__(f1)
#define __HX_PP_FOR_IMPL_2(__FUNC__, f1, f2) __FUNC__(f1) __FUNC__(f2)
*/
    [] <std::size_t... Idx> (std::index_sequence<Idx...>) {
        log::hxLog.debug((const char *)(
            "#define __HX_PP_FOR_IMPL_"
            + std::to_string(Idx)
            + "(__FUNC__, " 
            + makeF0ToFn<Idx, true>("f", ", ")
            + ") "
            + makeF0ToFn<Idx>("__FUNC__(f", ") ")
            + "\n"
        ).data()...);
    }(std::make_index_sequence<N>{});
#endif
    return 0;
}