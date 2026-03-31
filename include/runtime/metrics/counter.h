
#include "runtime/metrics.h"
#include <cstddef.h>
namespace runtime::metric {

class Counter : public Metric {
public:
    void Inc(int64_t val = 1) {

    }
private:
};
}