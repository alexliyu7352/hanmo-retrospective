// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "estate_rest_history.hpp"
#include "estate_title_history.hpp"
#include "resident/domains/actor_foundation/authored_origin.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const StableId& code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}
bool same(const SourceRef&a,const SourceRef&b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
bool has(const std::vector<StableId>& rows,const StableId&id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
const EstateDefinition& definition(const EstateRestHistoryContext&x,const StableId&id){const auto*d=x.estates.estates.find(id);if(!d||d->id!=id||d->owner_actor_id.empty()||d->inventory_id.empty()||d->place_id.empty()||d->food_item_id.empty()||d->ration_per_person<0)fail("EstateRestInitialDefinitionRequired",id);return *d;}
const ScenarioOriginRef& actor_origin(const EstateRestHistoryContext&x,const StableId&id){
 const auto*d=x.foundation_catalog.actors.find(id);if(!d)fail("EstateRestInitialActorSourceRequired",id);
 const auto&origin=d->scenario_origin_ref;const auto&m=x.common.state().metadata;
 if(origin.initial_tick<0||origin.catalog_digest!=m.content_digest||origin.catalog_digest!=x.foundation_catalog.identity.content_digest||origin.scenario_id!=m.scenario_id||origin.scenario_id!=x.foundation_catalog.scenario_id)fail("EstateRestScenarioMismatch",id);
 bool retained=false;for(const auto&r:x.foundation.actor_start_context.scenario_origins)if(actor_foundation_origin_equal(r.ref,origin))retained=true;
 const ActorConstructionReceipt* adopted=nullptr;for(const auto&r:x.foundation.actor_start_context.construction_receipts.rows())if(actor_foundation_origin_equal(r.second.scenario_origin_ref,origin)&&has(r.second.constructed_actor_ids,id)){if(adopted)fail("DuplicateEstateRestInitialActor",id);adopted=&r.second;}
 if(!retained||!adopted||adopted->initialization_result_ref.empty())fail("EstateRestScenarioAdoptionRequired",id);return origin;
}
std::size_t source_position(const CommonStore&c,const SourceRef&ref){std::size_t n=0;for(const auto&r:c.state().facts.rows()){if(r.first==ref.id){if(r.second.id!=ref.id||r.second.domain!=ref.domain||r.second.kind!=ref.kind)fail("EstateRestTitleHeaderMismatch",ref.id);return n;}++n;}fail("EstateRestTitleSourceMissing",ref.id);}
void prefix(const EstateRestHistoryContext&x,const EstateRestAdoptionBasis&b){
 if(b.source_schema!=1||b.common_fact_count<0||static_cast<std::uint64_t>(b.common_fact_count)>x.common.state().facts.size())fail("InvalidEstateRestAdoptionPrefix",b.estate_id);
 if(b.common_fact_count==0){if(b.last_common_fact)fail("InvalidEstateRestAdoptionPrefix",b.estate_id);}
 else {if(!b.last_common_fact||source_position(x.common,*b.last_common_fact)!=static_cast<std::size_t>(b.common_fact_count-1))fail("EstateRestAdoptionPrefixMismatch",b.estate_id);}
 if(take(compare_world_moments(b.adopted_at,{b.initial_origin.initial_tick,{}}))<0||take(compare_world_moments(b.adopted_at,take(x.clock.read_current_moment())))>0)fail("EstateRestAdoptionMomentMismatch",b.estate_id);
}
// S4 owns and validates consent/identity. Estate replays only its original, actual
// asset-disposition rows. Neither a designation nor a control-only successor is title.
void apply_succession(const EstateRestHistoryContext&x,const EstateDefinition&d,const StableId&id,const FamilyChoice&f,std::size_t position,StableId&owner,std::vector<SourceRef>&sources){
 if(f.kind!="family_succession"||!f.assets||!f.designation_source||f.tick<0||f.guardian_actor_id.empty()||f.heir_actor_id.empty()||f.guardian_actor_id==f.heir_actor_id||!has(x.family.successions,id))fail("EstateRestSuccessionSourceMismatch",id);
 const auto*row=x.family.facts.find(*f.designation_source);const auto*choice=row?std::get_if<FamilyChoice>(row):nullptr;
 if(!choice||choice->kind!="family_designation"||choice->guardian_actor_id!=f.guardian_actor_id||choice->heir_actor_id!=f.heir_actor_id||choice->adoption_source!=f.adoption_source||choice->tick>f.tick||source_position(x.common,{"family","family_designation",*f.designation_source})>=position)fail("EstateRestSuccessionConsentMismatch",id);
 bool applied=false;for(const auto&e:f.assets->estates)if(e.estate_id==d.id){if(applied||e.inventory_id!=d.inventory_id||e.wallet_id!=d.wallet_id||owner!=f.guardian_actor_id)fail("EstateRestTitleApplicationMismatch",id);applied=true;}
 // All original assets of this guardian were included by the original S4 writer.
 if((owner==f.guardian_actor_id)!=applied)fail("EstateRestTitleApplicationCoverage",id);
 if(applied){owner=f.heir_actor_id;sources.push_back({"family","family_succession",id});}
}
}
Outcome<EstateRestHistoricalRead> read_estate_rest_at_adoption(const EstateRestHistoryContext&x,const EstateRestAdoptionBasis&b){try{
 prefix(x,b);const auto&d=definition(x,b.estate_id);const auto&origin=actor_origin(x,d.owner_actor_id);if(!actor_foundation_origin_equal(origin,b.initial_origin))fail("EstateRestInitialOriginMismatch",b.estate_id);
 for(std::size_t i=0;i<d.resident_actor_ids.size();++i){const auto&id=d.resident_actor_ids[i];if(id.empty()||std::find(d.resident_actor_ids.begin(),d.resident_actor_ids.begin()+static_cast<std::ptrdiff_t>(i),id)!=d.resident_actor_ids.begin()+static_cast<std::ptrdiff_t>(i))fail("EstateRestInitialResidentMismatch",id);const auto&r=actor_origin(x,id);if(r.catalog_digest!=origin.catalog_digest||r.scenario_id!=origin.scenario_id||r.initial_tick!=origin.initial_tick)fail("EstateRestResidentScenarioMismatch",id);}
 EstateRestHistoricalRead out;out.view={d.owner_actor_id,d.place_id,d.inventory_id,d.food_item_id,d.ration_per_person,d.resident_actor_ids};std::size_t n=0;
 for(const auto&h:x.common.state().facts.rows()){
  if(n>=static_cast<std::size_t>(b.common_fact_count))break;
  if(h.second.domain=="family"&&h.second.kind=="family_succession"){
   if(h.second.id!=h.first)fail("EstateRestTitleHeaderMismatch",h.first);const auto*value=x.family.facts.find(h.first);const auto*f=value?std::get_if<FamilyChoice>(value):nullptr;if(!f||f->tick>b.adopted_at.half_day_tick)fail("EstateRestTitlePayloadMissing",h.first);apply_succession(x,d,h.first,*f,n,out.view.owner_actor_id,out.title_sources);
  }else if(h.second.domain==estate_employment_module&&h.second.kind=="estate_employment_decided"){
   if(!x.employment)fail("EstateRestEmploymentSourceRequired",h.first);
   const auto*decision=x.employment->decisions.find(h.first);const auto*contract=decision?x.employment->contracts.find(decision->contract_id):nullptr;
   if(!decision||decision->id!=h.first||h.second.id!=h.first||!contract||contract->id!=decision->contract_id||decision->tick>b.adopted_at.half_day_tick)fail("EstateRestEmploymentSourceMismatch",h.first);
   if(decision->decision=="accept"&&contract->terms.estate_id==d.id){
    if(contract->acceptance_fact_id!=decision->id||contract->accepted_tick!=decision->tick||decision->actor_id!=contract->terms.worker_actor_id||!decision->meeting||decision->tick<contract->offered_tick||decision->tick>=contract->terms.offer_expires_tick||contract->terms.payer_wallet_id!=d.wallet_id||contract->terms.employer_actor_id!=out.view.owner_actor_id||source_position(x.common,{estate_employment_module,"estate_employment_offered",contract->id})>=n)fail("EstateRestResidenceAcceptanceMismatch",h.first);
    if(!has(out.view.resident_actor_ids,contract->terms.worker_actor_id))out.view.resident_actor_ids.push_back(contract->terms.worker_actor_id);
    out.residence_sources.push_back({estate_employment_module,"estate_employment_decided",decision->id});
   }
  }++n;
 }
 // Reverse coverage catches deleting a source header while retaining its S4 row.
 for(const auto&id:x.family.successions){const auto*value=x.family.facts.find(id);if(const auto*f=value?std::get_if<FamilyChoice>(value):nullptr)if(f->kind=="family_succession")source_position(x.common,{"family","family_succession",id});}
 if(x.employment)for(const auto&row:x.employment->contracts.rows())if(row.second.terms.estate_id==d.id&&!row.second.acceptance_fact_id.empty()){const auto position=source_position(x.common,{estate_employment_module,"estate_employment_decided",row.second.acceptance_fact_id});if(position<static_cast<std::size_t>(b.common_fact_count)){const auto found=std::find_if(out.residence_sources.begin(),out.residence_sources.end(),[&](const auto&ref){return ref.id==row.second.acceptance_fact_id;});if(found==out.residence_sources.end())fail("EstateRestResidenceSourceCoverage",row.first);}}
 return {true,std::move(out),{}};
 }catch(const Failure&f){return {false,{},f.error};}}
Outcome<EstateRestAdoptionBasis> capture_estate_rest_adoption(const EstateRestHistoryContext&x,const EstateState&state,const StableId&id){try{
 const auto&d=definition(x,id);const auto*current=state.estates.find(id);if(!current||current->id!=id)fail("UnknownEstate",id);EstateRestAdoptionBasis b;b.estate_id=id;b.initial_origin=actor_origin(x,d.owner_actor_id);b.adopted_at=take(x.clock.read_current_moment());
 if(x.common.state().facts.size()>static_cast<std::uint64_t>(std::numeric_limits<Integer>::max()))fail("EstateRestAdoptionPrefixOverflow",id);b.common_fact_count=static_cast<Integer>(x.common.state().facts.size());if(b.common_fact_count){const auto&h=x.common.state().facts.rows().back().second;b.last_common_fact=SourceRef{h.domain,h.kind,h.id};}
 auto historical=take(read_estate_rest_at_adoption(x,b));if(historical.view.owner_actor_id!=current->owner_actor_id||historical.view.resident_actor_ids!=current->resident_actor_ids)fail("EstateRestOriginalTitleOrResidenceSourceRequired",id);
 const auto*inventory=x.common.inventory(d.inventory_id);const auto*wallet=x.common.wallet(d.wallet_id);const Owner owner{Owner::Kind::actor,current->owner_actor_id,{}};if(!inventory||!wallet||!owner_equal(inventory->owner,owner)||!owner_equal(wallet->owner,owner))fail("EstateRestCurrentAssetOwnerMismatch",id);
 return {true,std::move(b),{}};
 }catch(const Failure&f){return {false,{},f.error};}}
Status validate_estate_rest_adopted_view(const EstateRestHistoryContext&x,const EstateRestAdoptionBasis&b,const RestEstateView&view){auto actual=read_estate_rest_at_adoption(x,b);if(!actual.ok)return {false,{},actual.error};const auto&v=actual.value.view;if(view.owner_actor_id!=v.owner_actor_id||view.place_id!=v.place_id||view.inventory_id!=v.inventory_id||view.food_item_id!=v.food_item_id||view.ration_per_person!=v.ration_per_person||view.resident_actor_ids!=v.resident_actor_ids)return common_failure("EstateRestFrozenViewMismatch",b.estate_id);return common_success();}
namespace {
EstateAssetTitleView title_at_prefix(const EstateRestHistoryContext&x,const StableId&id,std::size_t through){
 const auto&d=definition(x,id);EstateAssetTitleView out{id,d.inventory_id,d.wallet_id,{Owner::Kind::actor,d.owner_actor_id,{}},actor_origin(x,d.owner_actor_id),{}};std::size_t n=0;
 for(const auto&h:x.common.state().facts.rows()){if(n>=through)break;if(h.second.domain=="family"&&h.second.kind=="family_succession"){if(h.second.id!=h.first)fail("EstateTitleSourceHeaderMismatch",h.first);const auto*value=x.family.facts.find(h.first);const auto*f=value?std::get_if<FamilyChoice>(value):nullptr;if(!f)fail("EstateTitleApplicationMissing",h.first);apply_succession(x,d,h.first,*f,n,out.legal_owner.id,out.original_title_applications);}++n;}
 for(const auto&source:x.family.successions){const auto*value=x.family.facts.find(source);if(const auto*f=value?std::get_if<FamilyChoice>(value):nullptr)if(f->kind=="family_succession")source_position(x.common,{"family","family_succession",source});}
 return out;
}
}
Outcome<EstateAssetTitleView> NativeEstateAssetTitleRead::read_estate_title_before_source(const StableId&id,const SourceRef&source)const{try{return {true,title_at_prefix(x_,id,source_position(x_.common,source)),{}};}catch(const Failure&f){return {false,{},f.error};}}
Outcome<EstateAssetTitleView> NativeEstateAssetTitleRead::read_current_estate_title(const StableId&id)const{try{auto out=title_at_prefix(x_,id,x_.common.state().facts.size());const auto*estate=state_.estates.find(id);const auto*wallet=x_.common.wallet(out.wallet_id);const auto*inventory=x_.common.inventory(out.inventory_id);if(!estate||estate->owner_actor_id!=out.legal_owner.id||!wallet||!inventory||!owner_equal(wallet->owner,out.legal_owner)||!owner_equal(inventory->owner,out.legal_owner))fail("EstateTitleCurrentSourceMismatch",id);return {true,std::move(out),{}};}catch(const Failure&f){return {false,{},f.error};}}
Outcome<std::vector<SourceRef>> NativeEstateAssetTitleRead::read_estate_title_applications_after_source(const StableId&id,const SourceRef&source)const{try{
 const auto cut=source_position(x_.common,source);auto actual=take(read_current_estate_title(id));std::vector<SourceRef> out;for(const auto&application:actual.original_title_applications)if(source_position(x_.common,application)>cut)out.push_back(application);return {true,std::move(out),{}};
 }catch(const Failure&f){return {false,{},f.error};}}
Outcome<std::vector<SourceRef>> NativeEstateAssetTitleRead::read_estate_title_applications_after_adoption(const EstateRestAdoptionBasis&basis)const{try{
 // The period owner must resolve this exact basis from its original source first.
 take(read_estate_rest_at_adoption(x_,basis));auto actual=take(read_current_estate_title(basis.estate_id));std::vector<SourceRef> out;for(const auto&application:actual.original_title_applications)if(source_position(x_.common,application)>=static_cast<std::size_t>(basis.common_fact_count))out.push_back(application);return {true,std::move(out),{}};
 }catch(const Failure&f){return {false,{},f.error};}}

}
