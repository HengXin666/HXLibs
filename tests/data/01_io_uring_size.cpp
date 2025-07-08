#include <cstdio>

/**
 * @brief 本程序会测试出当前系统支持的最大 io_uring 环形队列的长度
 */

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
#elif defined(_MSC_VER)
    // 没事誰会msvc编译linux的它?
    #pragma warning(push)
    #pragma warning(disable : 4100 4101)
#endif

#include <liburing.h>

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
    #pragma warning(pop)
    #pragma warning(pop)
#endif

int main() {
    struct ::io_uring ring;
    for (unsigned entries = 256; entries; entries <<= 1) {
        int ret = ::io_uring_queue_init(entries, &ring, 0);
        if (ret == 0) {
            printf("Success: entries = %u\n", entries);
            ::io_uring_queue_exit(&ring);
        } else {
            perror("io_uring_queue_init");
            printf("Failed: entries = %u\n", entries);
            printf("now system max io_uring queue max size is %u\n", entries >> 1);
            break;
        }
    }
    return 0;
}
