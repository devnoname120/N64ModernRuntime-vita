#pragma once
#include <cstdint>
#include <stdexcept>

namespace ultramodern {
// Callers serialize transitions and their SP/DP notifications under one lock.
// Yield acknowledges SP only; both final events stay deferred until resume.
class YieldableGraphicsTask {
public:
    enum class Start { Execute, Resume, Complete };
private:
    uint32_t address=0;
    bool active=false, suspended=false, finished=false;
public:
    Start start(uint32_t task) {
        if(!active) {
            address=task;active=true;suspended=false;finished=false;
            return Start::Execute;
        }
        if(task!=address || !suspended)
            throw std::logic_error("Graphics task submitted before its predecessor completed");
        suspended=false;
        if(finished) { active=false;return Start::Complete; }
        return Start::Resume;
    }
    bool request_yield() {
        if(!active || suspended || finished)return false;
        suspended=true;
        return true;
    }
    bool was_yielded(uint32_t task) const {
        return active && suspended && task==address;
    }
    bool complete(uint32_t task) {
        if(!active || finished || task!=address)
            throw std::logic_error("Unexpected graphics task completion");
        finished=true;
        // DP can release guest inputs independently of final SP notification.
        if(suspended)return false;
        active=false;
        return true;
    }
};
}
