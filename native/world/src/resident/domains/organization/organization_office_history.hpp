// 公职历史只沿原目录与真实任职事实重建签发时刻，不要求今日持有人仍是旧签发者。
#pragma once
#include "organization_model.hpp"
#include <charconv>
#include <algorithm>
#include <limits>
namespace hanmo::resident {
// 共同实例序号区分同tick内先签发后免职；非规范ID不能伪装历史时序。
inline std::optional<Integer> organization_instance_sequence(const StableId& id,const StableId& prefix){
    const auto start=prefix+":";if(id.rfind(start,0)!=0)return {};
    const auto digits=id.substr(start.size());Integer value=0;
    const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),value);
    if(parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||value<1||std::to_string(value)!=digits)return {};
    return value;
}
// 精确布局由已验证目录选择，旧事实不猜offset；原源语义另由唯一lifecycle validator复验。
inline Status validate_office_change_payload_shape(const CommonStore& common,const OrganizationCatalog& catalog,const OrganizationFact& fact,Integer before_epoch,const std::optional<StableId>& before_holder){
    if(!catalog.authority_lifecycle_enabled)return !fact.office_change_payload&&!fact.grant_change_payload?common_success():common_failure("UnexpectedOfficeChangePayload",fact.id);
    if(!fact.office_change_payload||fact.grant_change_payload)return common_failure("OfficeChangePayloadRequired",fact.id);const auto& p=*fact.office_change_payload;
    if(p.schema_version!=2||p.before_epoch!=before_epoch||before_epoch==std::numeric_limits<Integer>::max()||p.after_epoch!=before_epoch+1||p.old_holder!=before_holder||p.old_holder==p.new_holder||fact.actor_id!=p.new_holder.value_or(StableId{})||fact.tick!=p.effective_moment.half_day_tick)return common_failure("OfficeChangePayloadMismatch",fact.id);
    auto encoded=encode_world_moment(p.effective_moment);if(!encoded.ok)return {false,{},encoded.error};SourceRef source;
    if(const auto* transition=std::get_if<ByOfficeTransition>(&p.cause))source=transition->transition_source_ref;else {const auto& death=std::get<VerifiedHolderDeath>(p.cause).death_ref;if(death.domain!="family_life"||death.kind!="family_death"||!p.old_holder||p.new_holder)return common_failure("InvalidOfficeDeathCause",fact.id);source={death.domain,death.kind,death.id};}
    const auto* header=common.state().facts.find(source.id);const auto& order=common.state().facts.order();auto cause=std::find(order.begin(),order.end(),source.id),result=std::find(order.begin(),order.end(),fact.id);
    if(!header||header->domain!=source.domain||header->kind!=source.kind||fact.cause_fact_ids!=std::vector<StableId>{source.id}||cause==order.end()||result==order.end()||cause>=result)return common_failure("InvalidOfficeChangeCause",fact.id);return common_success();
}
// 该读门只证明具名任职和时序；调用域仍须核同一Grant、范围、资源及业务事实。
inline Status validate_historical_office_signer(const CommonStore& common,const OrganizationState& state,const OrganizationCatalog& catalog,const ActingAuthority& acting,Integer tick,Integer sequence){
    const auto fail=[&](){return common_failure("InvalidHistoricalOfficeAuthority",acting.office_id);};
    const auto* initial=catalog.offices.find(acting.office_id);const auto* current=state.offices.find(acting.office_id);
    if(!initial||!current||initial->organization_id!=acting.organization_id||current->organization_id!=acting.organization_id||tick<0||tick>common.tick()||sequence<1||sequence>=common.state().next_instance_id||!common.actor(acting.actor_id))return fail();
    Integer epoch=initial->epoch,prior_sequence=0,prior_tick=0;
    auto holder=initial->holder_actor_id;auto at_holder=holder;Integer at_epoch=epoch;
    for(const auto& row:state.facts.rows()){
        const auto& fact=row.second;if(fact.kind!="organization_office_changed"||fact.office_id!=acting.office_id)continue;
        const auto number=organization_instance_sequence(fact.id,"organization");const auto* header=common.state().facts.find(fact.id);
        if(row.first!=fact.id||!number||*number<=prior_sequence||*number>=common.state().next_instance_id||*number==sequence||fact.organization_id!=acting.organization_id||fact.tick<prior_tick||fact.tick>common.tick()||epoch==std::numeric_limits<Integer>::max()||!header||header->domain!="organization"||header->kind!=fact.kind)return fail();
        if((*number<sequence&&fact.tick>tick)||(*number>sequence&&fact.tick<tick))return fail();
        auto payload=validate_office_change_payload_shape(common,catalog,fact,epoch,holder);if(!payload.ok)return payload;
        ++epoch;holder=fact.actor_id.empty()?std::nullopt:std::optional<StableId>{fact.actor_id};
        if(*number<sequence){at_epoch=epoch;at_holder=holder;}
        prior_sequence=*number;prior_tick=fact.tick;
    }
    if(epoch!=current->epoch||holder!=current->holder_actor_id||at_epoch!=acting.authority_epoch||at_holder!=std::optional<StableId>{acting.actor_id})return fail();
    return common_success();
}
}
