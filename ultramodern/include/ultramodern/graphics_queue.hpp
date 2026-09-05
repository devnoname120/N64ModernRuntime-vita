#pragma once
#include "blockingconcurrentqueue.h"
#include <mutex>
#include <utility>

namespace ultramodern {
    // VI interrupts keep running at the guest rate even if rasterization is
    // slower. Retain only the latest scanout registers behind one queued action.
    template<class Registers> class LatestPresentation {
        std::mutex mutex;
        Registers latest{};
        bool pending=false;
    public:
        bool publish(const Registers &value) {
            std::lock_guard lock(mutex);
            latest=value;
            return !std::exchange(pending,true);
        }
        Registers consume() {
            std::lock_guard lock(mutex);
            pending=false;
            return latest;
        }
    };
    // A synchronous display backend can maintain a non-empty VI stream. The
    // default heuristic always favors the larger producer subqueue and can
    // starve a lone graphics task forever. Use a token with prompt rotation.
    struct VitaGraphicsQueueTraits : moodycamel::ConcurrentQueueDefaultTraits {
        // Keep this >= 2: this queue version counts the first fallback dequeue
        // as 1 without rotating. A quota of 1 can then miss the equality check
        // forever when that producer remains nonempty.
        static constexpr std::uint32_t EXPLICIT_CONSUMER_CONSUMPTION_QUOTA_BEFORE_ROTATE = 2;
    };
}
