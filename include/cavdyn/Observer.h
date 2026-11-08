#pragma once

#include "cavdyn/Results.h"

namespace cavdyn {

class Observer {
public:
    virtual ~Observer() = default;
    virtual bool on_step(const StepResult& result, StateView state) = 0;
};

} // namespace cavdyn
