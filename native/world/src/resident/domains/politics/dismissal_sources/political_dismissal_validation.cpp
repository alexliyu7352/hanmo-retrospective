#include "resident/kernel/catalog_section_observation.hpp"
// 原权限与控制/生命前界共同授权；实际Office事后变化不能替旧签字补证。
#include "political_dismissal_rules.hpp"
#include "resident/domains/politics/political_member_history.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {namespace {
// 下游原权源读需要的辖区接口仅回答已逐首府/目标辖区验证的本次撤职，不能任意查询。
class ProvenTerritory final:public PoliticalTerritoryRead {
 const PoliticalDismissalContext& x_;const PoliticalDismissalSourcePolicy& policy_;const PoliticalDismissalSource& source_;
public:
 ProvenTerritory(const PoliticalDismissalContext& x,const PoliticalDismissalSourcePolicy& p,const PoliticalDismissalSource& s):x_(x),policy_(p),source_(s){}
 Outcome<std::optional<PoliticalTerritoryControl>> jurisdiction_control(const StableId&)const override{return {false,{},common_failure("PoliticalDismissalCurrentControlReadUnsupported").error};}
 Outcome<Integer> jurisdiction_revision_before_fact(const StableId& j,const StableId& fact,Integer tick)const override {if(j!=policy_.jurisdiction_id||fact!=source_.dismissal_fact_id||tick!=source_.tick||!x_.seats)return {false,0,common_failure("PoliticalDismissalTerritoryBoundaryMismatch",fact).error};auto valid=x_.seats(policy_,tick,fact);return valid.ok?Outcome<Integer>{true,0,{}}:Outcome<Integer>{false,0,valid.error};}
};
// 当时生死依原真实原因；额外核分配序/Common序同侧，后来才登记或重排不能恢复签字资格。
Status signer_before(const PoliticalDismissalContext& x,const PoliticalDismissalSource& s){auto present=read_actor_presence_before_fact(x.common,x.common_catalog,x.death_origin,s.authority.actor_id,s.tick,s.dismissal_fact_id);if(!present.ok)return {false,{},present.error};const auto* actor=x.common.state().actors.find(s.authority.actor_id);if(!present.value.alive||!actor||!actor->birth_tick||*actor->birth_tick< -2147483647||s.tick>2147483647||*actor->birth_tick>s.tick||s.tick-*actor->birth_tick<18*720)return common_failure("PoliticalDismissalHistoricalAdultRequired",s.dismissal_fact_id);
 const auto cutoff=political_dismissal_fact_sequence(x.common,s.dismissal_fact_id);const auto& order=x.common.state().facts.order();const auto boundary=std::find(order.begin(),order.end(),s.dismissal_fact_id);if(!cutoff||boundary==order.end())return common_failure("PoliticalDismissalLifeBoundaryRequired",s.dismissal_fact_id);
 auto cross=[&](const StableId& id,const StableId& prefix,Integer tick)->Status {const auto number=organization_instance_sequence(id,prefix);auto at=std::find(order.begin(),order.end(),id);if(!number||*number==*cutoff||*number>=x.common.state().next_instance_id||at==order.end())return common_failure("PoliticalDismissalLifeSourceMismatch",id);const bool earlier=*number<*cutoff;if(earlier!=(at<boundary)||(earlier&&tick>s.tick)||(!earlier&&tick<s.tick))return common_failure("PoliticalDismissalLifeOrderMismatch",id);return common_success();};
 if(!present.value.creation_fact_id.empty()){auto valid=cross(present.value.creation_fact_id,"actor",present.value.created_tick);if(!valid.ok)return valid;}
 if(x.death_origin){auto source=x.death_origin(s.authority.actor_id);if(!source.ok)return {false,{},source.error};if(source.value){auto valid=cross(source.value->casualty_fact_id,"military",source.value->tick);if(!valid.ok)return valid;}}
 return validate_political_historical_member_duty(x.common,x.organizations,x.organization_catalog,s.authority,s.tick,s.dismissal_fact_id,PoliticalMemberHistoryUse::political_dismissal);
}
}
Status validate_political_dismissal_source_catalog(const PoliticsCatalog& pc,const OrganizationCatalog& oc,const PoliticalDismissalSourceCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 95, "PoliticalDismissalSourceCatalog", "validate_political_dismissal_source_catalog", &catalog);
if(catalog.version!=1||catalog.policies.empty()||catalog.policies.size()>4096)return common_failure("InvalidPoliticalDismissalSourceCatalog");std::set<StableId> jurisdictions;for(const auto& row:catalog.policies.rows()){const auto& p=row.second;if(row.first!=p.id||!jurisdictions.insert(p.jurisdiction_id).second)return common_failure("PoliticalDismissalDuplicateJurisdiction",row.first);auto valid=validate_political_dismissal_source_policy(pc,oc,p);if(!valid.ok)return valid;}return common_success();}
Status validate_political_dismissal_sources(const PoliticalDismissalContext& x){auto valid=validate_political_dismissal_source_catalog(x.politics_catalog,x.organization_catalog,x.catalog);if(!valid.ok)return valid;valid=validate_political_dismissal_state_layout(x.state);if(!valid.ok)return valid;if(!x.player_before||!x.seats)return common_failure("PoliticalDismissalHistoricalReadersRequired");Integer prior=0,prior_tick=-1;std::size_t prior_position=0;std::set<StableId> changes;const auto& order=x.common.state().facts.order();
 for(const auto& row:x.state.dismissals.rows()){const auto& s=row.second.source;const auto* p=x.catalog.policies.find(s.policy_id);auto n=political_dismissal_fact_sequence(x.common,row.first);auto at=std::find(order.begin(),order.end(),row.first);if(row.first!=s.dismissal_fact_id||!p||!n||*n<=prior||s.tick<prior_tick||at==order.end()||(prior&&static_cast<std::size_t>(at-order.begin())<=prior_position)||!changes.insert(s.office_change_fact_id).second)return common_failure("PoliticalDismissalSourceOrderMismatch",row.first);
  ProvenTerritory territory(x,*p,s);auto original=read_political_dismissal_provenance(x.common,x.politics,x.politics_catalog,x.organizations,x.organization_catalog,*p,s,&territory);if(!original.ok)return {false,{},original.error};auto player=x.player_before(s.tick,row.first);if(!player.ok)return {false,{},player.error};if(player.value!=s.authority.actor_id)return common_failure("PoliticalDismissalHistoricalPlayerMismatch",row.first);valid=signer_before(x,s);if(!valid.ok)return valid;prior=*n;prior_tick=s.tick;prior_position=static_cast<std::size_t>(at-order.begin());
 }
 // 此显式profile的每次原撤职都必须有来源；删sidecar或借未配置辖区退回旧writer都拒。
 for(const auto& row:x.politics.facts.rows())if(row.second.kind=="political_office_dismissed"&&!x.state.dismissals.find(row.first))return common_failure("PoliticalDismissalSourceRequired",row.first);
 for(const auto& row:x.common.state().facts.rows())if(row.second.kind=="political_office_dismissed"&&(row.second.domain!="politics"||!x.state.dismissals.find(row.first)))return common_failure("PoliticalDismissalSourceRequired",row.first);
 return common_success();
}
Status validate_political_dismissal_sources_complete(const PoliticalDismissalContext& x){auto valid=validate_political_dismissal_sources(x);return valid.ok?validate_political_dismissal_results(x.common,x.state,x.result_matches):valid;}
}
