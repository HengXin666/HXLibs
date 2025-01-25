#include <HXWeb/router/RequestParsing.h>

#include <HXSTL/utils/StringUtils.h>

namespace HX { namespace web { namespace router {

std::vector<int> RequestTemplateParsing::getPathWildcardAnalysisArr(std::string_view path) {
    auto arr = HX::STL::utils::StringUtil::split(path, "/", std::vector<std::string_view>{});
    std::vector<int> res;
    std::size_t n = arr.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (arr[i][0] == '{') {
            res.push_back(i);
        }
    }
    if (res.empty())
        throw "The path does not have wildcard characters ({?})";
    return res;
}

std::size_t RequestTemplateParsing::getUniversalWildcardPathBeginIndex(std::string_view path) {
    std::size_t pos = path.find("**");
    if (pos == std::string_view::npos)
        throw "The path does not have wildcard characters (**)";
    return pos;
}

}}} // namespace HX::web::router
