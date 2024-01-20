#pragma once

namespace cppsafe {

struct CppsafeOptions {
    bool NoLifetimeNull = false;
    bool NoLifetimePost = false;
    bool LifetimeDisabled = false;
    bool LifetimeGlobal = false;
    bool LifetimeOutput = false;
};

}
