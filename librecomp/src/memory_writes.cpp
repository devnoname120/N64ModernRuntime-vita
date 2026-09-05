#include "librecomp/memory_writes.hpp"
#include "recomp.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

extern "C" { uint8_t recomp_watched_pages[0x20000]{}; }
namespace {
    uint8_t *tracked_rdram=nullptr;
    uint32_t tracked_size=0;
    std::vector<uint32_t> page_users,dirty_pages,dirty_bytes;
}

void recomp::initialize_memory_writes(uint8_t *rdram,uint32_t size) {
    if(!rdram || !size || size>0x20000000U || (size&3))
        throw std::invalid_argument("Invalid tracked RDRAM");
    std::memset(recomp_watched_pages,0,sizeof(recomp_watched_pages));
    page_users.assign((size+4095)/4096,0);
    dirty_pages.assign(page_users.size(),0);
    dirty_bytes.assign((size+31)/32,0);
    tracked_rdram=rdram; tracked_size=size;
}

void recomp::watch_memory_writes(uint32_t address,uint32_t size,bool enable) {
    if(!size || address>=tracked_size || size>tracked_size-address)
        throw std::out_of_range("Watched memory range exceeds RDRAM");
    for(uint32_t page=address>>12;page<=(address+size-1)>>12;++page) {
        auto &users=page_users[page];
        if(enable) {
            if(!users) {
                const uint32_t end=std::min<uint32_t>(dirty_bytes.size(),(page+1)*128);
                for(uint32_t word=page*128;word<end;++word) __atomic_store_n(&dirty_bytes[word],0U,__ATOMIC_RELAXED);
                __atomic_store_n(&dirty_pages[page],0U,__ATOMIC_RELAXED);
            }
            ++users;
        }
        else if(users) --users;
        else throw std::logic_error("Unbalanced memory watch release");
        __atomic_store_n(&recomp_watched_pages[page],uint8_t(users!=0),__ATOMIC_RELEASE);
    }
}

extern "C" void recomp_record_memory_write(uint8_t *rdram,uint32_t address,uint32_t size) {
    if(rdram!=tracked_rdram || !size || address>=tracked_size || size>tracked_size-address) return;
    const uint32_t end=address+size;
    while(address<end) {
        const uint32_t page=address>>12;
        const uint32_t page_end=std::min(end,(page+1)*4096);
        if(__atomic_load_n(&recomp_watched_pages[page],__ATOMIC_RELAXED)) {
            while(address<page_end) {
                const uint32_t first=address&31,count=std::min(page_end-address,32-first);
                const uint32_t mask=(UINT32_MAX>>(32-count))<<first;
                __atomic_fetch_or(&dirty_bytes[address>>5],mask,__ATOMIC_RELEASE);
                address+=count;
            }
            __atomic_store_n(&dirty_pages[page],1U,__ATOMIC_RELEASE);
        }
        address=page_end;
    }
}

std::vector<recomp::MemoryWrite> recomp::collect_memory_writes() {
    std::vector<MemoryWrite> result;
    for(uint32_t page=0;page<dirty_pages.size();++page) {
        if(!__atomic_load_n(&dirty_pages[page],__ATOMIC_RELAXED)) continue;
        if(!__atomic_exchange_n(&dirty_pages[page],0U,__ATOMIC_ACQUIRE)) continue;
        const uint32_t end=std::min<uint32_t>(dirty_bytes.size(),(page+1)*128);
        for(uint32_t word=page*128;word<end;++word) {
            const uint32_t mask=__atomic_exchange_n(&dirty_bytes[word],0U,__ATOMIC_ACQUIRE);
            if(!mask) continue;
            MemoryWrite write{word*32,mask};
            for(unsigned i=0;i<32;++i) if(mask&(1U<<i)) write.bytes[i]=tracked_rdram[(write.address+i)^3];
            result.push_back(write);
        }
    }
    return result;
}
