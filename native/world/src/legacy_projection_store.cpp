// 两个离线profile共用一次性Flecs安装；候选完整构建后才发布，无刷新或世界写回。
#include "legacy_projection_store_internal.hpp"
#include "inventory_core.hpp"
#include "typed_wire.hpp"
#include <new>

namespace hanmo {
namespace {
// 每个记录域建立独立匿名实体索引；StableId只存原字节，不用Flecs名称解析路径。
template<class T> void install_rows(flecs::world& world,std::map<std::string,flecs::entity>& index,const std::vector<T>& rows) {
    world.component<T>();
    for(const auto& row:rows) {
        auto entity=world.entity();
        entity.set<detail::StableId>({row.id}); entity.set<T>(row);
        index.emplace(row.id,entity);
    }
}
}
// 空实例尚无Flecs世界，失败解码不会构造第二份状态。
ProjectionStore::ProjectionStore()=default;
// 析构只释放本离线缓存，不触及原Kernel或工件文件。
ProjectionStore::~ProjectionStore()=default;

// 仅精确新format进入新decoder；其它形状仍交旧v1，保持原首错和失败可重试。
Result<bool> ProjectionStore::install_once(const std::string& bytes) {
    if(impl_) return {false,false,{"AlreadyInstalled","/runtime"}};
    detail::Input old_input;
    detail::LegacyProjectionInput legacy_input;
    bool legacy=false;
    try {
        const auto envelope=detail::parse(bytes,"/artifact");
        legacy=envelope.is_object() && envelope.contains("format") && envelope.at("format")==detail::legacy_profile;
        if(legacy) {
            auto decoded=detail::decode_legacy_artifact(bytes);
            if(!decoded.ok) return {false,false,decoded.error};
            legacy_input=std::move(decoded.value);
        } else {
            auto decoded=detail::decode_artifact(bytes);
            if(!decoded.ok) return {false,false,decoded.error};
            old_input=std::move(decoded.value);
        }
    } catch(const detail::Failure& failure) { return {false,false,failure.error}; }
      catch(const std::bad_alloc&) { return {false,false,{"ProjectionResourceLimit","/artifact"}}; }
    try {
        auto candidate=std::make_unique<Impl>(); candidate->world.component<detail::StableId>();
        candidate->profile=legacy?detail::legacy_profile:detail::inventory_profile;
        if(legacy) {
            candidate->input_sha256=legacy_input.input_sha256; candidate->payload_sha256=legacy_input.payload_sha256;
            install_rows(candidate->world,candidate->actors,legacy_input.actors);
            install_rows(candidate->world,candidate->wallets,legacy_input.wallets);
            install_rows(candidate->world,candidate->inventories,legacy_input.inventories);
            install_rows(candidate->world,candidate->activities,legacy_input.activities);
            install_rows(candidate->world,candidate->offices,legacy_input.offices);
            install_rows(candidate->world,candidate->organizations,legacy_input.organizations);
            install_rows(candidate->world,candidate->facts,legacy_input.facts);
            install_rows(candidate->world,candidate->items,legacy_input.items);
            candidate->world.component<detail::ProjectionMetadata>();
            candidate->metadata=candidate->world.entity();
            candidate->metadata.set<detail::ProjectionMetadata>({std::move(legacy_input.source),std::move(legacy_input.provenance),std::move(legacy_input.references),std::move(legacy_input.coverage)});
        } else {
            candidate->input_sha256=old_input.input_sha256; candidate->payload_sha256=old_input.payload_sha256;
            install_rows(candidate->world,candidate->inventories,old_input.inventories);
        }
        impl_=std::move(candidate);
        return {true,true,{}};
    } catch(const std::bad_alloc&) { return {false,false,{"ProjectionResourceLimit","/runtime"}}; }
}

// 实际组件重新组装出独立摘要；旧v1新增map保持空，不添虚构Actor/Wallet或metadata。
ProjectionAudit ProjectionStore::audit() const {
    ProjectionAudit result;
    if(!impl_) return result;
    result.installed=true; result.install_count=1; result.profile=impl_->profile;
    result.input_sha256=impl_->input_sha256; result.payload_sha256=impl_->payload_sha256;
    for(const auto& [id,entity]:impl_->inventories) {
        const auto* inventory=entity.view().try_get<detail::InventoryState>();
        if(!inventory) continue;
        for(const auto& lot:inventory->lots) result.lot_ids[id].push_back(lot.id);
        // 空库存也应保原审计键，不把零批次误写成不存在。
        result.lot_ids.try_emplace(id);
        result.component_sha256[id]=detail::component_digest(*inventory);
    }
    if(impl_->profile!=detail::legacy_profile) return result;
    // 类型标签从实际组件重组，不能以已缓存payload或输入摘要代替内容证明。
    auto audit_rows=[&](const auto& index,const std::string& domain,auto record) {
        auto& rows=result.record_component_sha256[domain];
        for(const auto& [id,entity]:index) rows[id]=record(entity);
    };
    audit_rows(impl_->actors,"actors",[](flecs::entity e){return sha256(detail::encode_value(detail::actor_record(*e.view().try_get<detail::ActorState>())).dump());});
    audit_rows(impl_->wallets,"wallets",[](flecs::entity e){return sha256(detail::encode_value(detail::wallet_record(*e.view().try_get<detail::WalletState>())).dump());});
    audit_rows(impl_->inventories,"inventories",[](flecs::entity e){return detail::component_digest(*e.view().try_get<detail::InventoryState>());});
    audit_rows(impl_->activities,"activities",[](flecs::entity e){return sha256(detail::encode_value(detail::activity_record(*e.view().try_get<detail::LegacyActivity>())).dump());});
    audit_rows(impl_->offices,"offices",[](flecs::entity e){return sha256(detail::encode_value(detail::office_record(*e.view().try_get<detail::LegacyOffice>())).dump());});
    audit_rows(impl_->organizations,"organizations",[](flecs::entity e){return sha256(detail::encode_value(detail::organization_record(*e.view().try_get<detail::OrganizationDefinition>())).dump());});
    audit_rows(impl_->facts,"facts",[](flecs::entity e){return sha256(detail::encode_value(detail::fact_record(*e.view().try_get<detail::LegacyFact>())).dump());});
    audit_rows(impl_->items,"items",[](flecs::entity e){return sha256(detail::encode_value(detail::item_record(*e.view().try_get<detail::ItemDefinition>())).dump());});
    const auto* metadata=impl_->metadata.view().try_get<detail::ProjectionMetadata>();
    detail::Value::Dict references;
    for(const auto& [kind,ids]:metadata->references) {
        detail::Value::Array values; for(const auto& id:ids) values.push_back(detail::Value{id});
        references.emplace(kind,detail::Value{std::move(values)});
    }
    auto& rows=result.record_component_sha256["metadata"];
    rows["source"]=sha256(detail::encode_value(detail::Value{metadata->source}).dump());
    rows["provenance"]=sha256(detail::encode_value(detail::Value{metadata->provenance}).dump());
    rows["references"]=sha256(detail::encode_value(detail::Value{std::move(references)}).dump());
    rows["coverage"]=sha256(detail::encode_value(metadata->coverage).dump());
    return result;
}
} // 候选发布与审计不具备领域命令写权限。
