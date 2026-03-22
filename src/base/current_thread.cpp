#include "runtime/base/current_thread.h"

#include <sys/syscall.h>
#include <unistd.h>

namespace runtime::base {

thread_local int t_cached_tid = 0;

void cacheTid() {
    if (t_cached_tid == 0) {
        t_cached_tid = static_cast<int>(::syscall(SYS_gettid));
    }
}

}  // namespace runtime::base::current_thread
