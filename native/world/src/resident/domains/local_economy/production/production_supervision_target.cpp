// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "production_supervision_target.hpp"
#include "production_capacity_input.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{v.error};return std::move(v.value);}
void take(Status v){if(!v.ok)throw Failure{v.error};}
int cmp(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
bool same_owner(const Owner&a,const Owner&b){return a.kind==b.kind&&a.id==b.id;}
void append(std::vector<MomentOwnerSource>& out,const MomentOwnerSource& source){
 if(!valid_source_ref(source.source_ref)||source.source_schema<=0)fail("ProductionSupervisionSourceShape");
 for(const auto& old:out)if(same_source_ref(old.source_ref,source.source_ref)){
  if(old.source_schema!=source.source_schema)fail("ProductionSupervisionSourceSchemaMismatch",source.source_ref.id);return;
 }
 out.push_back(source);
}
void append(std::vector<MomentOwnerSource>& out,const std::vector<MomentOwnerSource>& sources){for(const auto& s:sources)append(out,s);}
struct OriginalTarget {
 const ProductionUnit* unit=nullptr;const ProductionCapacityPlan* plan=nullptr;
 std::vector<const ProductionAsset*> assets;MovementAnchor anchor;
};
OriginalTarget original(const ProductionSupervisionTargetContext& x,const GovernanceSupervisionTargetRequest& r){
 const auto* target=std::get_if<GovernanceAgronomy>(&r.specification.target);
 if(!target){if(std::holds_alternative<GovernanceIndustrySupport>(r.specification.target))fail("IndustryOriginalFinancingTargetReadRequired",r.approval.id);
  if(std::holds_alternative<GovernanceFacilityBuild>(r.specification.target))fail("ConstructionOriginalStageTargetReadRequired",r.approval.id);
  fail("UnsupportedProductionSupervisionTarget",r.approval.id);}
 const auto* approved=std::get_if<GovernanceAgronomy>(&r.approval.target);
 if(!approved||approved->production_unit_id!=target->production_unit_id||approved->improvement_policy_id!=target->improvement_policy_id||
    r.approval.spec_ref.id!=r.specification.spec_ref.id||r.approval.spec_ref.version!=r.specification.spec_ref.version||
    r.approval.spec_ref.catalog_digest!=r.specification.spec_ref.catalog_digest||r.specification.spec_ref.catalog_digest!=x.common.state().metadata.content_digest)
  fail("ProductionSupervisionApprovalTargetMismatch",r.approval.id);
 const auto& work=r.accepted_plan.economic_work_ref;
 if(r.accepted_plan.kind!=WorkKind::agronomy_supervision||work.owner_domain!="local_economy"||work.kind!=EconomicWorkKind::agronomy_operation||work.work_id!=target->production_unit_id)
  fail("ProductionSupervisionWorkTargetMismatch",work.work_id);
 const auto* profile=x.work_catalog.production_profiles.find(work.work_id);
 const auto* actual=x.economy.production_units.find(work.work_id);const auto* unit=x.catalog.production_units.find(work.work_id);
 if(!x.catalog.enabled||!profile||profile->unit_id!=work.work_id||profile->work_kind!=work.kind||profile->rule_version<=0||!actual||!unit||unit->id!=work.work_id||actual->id!=unit->id||
    actual->site_id!=unit->site_id||!same_owner(actual->owner,unit->owner)||actual->recipe_id!=unit->recipe_id||actual->capacity!=unit->capacity||
    actual->input_inventory_ids!=unit->input_inventory_ids||actual->output_inventory_id!=unit->output_inventory_id||!x.catalog.recipes.find(unit->recipe_id))
  fail("ProductionSupervisionOriginalUnitMismatch",work.work_id);
 // This is the original public production branch. Private/household operation
 // requires its own real mandate/consent source, which an Office cannot supply.
 if(unit->owner.kind!=Owner::Kind::organization)fail("PrivateProductionSupervisionMandateRequired",unit->id);
 if(r.specification.scope.kind!=ScopeRef::Kind::site||r.specification.scope.id!=unit->site_id)
  fail("ProductionSupervisionTargetScopeMismatch",unit->id);
 if(!x.catalog.production_management||x.catalog.production_management->version!=1)fail("ProductionSupervisionOriginalAssetPlanRequired",unit->id);
 const auto& management=*x.catalog.production_management;const auto* plan=management.plans.find(unit->id);
 if(!plan||plan->unit_id!=unit->id||plan->asset_uses.empty())fail("ProductionSupervisionOriginalAssetPlanRequired",unit->id);
 OriginalTarget out;out.unit=unit;out.plan=plan;
 auto point=take(read_original_spatial_site_anchor(x.shared,x.travel,r.accepted_plan.target_site_or_anchor_ref));
 auto original_point=take(x.spatial.original_site_anchor(unit->site_id));
 if(!take(same_spatial_anchor(point,original_point)))fail("ProductionSupervisionTargetPlaceMismatch",unit->id);out.anchor=std::move(point);
 bool land=false;
 for(const auto& use:plan->asset_uses){const auto* asset=management.assets.find(use.asset_id);
  if(!asset||asset->id!=use.asset_id||asset->site_id!=unit->site_id||asset->source_key.empty()||use.units<=0||use.batches_per_unit<=0||asset->capacity_units<use.units||
     std::any_of(out.assets.begin(),out.assets.end(),[&](const auto* a){return a->id==asset->id;})||!same_owner(asset->owner,unit->owner))fail("ProductionSupervisionOriginalAssetMismatch",use.asset_id);
  land|=asset->kind==ProductionAssetKind::land;out.assets.push_back(asset);
 }
 if(!land)fail("AgronomyOriginalLandRequired",unit->id);
 // Containers remain the original input/output identities. Their present Owner
 // is never used to rewrite the approved original responsibility.
 std::vector<StableId> containers=unit->input_inventory_ids;containers.push_back(unit->output_inventory_id);
 for(const auto& id:containers){const auto* initial=x.shared.required_inventories.find(id);if(!initial||!same_owner(initial->owner,unit->owner)||!x.common.inventory(id))fail("ProductionSupervisionOriginalContainerMissing",id);}
 if(cmp(take(x.clock.read_current_moment()),take(x.spatial.current_moment()))||&x.conditions.facilities.common!=&x.common||&x.conditions.clock!=&x.clock||&x.conditions.facilities.production_assets!=&management)
  fail("ProductionSupervisionSnapshotMismatch",unit->id);
 return out;
}
PoliticalResponsibilityRef unit_ref(const OriginalTarget& o){return {PoliticalResponsibilityKind::production_unit,o.unit->id,{},{}};}
PoliticalResponsibilityRef asset_ref(const OriginalTarget& o,const ProductionAsset& a){return {PoliticalResponsibilityKind::production_asset_use,a.id,o.unit->id,a.id};}
void responsibility(GovernanceSupervisionTargetView& out,const PoliticalResponsibilityMomentRead& read,
 const Owner& original,const PoliticalResponsibilityRef& ref,const std::vector<StableId>& assets,const StableId& organization){
 append(out.actual_sources,read.actual_sources);auto owner=take(read_production_responsibility_execution_owner(read,original,ref,assets));
 if(owner&&owner->kind==Owner::Kind::organization&&owner->id==organization)return;
 if(read.actual_sources.empty())fail("ProductionSupervisionOriginalOperatingAuthorityRequired",ref.responsibility_id);
 if(!out.actual_blocking_source)out.actual_blocking_source=read.actual_sources.back();
}
std::vector<StableId> asset_ids(const OriginalTarget&o){std::vector<StableId> out;for(const auto* a:o.assets)out.push_back(a->id);return out;}
Integer total_capacity(const OriginalTarget& o,const std::vector<Integer>& usable){
 Integer land=0,facility=0;bool has_land=false,has_facility=false;
 for(std::size_t i=0;i<o.assets.size();++i){const auto& use=o.plan->asset_uses[i];Integer batches=0;
  if(usable[i]<0||__builtin_mul_overflow(std::min(use.units,usable[i]),use.batches_per_unit,&batches))fail("ProductionSupervisionCapacityOverflow",use.asset_id);
  auto& sum=o.assets[i]->kind==ProductionAssetKind::land?land:facility;if(__builtin_add_overflow(sum,batches,&sum))fail("ProductionSupervisionCapacityOverflow",use.asset_id);
  if(o.assets[i]->kind==ProductionAssetKind::land)has_land=true;else has_facility=true;
 }
 Integer result=o.unit->capacity;if(has_land)result=std::min(result,land);if(has_facility)result=std::min(result,facility);return result;
}
void unavailable(GovernanceSupervisionTargetView& out,const std::vector<SourceRef>& conditions){
 for(const auto& source:conditions)if(source.kind=="facility_condition_change"){if(!out.actual_blocking_source)out.actual_blocking_source=MomentOwnerSource{source,1};return;}
 fail("ProductionSupervisionInitialCapacityUnavailable");
}
}
Outcome<GovernanceSupervisionTargetView> NativeProductionSupervisionTargetRead::read_supervision_target_at_start(
 const GovernanceSupervisionTargetRequest& r,const WorldMoment& at,const SourceRef* cut)const{try{
 const auto o=original(x_,r);const auto now=take(x_.clock.read_current_moment());if(cmp(at,now)>0||(!cut&&cmp(at,now)))fail("ProductionSupervisionStartClockMismatch",o.unit->id);
 GovernanceSupervisionTargetView out;out.original_target_anchor=o.anchor;append(out.actual_sources,{r.accepted_plan.target_site_or_anchor_ref,1});
 const auto all=asset_ids(o);responsibility(out,take(x_.takeovers.read_before_supervision_start(unit_ref(o),at,cut)),o.unit->owner,unit_ref(o),all,r.approval.issuer.organization_id);
 std::vector<Integer> usable;std::vector<SourceRef> condition_sources;
 for(const auto* a:o.assets){auto ref=asset_ref(o,*a);responsibility(out,take(x_.takeovers.read_before_supervision_start(ref,at,cut)),a->owner,ref,{a->id},r.approval.issuer.organization_id);
  if(a->kind==ProductionAssetKind::land)usable.push_back(a->capacity_units);
  else{auto capacity=take(read_production_facility_capacity_at_adoption(x_.conditions,a->id,at,cut));usable.push_back(capacity.usable_asset_units);if(capacity.usable_asset_units==0)condition_sources.push_back(capacity.condition_change_source_ref);append(out.actual_sources,{capacity.condition_change_source_ref,1});}
 }
 if(total_capacity(o,usable)<=0)unavailable(out,condition_sources);
 return {true,std::move(out),{}};
 }catch(const Failure&e){return {false,{},e.error};}}
Outcome<GovernanceSupervisionTargetView> NativeProductionSupervisionTargetRead::read_supervision_target_for_span(
 const GovernanceSupervisionTargetRequest& r,const SupervisionActionStartSource& start,const MomentSpan& span)const{try{
 const auto o=original(x_,r);if(cmp(span.from,span.to)>=0||cmp(span.from,start.started_at)<0||cmp(span.to,take(x_.clock.read_current_moment()))>0)fail("ProductionSupervisionServiceWindowMismatch",start.fact_id);
 GovernanceSupervisionTargetView out;out.original_target_anchor=o.anchor;append(out.actual_sources,{r.accepted_plan.target_site_or_anchor_ref,1});
 const auto check=[&](const PoliticalResponsibilityRef& ref,const Owner& owner,const std::vector<StableId>& assets){auto history=take(x_.takeovers.read_supervision_responsibility_window(ref,start,span));responsibility(out,history.initial,owner,ref,assets,r.approval.issuer.organization_id);for(const auto& change:history.changes){append(out.actual_sources,change.source);responsibility(out,change.after,owner,ref,assets,r.approval.issuer.organization_id);}};
 check(unit_ref(o),o.unit->owner,asset_ids(o));
 std::vector<std::vector<ConditionBoundAssetCapacitySegment>> conditions(o.assets.size());std::vector<WorldMoment> boundaries{span.from,span.to};
 for(std::size_t i=0;i<o.assets.size();++i){const auto& a=*o.assets[i];check(asset_ref(o,a),a.owner,{a.id});if(a.kind==ProductionAssetKind::land)continue;
  conditions[i]=take(read_production_facility_capacity_window(x_.conditions,a.id,span));if(conditions[i].empty())fail("ProductionSupervisionConditionWindowMissing",a.id);
  for(const auto& part:conditions[i]){boundaries.push_back(part.window.from);boundaries.push_back(part.window.to);append(out.actual_sources,{part.capacity.condition_change_source_ref,1});}
 }
 std::sort(boundaries.begin(),boundaries.end(),[](const auto&a,const auto&b){return cmp(a,b)<0;});boundaries.erase(std::unique(boundaries.begin(),boundaries.end(),[](const auto&a,const auto&b){return cmp(a,b)==0;}),boundaries.end());
 for(std::size_t n=1;n<boundaries.size();++n){std::vector<Integer> usable;std::vector<SourceRef> refs;
  for(std::size_t i=0;i<o.assets.size();++i){const auto& a=*o.assets[i];if(a.kind==ProductionAssetKind::land){usable.push_back(a.capacity_units);continue;}const ConditionBoundAssetCapacity* found=nullptr;
   for(const auto& part:conditions[i])if(cmp(part.window.from,boundaries[n-1])<=0&&cmp(boundaries[n],part.window.to)<=0){if(found)fail("ProductionSupervisionConditionWindowOverlap",a.id);found=&part.capacity;}
   if(!found)fail("ProductionSupervisionConditionWindowGap",a.id);usable.push_back(found->usable_asset_units);if(found->usable_asset_units==0)refs.push_back(found->condition_change_source_ref);
  }
  if(total_capacity(o,usable)<=0)unavailable(out,refs);
 }
 return {true,std::move(out),{}};
 }catch(const Failure&e){return {false,{},e.error};}}
}
