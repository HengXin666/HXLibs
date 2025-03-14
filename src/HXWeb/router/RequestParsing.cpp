#include <HXWeb/router/RequestParsing.h>

#include <HXSTL/utils/StringUtils.h>

namespace HX { namespace web { namespace router {

std::vector<std::size_t> RequestTemplateParsing::getPathWildcardAnalysisArr(std::string_view path) {
    auto arr = HX::STL::utils::StringUtil::split<std::string_view>(path, "/");
    std::vector<std::size_t> res;
    std::size_t n = arr.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (arr[i].front() == '{') {
            res.push_back(i);
        }
    }
    if (res.empty()) [[unlikely]] {
        throw std::runtime_error{"The path does not have wildcard characters ({?})"};
    }
    return res;
}

std::size_t RequestTemplateParsing::getUniversalWildcardPathBeginIndex(std::string_view path) {
    using namespace std::string_view_literals;
    auto arr = HX::STL::utils::StringUtil::split<std::string_view>(path, "/");
    std::size_t n = arr.size();
    // 需要保证**是在最后一项, 而不能是中间的
    if (arr.back() != "**"sv) [[unlikely]] {
        throw std::runtime_error(std::string{path} + "is not a correct wildcard statement");
    }
    for (int i = n - 2; i >= 0; --i) {
        if (arr[i] == "**"sv) {
            throw std::runtime_error(std::string{path} + "is not a correct wildcard statement");
        }
    }
    return n - 1;
}

}}} // namespace HX::web::router
