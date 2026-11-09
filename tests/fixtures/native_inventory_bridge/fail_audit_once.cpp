// 仅局部异常回归用：拦截一次真实候选audit，使用候选自己的C++运行库抛bad_alloc。
#include "hanmo/inventory_projection.hpp"
#include <cstdlib>
#include <cstdio>
#include <dlfcn.h>
namespace hanmo {
// 环境标记只消费一次；未标记时精确转发原audit，不替换任何领域结果。
ProjectionAudit InventoryProjection::audit() const {
    void* handle=dlopen(std::getenv("HANMO_REVIEW_BRIDGE_LIBRARY"),RTLD_NOW|RTLD_NOLOAD);
    if(!handle) { std::fprintf(stderr,"INJECTION_ERROR: candidate not loaded\n"); std::abort(); }
    if(std::getenv("HANMO_REVIEW_FAIL_NEXT_AUDIT")) {
        unsetenv("HANMO_REVIEW_FAIL_NEXT_AUDIT");
        const auto allocate=reinterpret_cast<void*(*)(std::size_t)>(dlsym(handle,"__cxa_allocate_exception"));
        const auto raise=reinterpret_cast<void(*)(void*,void*,void(*)(void*))>(dlsym(handle,"__cxa_throw"));
        const auto destroy=reinterpret_cast<void(*)(void*)>(dlsym(handle,"_ZNSt9bad_allocD1Ev"));
        void* type=dlsym(handle,"_ZTISt9bad_alloc");
        auto vtable=reinterpret_cast<void**>(dlsym(handle,"_ZTVSt9bad_alloc"));
        if(!allocate || !raise || !destroy || !type || !vtable) std::abort();
        auto object=reinterpret_cast<void**>(allocate(sizeof(void*)));
        *object=vtable+2;
        std::fprintf(stderr,"INJECTION: candidate-runtime bad_alloc at audit once\n");
        raise(object,type,destroy); std::abort();
    }
    using Function=ProjectionAudit(*)(const InventoryProjection*);
    const auto original=reinterpret_cast<Function>(dlsym(handle,"_ZNK5hanmo19InventoryProjection5auditEv"));
    if(!original) std::abort();
    return original(this);
}
} // hook不进入正式桥或发行库。

// 仅指定候选去掉DEEPBIND以允许故障注入；不改候选二进制，也不改变其他动态库装载。
extern "C" void* dlopen(const char* name,int flags) {
    using Open=void*(*)(const char*,int);
    const auto next=reinterpret_cast<Open>(dlsym(RTLD_NEXT,"dlopen"));
    if(name && __builtin_strstr(name,"libhanmo_world_bridge.so")) flags &= ~RTLD_DEEPBIND;
    return next(name,flags);
}
