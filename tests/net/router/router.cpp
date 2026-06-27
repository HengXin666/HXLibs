#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <HXLibs/net/ApiMacro.hpp>

#include <HXLibs/net/router/RouterTree.hpp>
#include <HXLibs/net/socket/IO.hpp>

using namespace HX;

struct RouterTreeTest {
    void addPath(std::string path) {
        auto buildLink = utils::StringUtil::split<std::string_view>(
            path, "/", {getMethodStringView(net::GET)});
        _tree.insert(buildLink, [this, _path = std::move(path)] ENDPOINT {
            ++_case[_path];
            co_return;
        });
    }

    void test(std::string_view test, std::string_view byPath) {
        auto buildLink = utils::StringUtil::split<std::string_view>(
            test, "/", {getMethodStringView(net::GET)});
        auto cnt = _case[byPath];
        _tree.find(buildLink)(_req, _res).runSync();
        CHECK(cnt + 1 == _case[byPath]);
    }

private:
    coroutine::EventLoop _loop;
    net::HttpIO _io{_loop};
    net::HttpRequest<net::HttpIO> _req{_io};
    net::HttpResponse<net::HttpIO> _res{_io};
    net::RouterTree<net::HttpIO> _tree;
    std::unordered_map<std::string_view, std::size_t> _case;
};

TEST_CASE("tree: url 参数") {
    RouterTreeTest tree;
    tree.addPath("/home");
    tree.addPath("/home/{id}");
    tree.addPath("/home/{id}/link/{that}");
    tree.addPath("/home/op/{id}");
    tree.addPath("/home/**");

    tree.test("/home", "/home");
    tree.test("/home/123", "/home/{id}");
    tree.test("/home/123dasdwaasdasdwasdwas2dwasdas88dwasdwfdcvxcvxcve2s", "/home/{id}");
    tree.test("/home/123/link/qwq", "/home/{id}/link/{that}");
    tree.test("/home/op/awa123", "/home/op/{id}");
    tree.test("/home/o1p/awa123/qwq666", "/home/**");
}