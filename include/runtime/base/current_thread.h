#pragma once

namespace runtime::base {

    extern thread_local int t_cached_tid;

    void cacheTid();

    inline int tid() {
        if(__builtin_expect(t_cached_tid == 0, 0)) {
            cacheTid();
        }
        return t_cached_tid;
    }


} // namespace runtime::base::current_thread