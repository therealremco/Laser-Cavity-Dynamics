#pragma once

#include "cavdyn/Results.h"

namespace cavdyn {

/// Hook for a progress display or a GUI. Simulator::run() calls on_step()
/// after every step with that step's results and a read-only view of the
/// current state (valid only during the call). Snapshots requested through
/// simulation.snapshot_interval are collected in Results independently.
class Observer {
public:
    virtual ~Observer() = default;
    /// Return false to stop the run after this step.
    virtual bool on_step(const StepResult& result, StateView state) = 0;
};

} // namespace cavdyn
