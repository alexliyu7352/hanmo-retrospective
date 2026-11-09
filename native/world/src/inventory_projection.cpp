// 旧v1公开门面薄委托同一ProjectionStore；输入合同、状态首错和审计格式保持。
#include "hanmo/inventory_projection.hpp"
#include "hanmo/legacy_projection.hpp"
#include "wire.hpp"
#include <new>

namespace hanmo {
// 只持同一只读store，不再建立独立库存Flecs世界或第二份count算法。
struct InventoryProjection::Impl { ProjectionStore store; };
// 未安装实例不建Flecs世界；失败仍允许同对象重试。
InventoryProjection::InventoryProjection()=default;
// 包装析构只释放其唯一store。
InventoryProjection::~InventoryProjection()=default;

// 先保原v1解码合同，再由同store构建候选；不能让旧Bridge默收新profile。
Result<bool> InventoryProjection::install_once(const std::string& bytes) {
    if(impl_) return {false,false,{"AlreadyInstalled","/runtime"}};
    auto parsed=detail::decode_artifact(bytes);
    if(!parsed.ok) return {false,false,parsed.error};
    try {
        auto candidate=std::make_unique<Impl>();
        const auto installed=candidate->store.install_once(bytes);
        if(!installed.ok) return installed;
        impl_=std::move(candidate);
        return {true,true,{}};
    } catch(const std::bad_alloc&) { return {false,false,{"ProjectionResourceLimit","/runtime"}}; }
}

// 未安装先拒；原整批查询委托唯一库存count，不改变未知匹配为零的原义。
Result<std::vector<CountRow>> InventoryProjection::query_batch(const std::string& bytes) const {
    if(!impl_) return {false,{}, {"NotInstalled","/runtime"}};
    return impl_->store.query_count_batch(bytes);
}

// 旧公开审计仅保旧字段；新profile审计不从旧门面扩散到已发表Bridge协议。
ProjectionAudit InventoryProjection::audit() const {
    if(!impl_) return {};
    auto result=impl_->store.audit();
    result.profile.clear(); result.record_component_sha256.clear();
    return result;
}
} // 对旧调用方仍然是InventoryProjection的同一合同。
