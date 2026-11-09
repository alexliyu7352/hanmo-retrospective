// 单一Flecs存储的私有布局；各记录域独立索引，不以同名字符串合并身份。
#pragma once
#include "hanmo/legacy_projection.hpp"
#include "wire.hpp"
#include <flecs.h>

namespace hanmo::detail {
// 四项元数据只存值拥有组件，不保留可反传的原工件JSON或world_id。
struct ProjectionMetadata {
    Value::Dict source,provenance;
    std::map<std::string,std::set<std::string>> references;
    Value coverage;
};
inline constexpr const char* inventory_profile="HM-NATIVE-INVENTORY-ARTIFACT/1";
inline constexpr const char* legacy_profile="HM-NATIVE-LEGACY-ARTIFACT/1";
// 只读访问实际组件；缺失组件由调用方法转换为明确失败，不造默认记录。
template<class T> const T* component(const std::map<std::string,flecs::entity>& index,const std::string& id) {
    const auto found=index.find(id);
    return found==index.end()?nullptr:found->second.view().try_get<T>();
}
// 原记录重组用于实际组件审计，known与extras的互斥已由唯一decoder核定。
Value actor_record(const ActorState&);
Value wallet_record(const WalletState&);
Value activity_record(const LegacyActivity&);
Value office_record(const LegacyOffice&);
Value organization_record(const OrganizationDefinition&);
Value fact_record(const LegacyFact&);
Value item_record(const ItemDefinition&);
} // 私有组件及重组，不形成第二领域实现。

namespace hanmo {
// 所有profile共享这一个世界；v1仅安装库存，不生成新五类占位组件。
struct ProjectionStore::Impl {
    flecs::world world;
    std::map<std::string,flecs::entity> actors,wallets,inventories,activities,offices,organizations,facts,items;
    flecs::entity metadata;
    std::string profile,input_sha256,payload_sha256;
};
} // 公开头只有PIMPL，不能取得Flecs可写句柄。
