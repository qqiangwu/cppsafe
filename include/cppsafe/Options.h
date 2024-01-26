#pragma once

namespace cppsafe {

struct CppsafeOptions {
    bool NoLifetimeNull = false;
    bool LifetimePost = false;
    bool LifetimeDisabled = false;
    bool LifetimeGlobal = false;
    bool LifetimeOutput = false;
};

}
