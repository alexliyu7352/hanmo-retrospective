// 访问器仅在调用栈内借同一候选；复制的原legacy事务不能回读旧控制状态或旧关系账。
#include "disciple_control_world.hpp"
#include "biological_control_world.hpp"
#include "collateral_control_world.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {namespace {
Status profile(const WorldBundle& w){
    const bool enabled=w.family_disciple_legacy_enabled();Integer markers=0;for(const auto& m:w.catalog->identity.module_versions)if(m.first=="family_disciple_legacy"){++markers;if(m.second!="1")return common_failure("UnsupportedDiscipleControlVersion");}
    if(enabled!=w.family_disciple_legacy_seed_complete||bool(w.catalog->family_disciple_legacy)!=enabled||markers!=(enabled?1:0))return common_failure("DiscipleControlProfileMismatch");
    if(!enabled)return w.family_disciple_legacy.commits.empty()?common_success():common_failure("DiscipleControlProfileRequired");
    if(!w.dynamic_enabled()||!w.family_life_enabled()||!w.family_legacy_enabled()||!w.family_apprenticeship_enabled()||!w.catalog->family->enabled||w.catalog->interaction->common.get()!=w.catalog->common.get())return common_failure("DiscipleControlDependenciesRequired");
    return validate_disciple_control_catalog(*w.catalog->family_disciple_legacy,*w.catalog->family_legacy,*w.catalog->common,*w.catalog->interaction);
}
// 所有状态引用随这份World的调用栈生灭；没有捕获前一World或暂存的legacy副本。
std::optional<DiscipleControlRelationContext> access(const WorldBundle& w){if(!w.family_disciple_legacy_enabled())return {};return DiscipleControlRelationContext{w.common,*w.catalog->common,w.family_apprenticeship,*w.catalog->family_legacy,*w.catalog->family_disciple_legacy,w.family_apprenticeship_access()};}
}
Status validate_family_disciple_legacy_world(const WorldBundle& w){
    auto valid=profile(w);if(!valid.ok||!w.family_disciple_legacy_enabled())return valid;auto relation=access(w);std::shared_ptr<CollateralControlWorldAccess> collateral;if(w.family_collateral_legacy_enabled()){auto selected=CollateralControlWorldAccess::create(w);if(!selected.ok)return {false,{},selected.error};collateral=std::move(selected.value);}std::shared_ptr<BiologicalControlWorldAccess> biological;if(w.family_biological_legacy_enabled()){auto selected=BiologicalControlWorldAccess::create(w);if(!selected.ok)return {false,{},selected.error};biological=std::move(selected.value);}
    valid=validate_family_legacy(w.common,w.family,w.family_life,w.family_death_read(),*w.catalog->common,*w.catalog->interaction,*w.catalog->family_legacy,w.family_legacy,w.family_legacy_playable_read(),&*relation,biological?&biological->context():nullptr,collateral?&collateral->context():nullptr);if(!valid.ok)return valid;
    return validate_disciple_control_result_bindings(w.common,w.family_legacy,*w.catalog->family_disciple_legacy,w.family_disciple_legacy);
}
Status bind_family_disciple_legacy_result_world(WorldBundle& w,const FamilyLegacyState* before_legacy,const DiscipleControlState* before_bindings,const StableId& actor,Integer request,const CommitResult& result){
    if(!w.family_disciple_legacy_enabled())return before_legacy||before_bindings?common_failure("DiscipleControlProfileRequired"):common_success();
    auto valid=profile(w);if(!valid.ok)return valid;if(!before_legacy||!before_bindings)return common_failure("DiscipleControlBeforeStateRequired");
    return bind_disciple_control_result_in_candidate(w.common,*before_legacy,w.family_legacy,*w.catalog->family_disciple_legacy,*before_bindings,w.family_disciple_legacy,actor,request,result,w.family_collateral_legacy_enabled()?FamilyLegacyVersion::collateral3:w.family_biological_legacy_enabled()?FamilyLegacyVersion::biological2:FamilyLegacyVersion::original1);
}
Outcome<FamilyLegacyQuote> preview_family_legacy_world(const WorldBundle& w,const StableId& actor,const FamilyLegacyCommand& command){
    auto valid=profile(w);if(!valid.ok)return {false,{},valid.error};if(!w.family_legacy_enabled())return {false,{},common_failure("LegacyControlConsentProfileRequired").error};auto relation=access(w);std::shared_ptr<CollateralControlWorldAccess> collateral;if(w.family_collateral_legacy_enabled()){auto selected=CollateralControlWorldAccess::create(w);if(!selected.ok)return {false,{},selected.error};collateral=std::move(selected.value);}std::shared_ptr<BiologicalControlWorldAccess> biological;if(w.family_biological_legacy_enabled()){auto selected=BiologicalControlWorldAccess::create(w);if(!selected.ok)return {false,{},selected.error};biological=std::move(selected.value);}auto interaction=w.services.interaction_access();
    return preview_family_legacy_command(w.common,w.family,w.family_life,w.family_death_read(),w.interaction,*w.catalog->interaction,interaction,*w.catalog->family_legacy,w.family_legacy,w.family_legacy_playable_read(),actor,command,relation?&*relation:nullptr,biological?&biological->context():nullptr,collateral?&collateral->context():nullptr);
}
Outcome<FamilyEffect> apply_family_legacy_world(WorldBundle& w,const StableId& actor,const FamilyLegacyCommand& command,const FamilyLegacyQuote& quote){
    auto valid=profile(w);if(!valid.ok)return {false,{},valid.error};if(!w.family_legacy_enabled())return {false,{},common_failure("LegacyControlConsentProfileRequired").error};auto relation=access(w);std::shared_ptr<CollateralControlWorldAccess> collateral;if(w.family_collateral_legacy_enabled()){auto selected=CollateralControlWorldAccess::create(w);if(!selected.ok)return {false,{},selected.error};collateral=std::move(selected.value);}std::shared_ptr<BiologicalControlWorldAccess> biological;if(w.family_biological_legacy_enabled()){auto selected=BiologicalControlWorldAccess::create(w);if(!selected.ok)return {false,{},selected.error};biological=std::move(selected.value);}auto interaction=w.services.interaction_access();
    return apply_family_legacy_command(w.common,w.family,w.family_life,w.family_death_read(),w.interaction,*w.catalog->interaction,interaction,*w.catalog->family_legacy,w.family_legacy,w.family_legacy_playable_read(),actor,command,quote,relation?&*relation:nullptr,biological?&biological->context():nullptr,collateral?&collateral->context():nullptr);
}
Outcome<BoundaryValue> query_family_legacy_world(const WorldBundle& w,Integer page){
    auto valid=profile(w);if(!valid.ok)return {false,{},valid.error};if(!w.family_legacy_enabled())return {false,{},common_failure("LegacyControlConsentProfileRequired").error};auto relation=access(w);std::shared_ptr<CollateralControlWorldAccess> collateral;if(w.family_collateral_legacy_enabled()){auto selected=CollateralControlWorldAccess::create(w);if(!selected.ok)return {false,{},selected.error};collateral=std::move(selected.value);}std::shared_ptr<BiologicalControlWorldAccess> biological;if(w.family_biological_legacy_enabled()){auto selected=BiologicalControlWorldAccess::create(w);if(!selected.ok)return {false,{},selected.error};biological=std::move(selected.value);}auto interaction=w.services.interaction_access();
    return query_family_legacy_page(w.common,w.family,w.family_life,w.family_death_read(),w.interaction,*w.catalog->interaction,interaction,*w.catalog->family_legacy,w.family_legacy,w.family_legacy_playable_read(),page,relation?&*relation:nullptr,biological?&biological->context():nullptr,collateral?&collateral->context():nullptr);
}
}
