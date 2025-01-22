#include <HXSTL/utils/StringUtils.h>

#include <chrono>

namespace HX { namespace STL { namespace utils {

std::vector<std::string> StringUtil::split(
    std::string_view str, 
    std::string_view delim, 
    std::vector<std::string> res /*= std::vector<std::string>{}*/
)  {
    if (str.empty()) 
        return res;

    std::size_t start = 0;
    std::size_t end = 0;
    while ((end = str.find(delim, start)) != std::string_view::npos) {
        auto tk = str.substr(start, end - start);
        if (tk.size()) {
            res.emplace_back(tk);
        }
        start = end + delim.size();
    }

    // 添加最后一个分割的部分
    res.emplace_back(str.substr(start));
    return res;
}

std::pair<std::string, std::string> StringUtil::splitAtFirst(std::string_view str, std::string_view delim) {
    std::pair<std::string, std::string> res;
    std::size_t pos = str.find(delim);
    if (pos != std::string_view::npos) {
        res.first = str.substr(0, pos);
        res.second = str.substr(pos + delim.size());
    } else {
        res.first = res.second = "";
    }
    return res;
}

std::string DateTimeFormat::format(const std::string& fmt /*= "%Y-%m-%d %H:%M:%S"*/) {
	// 获取当前时间
	auto now = std::chrono::system_clock::now();
	
	// 格式时间
	std::stringstream ss;
	auto tNow = std::chrono::system_clock::to_time_t(now);
	ss << std::put_time(std::localtime(&tNow), fmt.c_str());
	return ss.str();
}

std::string DateTimeFormat::formatWithMilli(const std::string& fmt /*= "%Y-%m-%d %H:%M:%S"*/, const std::string msDelim /*= " "*/) {
	// 获取当前时间
	auto now = std::chrono::system_clock::now();
	
	// 格式化时间
	std::stringstream ss;
	auto tNow = std::chrono::system_clock::to_time_t(now);
	ss << std::put_time(std::localtime(&tNow), fmt.c_str());

	// 获取当前时间的秒数
	auto tSeconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
	// 获取当前时间的毫秒
	auto tMilli = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
	// 作差求出毫秒数
	auto ms = tMilli - tSeconds;
	ss << msDelim << std::setfill('0') << std::setw(3) << ms.count();
	return ss.str();
}

}}} // namespace HX::STL::tools
