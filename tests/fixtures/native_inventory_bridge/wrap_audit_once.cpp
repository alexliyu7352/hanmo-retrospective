// 仅链接期异常回归：生产库隐藏静态符号后，旧LD_PRELOAD注入不再适用。
#include "hanmo/inventory_projection.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>

// GNU --wrap把此声明解析到原audit，使用同一个最终ELF内的C++ ABI。
extern "C" hanmo::ProjectionAudit __real__ZNK5hanmo19InventoryProjection5auditEv(
    const hanmo::InventoryProjection* instance);

// 明确标记只消费一次；使用同一静态运行库抛异常，其余调用精确转发原实现。
extern "C" hanmo::ProjectionAudit __wrap__ZNK5hanmo19InventoryProjection5auditEv(
    const hanmo::InventoryProjection* instance) {
    if (std::getenv("HANMO_REVIEW_FAIL_NEXT_AUDIT")) {
        unsetenv("HANMO_REVIEW_FAIL_NEXT_AUDIT");
        std::fprintf(stderr, "INJECTION: link-wrapped audit bad_alloc once\n");
        throw std::bad_alloc{};
    }
    return __real__ZNK5hanmo19InventoryProjection5auditEv(instance);
}
