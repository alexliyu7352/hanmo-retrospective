// 接续页面读取明确本人接受及真实待办；所有按钮仍走原共同命令，不由UI换控制人物。
#include "voluntary_succession.hpp"
#include "resident/views/common_presentation.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
using V=BoundaryValue;
// 全页绑定当前world/人物/代次/版本，旧页面不能在切人或冷恢复后继续提交。
V source_identity(const CommonStore& common){using namespace boundary;StableId world;const char* hex="0123456789abcdef";for(const auto b:common.state().identity.world_id){world+=hex[b>>4];world+=hex[b&15];}const auto& id=common.state().identity;return obj({{"world_id",V{world}},{"execution_epoch",V{id.execution_epoch}},{"revision",V{id.revision}},{"tick",V{id.tick}}});}
// 实际年龄与伤病来自原人物，不把前代属性投影到继任者。
V person(const CommonStore& common,const CommonCatalog& catalog,const StableId& id){using namespace boundary;const auto* actor=common.state().actors.find(id);if(!actor)return obj({});CommonPresentation names(common.state(),catalog);auto name=names.display_name(id);auto place=common.resolved_location(id);const auto* at=place.ok?std::get_if<SettlementLocation>(&place.value):nullptr;const auto* action=common.action(actor->action_id);return obj({{"actor_id",V{id}},{"name",V{name.ok?name.value:id}},{"age_years",V{actor->birth_tick&&*actor->birth_tick>=-2147483647&&*actor->birth_tick<=common.tick()?(common.tick()-*actor->birth_tick)/720:Integer{-1}}},{"hp",V{actor->hp}},{"fatigue",V{actor->fatigue}},{"injury",V{actor->injury}},{"place_id",V{at?at->place_id:StableId{}}},{"action_kind",V{action?action->kind:StableId{}}}});}
// 只返回命令自己的参数与当前具名拒因，不把完整报价或账户放进任意UI动作。
V action(const CommonStore& common,const FamilyState& family,const FamilyCatalog& catalog,const CommonCatalog& shared,const EstateState& estates,const EstateCatalog& estate_catalog,const FamilyAccess& access,const FamilyCommand& command,const V& params){using namespace boundary;const auto quote=preview_family(common,family,catalog,shared,estates,estate_catalog,access,common.state().metadata.player_actor_id,command);return obj({{"command",V{command.kind}},{"params",params},{"available",V{quote.ok}},{"code",V{quote.ok?StableId{}:quote.error.original.code}}});}
}
// 分页固定16项；未知会话节点的接受定义不泄漏到全族名单。
Outcome<BoundaryValue> query_voluntary_succession_page(const CommonStore& common,const FamilyState& family,const FamilyCatalog& catalog,const CommonCatalog& shared,const EstateState& estates,const EstateCatalog& estate_catalog,const FamilyAccess& access,Integer page){
    using namespace boundary;if(page<0||page>255)return {false,{},common_failure("InvalidSuccessionPage").error};if(!catalog.voluntary_succession||!catalog.voluntary_succession->enabled||!access.interaction||!access.interaction_catalog||!access.interaction_access)return {false,{},common_failure("VoluntarySuccessionCapabilityRequired").error};auto valid=validate_voluntary_succession_facts(common,family,shared,access.interaction_catalog,&*catalog.voluntary_succession,access.legacy);if(!valid.ok)return {false,{},valid.error};const auto& actor=common.state().metadata.player_actor_id;V::Array requests,plans;
    for(const auto& row:catalog.voluntary_succession->definitions.rows()){
        const auto& d=row.second;if(d.predecessor_actor_id!=actor)continue;auto proof=preview_authored_node_source(common,*access.interaction,*access.interaction_catalog,*access.interaction_access,actor,d.successor_actor_id);if(!proof.ok||proof.value.dialogue_id!=d.dialogue_id||proof.value.node_id!=d.node_id)continue;
        FamilyCommand command;command.kind="family_agree_succession";command.request_id=d.id;command.heir_actor_id=d.successor_actor_id;auto operation=action(common,family,catalog,shared,estates,estate_catalog,access,command,obj({{"request_id",V{d.id}},{"heir_actor_id",V{d.successor_actor_id}}}));requests.push_back(obj({{"request_id",V{d.id}},{"successor",person(common,shared,d.successor_actor_id)},{"personal",encode_succession_personal_selection(d.personal)},{"accept_action",std::move(operation)}}));
    }
    for(const auto& row:family.facts.rows())if(const auto* plan=std::get_if<SuccessionConsentFact>(&row.second)){
        const auto& source=plan->agreement;if(source.personal.predecessor_actor_id!=actor&&source.personal.successor_actor_id!=actor)continue;const auto state=plan->conclusion?plan->conclusion->kind:StableId{"accepted"};V::Array actions;
        if(!plan->conclusion&&source.personal.predecessor_actor_id==actor){for(const auto* kind:{"family_continue_as","family_cancel_succession"}){FamilyCommand command;command.kind=kind;command.plan_id=row.first;actions.push_back(action(common,family,catalog,shared,estates,estate_catalog,access,command,obj({{"plan_id",V{row.first}}})));}}
        plans.push_back(obj({{"plan_id",V{row.first}},{"state",V{state}},{"accepted_tick",V{source.accepted_tick}},{"predecessor",person(common,shared,source.personal.predecessor_actor_id)},{"successor",person(common,shared,source.personal.successor_actor_id)},{"personal",encode_succession_personal_terms(source.personal)},{"actions",V{std::move(actions)}}}));
    }
    const auto request_count=Integer(requests.size()),plan_count=Integer(plans.size());auto slice=[page](V::Array& rows){V::Array part;for(auto i=page*16;i<std::min(Integer(rows.size()),page*16+16);++i)part.push_back(std::move(rows[std::size_t(i)]));rows=std::move(part);};slice(requests);slice(plans);return {true,obj({{"actor_id",V{actor}},{"source_identity",source_identity(common)},{"page",V{page}},{"request_count",V{request_count}},{"plan_count",V{plan_count}},{"requests",V{std::move(requests)}},{"plans",V{std::move(plans)}}}),{}};
}
}
