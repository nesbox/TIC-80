#include "yuescript/yue_parser.h"

namespace yue {
    // Override the shared() implementation to use our global instance
    YueParser& YueParser::shared() {
        extern void* yue_get_parser_instance();
        return *static_cast<YueParser*>(yue_get_parser_instance());
    }
}
