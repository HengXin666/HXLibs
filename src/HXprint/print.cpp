#include <HXprint/print.h>

namespace HX { namespace print {

#ifdef _HX_DEBUG_
void internal::logMessage(LogLevel level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    switch (level) {
        case LOG_ERROR:
            printf("\033[0m\033[1;31m[ERROR]: ");
            break;
        case LOG_WARNING:
            printf("\033[0m\033[1;33m[WARNING]: ");
            break;
        case LOG_INFO:
            printf("\033[0m\033[1;32m[INFO]: ");
            break;
    }
    vprintf(format, args);
    printf("\033[0m\n");
    va_end(args);
}
#endif // _HX_DEBUG_

}} // namespace HX::print