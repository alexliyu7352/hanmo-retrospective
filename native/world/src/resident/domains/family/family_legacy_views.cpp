#include <type_traits>
// 控制接续页不向继受者注入死亡知识；只展示用户自己的明确许可与有限控制选择。
#include "family_legacy_commands.hpp"
#include "biological_control_sources.hpp"
#include "disciple_control_sources.hpp"
#include "resident/views/common_presentation.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
// 页身份沿原世界/代次/版本/时刻绑定，旧回调不得在冷恢复或换人后重用。
V identity(const CommonStore& c){using namespace boundary;StableId world;const char* digits="0123456789abcdef";for(auto b:c.state().identity.world_id){world+=digits[b>>4];world+=digits[b&15];}const auto& id=c.state().identity;return obj({{"world_id",V{world}},{"execution_epoch",V{id.execution_epoch}},{"revision",V{id.revision}},{"tick",V{id.tick}}});}
// 候选基本资料来自已有真人；不复制前代技能、钱包和角色，不扩散无关人物资料。
V person(const CommonStore& c,const CommonCatalog& shared,const StableId& id){using namespace boundary;const auto* a=c.state().actors.find(id);if(!a)return obj({});auto name=CommonPresentation(c.state(),shared).display_name(id);return obj({{"actor_id",V{id}},{"name",V{name.ok?name.value:id}},{"age_years",V{a->birth_tick&&*a->birth_tick>=-2147483647&&*a->birth_tick<=c.tick()?(c.tick()-*a->birth_tick)/720:Integer{-1}}}});}
}
// 查询仅形成业务报价和拒因，不写事实、控制、资源、知情或RNG。
Outcome<BoundaryValue> query_family_legacy_page(const CommonStore& c,const FamilyState& family,const FamilyDeathState& deaths,const FamilyDeathOriginRead& read,const InteractionState& interaction_state,const InteractionCatalog& interaction,const InteractionAccess& access,const LegacyControlConsentCatalog& catalog,const FamilyLegacyState& legacy,const FamilyLegacyPlayableRead& playable,Integer page,const DiscipleControlRelationContext* disciple,const BiologicalControlDecisionContext* biological,const CollateralControlDecisionContext* collateral,const FamilyContinuationPeopleRead4* complete_people){
    using namespace boundary;if(page<0||page>255)return {false,{},common_failure("InvalidLegacyPage").error};if((!catalog.enabled&&!(biological&&biological->catalog.enabled))||!interaction.common)return {false,{},common_failure("LegacyControlConsentProfileRequired").error};auto valid=validate_family_legacy(c,family,deaths,read,*interaction.common,interaction,catalog,legacy,playable,disciple,biological,collateral,complete_people);if(!valid.ok)return {false,{},valid.error};const auto& actor=c.state().metadata.player_actor_id;const auto* current=c.actor(actor);if(!current)return {false,{},common_failure("UnknownActor").error};V::Array requests,consents;
    auto relation_row=[&](V row,const StableId& request){if(disciple&&disciple->catalog.enabled){if(const auto* basis=disciple->catalog.requests.find(request)){row.as<V::Dict>()->emplace_back("relation_basis",V{StableId{"disciple"}});row.as<V::Dict>()->emplace_back("relation_skill_id",V{basis->skill_id});}}return row;};
    auto operation=[&](const FamilyLegacyCommand& command){auto quote=preview_family_legacy_command(c,family,deaths,read,interaction_state,interaction,access,catalog,legacy,playable,actor,command,disciple,biological,collateral,complete_people);V::Dict params;if(!command.request_id.empty())params.emplace_back("request_id",V{command.request_id});if(!command.consent_fact_id.empty())params.emplace_back("consent_fact_id",V{command.consent_fact_id});return obj({{"command",V{command.kind}},{"params",V{std::move(params)}},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}});};
    if(current->alive)for(const auto& row:catalog.definitions.rows()){const auto& d=row.second;if(d.predecessor_actor_id!=actor)continue;auto source=preview_authored_node_source(c,interaction_state,interaction,access,actor,d.successor_actor_id);if(!source.ok||source.value.dialogue_id!=d.dialogue_id||source.value.node_id!=d.accept_node_id)continue;requests.push_back(relation_row(obj({{"request_id",V{d.id}},{"successor",person(c,*interaction.common,d.successor_actor_id)},{"action",operation({"family_legacy_accept",d.id,{}})}}),d.id));}
    for(const auto& row:legacy.consents.rows()){const auto& source=row.second.source;if(source.biological_origin||source.collateral_dialogue)continue;if(source.predecessor_actor_id!=actor&&source.successor_actor_id!=actor)continue;StableId status="accepted";for(const auto& r:legacy.revocations.rows())if(r.second.consent_fact_id==row.first)status="revoked";for(const auto& r:legacy.activations.rows())if(r.second.consent_fact_id==row.first)status="activated";V::Array operations;
        if(status=="accepted"&&source.predecessor_actor_id==actor){if(!current->alive)operations.push_back(operation({"family_legacy_activate",{},row.first}));else{auto proof=preview_authored_node_source(c,interaction_state,interaction,access,actor,source.successor_actor_id);const auto* d=catalog.definitions.find(source.request_id);if(proof.ok&&d&&proof.value.dialogue_id==d->dialogue_id&&proof.value.node_id==d->revoke_node_id)operations.push_back(operation({"family_legacy_revoke",source.request_id,row.first}));}}
        consents.push_back(relation_row(obj({{"consent_fact_id",V{row.first}},{"status",V{status}},{"accepted_tick",V{source.tick}},{"successor",person(c,*interaction.common,source.successor_actor_id)},{"actions",V{std::move(operations)}}}),source.request_id));
    }
    Integer request_count=requests.size(),consent_count=consents.size();auto slice=[page](V::Array& list){V::Array out;for(Integer i=page*16;i<std::min<Integer>(list.size(),page*16+16);++i)out.push_back(std::move(list[static_cast<std::size_t>(i)]));list=std::move(out);};slice(requests);slice(consents);
    return {true,obj({{"actor_id",V{actor}},{"source_identity",identity(c)},{"page",V{page}},{"request_count",V{request_count}},{"consent_count",V{consent_count}},{"requests",V{std::move(requests)}},{"consents",V{std::move(consents)}},{"awaiting_control_choice",V{!current->alive}},{"scope",V{StableId{"own_life_control_only"}}},{"assets_transferred",V{false}},{"knowledge_granted",V{false}},{"public_authority_transferred",V{false}}}),{}};
}
// 同一翻译单元必须只有头中声明的实际签名，防新尾参误成未定义旧入口的静默重载。
static_assert(std::is_pointer_v<decltype(&query_family_legacy_page)>,"legacy typed access signature must be unambiguous");

}
