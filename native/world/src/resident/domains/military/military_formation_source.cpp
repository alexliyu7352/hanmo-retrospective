#include "resident/domains/military/military_model.hpp"
#include "military_formation_source.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <charconv>
#include <algorithm>
namespace hanmo::resident {using namespace movement_detail;namespace {
CheckpointMemoryCharge reserve_formation_source(const MilitaryFormationSourceBuildView& input){return CheckpointMemoryCharge(0);}
Integer serial(const CommonState& c,const StableId& id){const auto at=id.rfind(':');Integer value=0;if(at==StableId::npos)fail("MilitaryFormationOriginalSerialRequired",id);const std::string_view suffix{id.data()+at+1,id.size()-at-1};const auto p=std::from_chars(suffix.data(),suffix.data()+suffix.size(),value);if(suffix.empty()||suffix.front()=='0'||p.ec!=std::errc{}||p.ptr!=suffix.data()+suffix.size()||value<1||value>=c.next_instance_id)fail("MilitaryFormationOriginalSerialRequired",id);return value;}
void header(const CommonState& c,const SourceRef& ref){const auto* f=c.facts.find(ref.id);if(!f||f->id!=ref.id||f->domain!=ref.domain||f->kind!=ref.kind)fail("MilitaryFormationActualSourceRequired",ref.id);}
}
MilitaryFormationSourceView::MilitaryFormationSourceView(const MilitaryFormationSourceBuildView& input)
 :copied_fields(reserve_formation_source(input)),original_terms(input.original_terms),
 completed_source{{StableId(input.completed_source.source_ref.domain),StableId(input.completed_source.source_ref.kind),StableId(input.completed_source.source_ref.id)},input.completed_source.source_schema},
 original_action_source{StableId(input.original_action_source.domain),StableId(input.original_action_source.kind),StableId(input.original_action_source.id)},
 completed_at(input.completed_at),original_completion_serial(input.original_completion_serial){}
Outcome<MilitaryFormationSourceView> read_original_military_formation(const CommonState& c,const MilitaryState& m,const ArmyRecord& army,const WorldMoment& now,const MilitaryFormationSourceRead* exact,ExactArithmeticBudget budget){return guard<MilitaryFormationSourceView>([&]{
 const MilitaryActionFact* old=nullptr;for(const auto& row:m.action_facts)if(row.id==army.formation_source){if(old)fail("MilitaryFormationSourceRepeated",row.id);old=&row;}
 const auto actual=[&]()->std::optional<MilitaryFormationSourceView>{if(!exact)return std::nullopt;if(!exact->bound_to(c,m))fail("CrossCandidateMilitaryFormationRead",army.id);return take(exact->read_exact_army_formation(army.id));}();
 if(bool(old)==bool(actual))fail(old?"MilitaryFormationSourceDuplicated":"ExactMilitaryFormationSourceReadRequired",army.id);
 MilitaryFormationSourceView out=[&]{
  if(!old)return MilitaryFormationSourceView(*actual);
  const auto* terms=std::get_if<ArmyFormationTerms>(&old->terms);
  if(!terms||old->command!="army_form"||old->phase!="completed"||!old->success||!old->result||old->result->army_id!=army.id||old->actor_id!=terms->actor_id||old->completed_tick<0)fail("MilitaryFormationOriginalResultRequired",army.id);
  const auto completed=take(normalize_world_moment(old->completed_tick,{},budget));
  const auto completion_serial=serial(c,army.id);if(completion_serial<=serial(c,old->id))fail("MilitaryFormationActualOrderMismatch",army.id);
  const SourceRefBuildView source{"military","military_action",old->id};
  return MilitaryFormationSourceView{MilitaryFormationSourceBuildView{terms,{source,1},source,completed,completion_serial}};
 }();
 const auto* terms=out.original_terms;if(!terms||out.original_action_source.domain!="military"||out.original_action_source.kind!="military_action"||out.original_action_source.id!=army.formation_source||terms->organization_id!=army.organization_id||terms->order_id!=army.order_id||!c.actors.find(terms->actor_id))fail("MilitaryFormationArmyIdentityMismatch",army.id);
 header(c,out.original_action_source);header(c,out.completed_source.source_ref);take(encode_world_moment(out.completed_at,budget));if(take(compare_world_moments(out.completed_at,now,budget))>0)fail("MilitaryFormationFutureSource",army.id);
 if(out.completed_source.source_schema==2){const auto& ref=out.completed_source.source_ref;if(ref.domain!="military"||ref.kind!="military_action_completion_applied"||out.original_completion_serial!=serial(c,ref.id)||serial(c,army.id)>=out.original_completion_serial||serial(c,army.formation_source)>=serial(c,army.id))fail("MilitaryFormationActualOrderMismatch",army.id);const auto a=take(read_actual_common_fact_position(c,out.original_action_source)),z=take(read_actual_common_fact_position(c,ref));if(a>=z)fail("MilitaryFormationActualOrderMismatch",army.id);}
 else if(out.completed_source.source_schema!=1||!same_source(out.completed_source.source_ref,out.original_action_source))fail("MilitaryFormationSourceSchemaMismatch",army.id);
 return out;
});}
}
