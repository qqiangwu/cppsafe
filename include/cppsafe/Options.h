#pragma once

namespace cppsafe {

struct CppsafeOptions {
    bool LifetimeNull = false;
    bool LifetimeCallNull = false;
    bool LifetimePost = false;
    bool LifetimeDisabled = false;
    bool LifetimeGlobal = false;
    bool LifetimeOutput = false;
    bool LifetimeContainerMove = false;
};

}
