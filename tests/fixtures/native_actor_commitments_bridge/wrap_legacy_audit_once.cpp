// 专用链接期一次异常门；生产源码无hook，候选自己的C++运行库负责抛出与回收。
#include "hanmo/legacy_projection.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
extern "C" hanmo::ProjectionAudit __real__ZNK5hanmo15ProjectionStore5auditEv(const hanmo::ProjectionStore* instance);
// 只消费本片独立标记一次；未标记时精确调用原真实store.audit。
extern "C" hanmo::ProjectionAudit __wrap__ZNK5hanmo15ProjectionStore5auditEv(const hanmo::ProjectionStore* instance) {
    if(std::getenv("HANMO_REVIEW_FAIL_NEXT_LEGACY_AUDIT")) {
        unsetenv("HANMO_REVIEW_FAIL_NEXT_LEGACY_AUDIT");
        std::fprintf(stderr,"INJECTION: link-wrapped legacy audit bad_alloc once\n");
        throw std::bad_alloc{};
    }
    return __real__ZNK5hanmo15ProjectionStore5auditEv(instance);
}
