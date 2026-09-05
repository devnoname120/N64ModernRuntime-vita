#include <cassert>
#include "threadqueue_debug.hpp"
#ifdef RECOMP_SCHEDULER_DIAGNOSTICS
#include <array>
#include <mutex>
#include <cstdio>
#endif

#include "ultramodern/ultramodern.hpp"

static PTR(OSThread) running_queue_impl = NULLPTR;

#ifdef RECOMP_SCHEDULER_DIAGNOSTICS
namespace {
struct QueueEvent { char operation; int32_t caller, queue, thread, next, ready; };
std::array<QueueEvent,128> queue_events{};
unsigned queue_events_count=0;
std::mutex queue_events_mutex;
}
void debug_queue_event(char operation, int32_t queue, int32_t thread, int32_t next) {
    std::lock_guard lock(queue_events_mutex);
    queue_events[queue_events_count++%queue_events.size()]={operation,ultramodern::this_thread(),queue,thread,next,running_queue_impl};
}
extern "C" void ultramodern_debug_queue_history(void (*output)(const char *)) {
    std::array<QueueEvent,128> events;
    unsigned count;
    { std::lock_guard lock(queue_events_mutex); events=queue_events; count=queue_events_count; }
    for(unsigned i=count>events.size()?count-events.size():0;i<count;++i) {
        const auto &e=events[i%events.size()]; char line[192];
        std::snprintf(line,sizeof(line),"Queue history %u %c caller=%08x queue=%08x thread=%08x next=%08x ready=%08x",
            i,e.operation,unsigned(e.caller),unsigned(e.queue),unsigned(e.thread),unsigned(e.next),unsigned(e.ready));
        output(line);
    }
}
#endif

static PTR(OSThread)* queue_to_ptr(RDRAM_ARG PTR(PTR(OSThread)) queue) {
    if (queue == ultramodern::running_queue) {
        return &running_queue_impl;
    }
    return TO_PTR(PTR(OSThread), queue);
}

void ultramodern::thread_queue_insert(RDRAM_ARG PTR(PTR(OSThread)) queue_, PTR(OSThread) toadd_) {
    PTR(OSThread)* cur = queue_to_ptr(PASS_RDRAM queue_);
    OSThread* toadd = TO_PTR(OSThread, toadd_); 
    debug_printf("[Thread Queue] Inserting thread %d into queue 0x%08X\n", toadd->id, (uintptr_t)queue_);
    while (*cur && TO_PTR(OSThread, *cur)->priority > toadd->priority) {
        cur = &TO_PTR(OSThread, *cur)->next;
    }
    toadd->next = (*cur);
    toadd->queue = queue_;
    *cur = toadd_;
    debug_queue_event('I',queue_,toadd_,toadd->next);

    debug_printf("  Contains:");
    cur = queue_to_ptr(PASS_RDRAM queue_);
    while (*cur) {
        debug_printf("%d (%d) ", TO_PTR(OSThread, *cur)->id, TO_PTR(OSThread, *cur)->priority);
        cur = &TO_PTR(OSThread, *cur)->next;
    }
    debug_printf("\n");
}

PTR(OSThread) ultramodern::thread_queue_pop(RDRAM_ARG PTR(PTR(OSThread)) queue_) {
    PTR(OSThread)* queue = queue_to_ptr(PASS_RDRAM queue_);
    PTR(OSThread) ret = *queue;
    *queue = TO_PTR(OSThread, ret)->next;
    TO_PTR(OSThread, ret)->queue = NULLPTR;
    debug_queue_event('P',queue_,ret,*queue);
    debug_printf("[Thread Queue] Popped thread %d from queue 0x%08X\n", TO_PTR(OSThread, ret)->id, (uintptr_t)queue_);
    return ret;
}

bool ultramodern::thread_queue_remove(RDRAM_ARG PTR(PTR(OSThread)) queue_, PTR(OSThread) t_) {
    debug_queue_event('D',queue_,t_,0);
    debug_printf("[Thread Queue] Removing thread %d from queue 0x%08X\n", TO_PTR(OSThread, t_)->id, (uintptr_t)queue_);

    PTR(PTR(OSThread)) cur = queue_;
    while (cur != NULLPTR) {
        PTR(OSThread)* cur_ptr = queue_to_ptr(PASS_RDRAM queue_);
        if (*cur_ptr == t_) {
            *cur_ptr = TO_PTR(OSThread, *cur_ptr)->next;
            return true;
        }
        cur = TO_PTR(OSThread, *cur_ptr)->next;
    }

    return false;
}

bool ultramodern::thread_queue_empty(RDRAM_ARG PTR(PTR(OSThread)) queue_) {
    PTR(OSThread)* queue = queue_to_ptr(PASS_RDRAM queue_);
    return *queue == NULLPTR;
}

PTR(OSThread) ultramodern::thread_queue_peek(RDRAM_ARG PTR(PTR(OSThread)) queue_) {
    PTR(OSThread)* queue = queue_to_ptr(PASS_RDRAM queue_);
    return *queue;
}
