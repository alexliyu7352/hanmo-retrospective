// 携货保护的同候选纯准备口；共同原语只发布临时行，永久原因仍由原业务绑定。
#pragma once
#include "resource_execution_draft.hpp"
namespace hanmo::resident {
class CommonStore;
struct ProtectedFreightCaptureIdentity;
struct ProtectedFreightRecipeIdentity;
// 领域可派生只读准备结果；构造/析构均不得改Common或领域旁账，不提供提交回调。
class PreparedFreightRecipe {
public:
    virtual ~PreparedFreightRecipe()=default;
};
// 每个成功recipe由Common封真实执行与唯一行身份；回滚重做相同值也不会复用该行。
struct ProtectedFreightResourceDraft {
    std::shared_ptr<const ProtectedFreightCaptureIdentity> capture;
    std::shared_ptr<const ProtectedFreightRecipeIdentity> identity;
    std::size_t recipe_index=0;
    ResourceExecutionDraft execution;
    std::unique_ptr<const PreparedFreightRecipe> prepared;
};
// 只借同栈领域服务；prepare失败或后续原市场拒绝均不能留下外部已接受状态。
class ProtectedFreightResourceRead {
public:
    virtual ~ProtectedFreightResourceRead()=default;
    // 全recipe已完成真实staging但尚未发布；必须纯读并返回本次独占暂存，成功空值亦拒绝。
    virtual Outcome<std::unique_ptr<const PreparedFreightRecipe>> prepare_freight_recipe(
        const CommonStore&,const ResourceExecutionDraft&) const=0;
};
} // 不写存档，不授权任意库存历史、业务原因或到店结算。
