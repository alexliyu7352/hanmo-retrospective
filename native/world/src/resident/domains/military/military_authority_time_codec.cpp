#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_authority_time_codec.hpp"
#include "military_authority_time_codec_fields.hpp"
#include "military_state_codec_archive.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
// Strictly call-local codec staging. These maps are never a World/seed member,
// provider or source ledger. Their copies retain their own actual reservations.
struct Staging {
 OrderedTable<MilitaryCampaignAcceptanceTime1> campaign_acceptances;
 OrderedTable<MilitaryOfficeResignationTime1> office_resignations;
};
struct Keys {
 template<class T>static constexpr bool owns=std::is_same_v<T,MilitaryCampaignAcceptanceTime1>||std::is_same_v<T,MilitaryOfficeResignationTime1>;
 bool operator()(const StableId& key,const MilitaryCampaignAcceptanceTime1& v)const{return !key.empty()&&key==v.actual_acceptance_source.id;}
 bool operator()(const StableId& key,const MilitaryOfficeResignationTime1& v)const{return !key.empty()&&key==v.actual_resignation_source.id;}
};
template<class T>T* find(std::vector<T>& rows,const StableId& key){
 T* found=nullptr;for(auto& row:rows)if(row.id==key){if(found)fail("MilitaryAuthorityOriginalOwnerAmbiguous",key);found=&row;}return found;
}
template<class A>Status fields(A& original,Staging& staging,Integer selected){
 if(selected!=1)return original.reject("MilitaryAuthorityTimeSelectedSchemaRequired");
 Integer schema=selected;original.field("schema",schema);
 if(!original.status().ok)return original.status();
 if(schema!=selected)return original.reject("MilitaryAuthorityTimeSelectedSchemaMismatch");
 military_state_codec::RootArchive<A,Keys> root{original};
 root.table("campaign_acceptances",staging.campaign_acceptances,[](auto& x,auto& row){return military_authority_time_fields::value(x,row);});
 root.table("office_resignations",staging.office_resignations,[](auto& x,auto& row){return military_authority_time_fields::value(x,row);});
 return root.status();
}
void collect(const MilitaryState& state,Staging& staging){
 for(const auto& row:state.campaigns){
  if(!row.exact_acceptance)fail("MilitaryCampaignAcceptanceSourceRequired",row.id);
  if(!Keys{}(row.id,*row.exact_acceptance)||!staging.campaign_acceptances.insert(row.id,*row.exact_acceptance))fail("MilitaryAuthorityOriginalOwnerAmbiguous",row.id);
 }
 for(const auto& row:state.resignation_facts){
  if(!row.exact_resignation)fail("MilitaryOfficeResignationSourceRequired",row.id);
  if(!Keys{}(row.id,*row.exact_resignation)||!staging.office_resignations.insert(row.id,*row.exact_resignation))fail("MilitaryAuthorityOriginalOwnerAmbiguous",row.id);
 }
}
void empty_targets(const MilitaryState& state){
 for(const auto& row:state.campaigns)if(row.exact_acceptance)fail("MilitaryAuthorityAdjunctAlreadyAttached",row.id);
 for(const auto& row:state.resignation_facts)if(row.exact_resignation)fail("MilitaryAuthorityAdjunctAlreadyAttached",row.id);
}
void attach(MilitaryState& state,const Staging& staging){
 // Validate the complete attachment set before changing any original owner.
 if(staging.campaign_acceptances.size()!=state.campaigns.size()||staging.office_resignations.size()!=state.resignation_facts.size())fail("MilitaryAuthorityAdjunctCoverageMismatch");
 for(const auto& row:staging.campaign_acceptances.rows())if(!find(state.campaigns,row.first))fail("MilitaryCampaignAcceptanceOrphan",row.first);
 for(const auto& row:staging.office_resignations.rows())if(!find(state.resignation_facts,row.first))fail("MilitaryOfficeResignationOrphan",row.first);
 for(const auto& row:staging.campaign_acceptances.rows())find(state.campaigns,row.first)->exact_acceptance.emplace(row.second);
 for(const auto& row:staging.office_resignations.rows())find(state.resignation_facts,row.first)->exact_resignation.emplace(row.second);
}
}
Status visit_military_authority_time(CodecVisitor& c,MilitaryState& state,Integer selected){return status([&]{try{
 // Actual parent ownership is visited by the sole World retained graph. Never
 // construct a synthetic codec table while measuring the original World.
 if(selected!=1)fail("MilitaryAuthorityTimeSelectedSchemaRequired");

 if(c.reading())empty_targets(state);Staging staging;if(!c.reading())collect(state,staging);
 take(fields(c,staging,selected));if(c.reading())attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status decode_military_authority_time(const BoundaryValue& raw,MilitaryState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("MilitaryAuthorityTimeSelectedSchemaRequired");
 if(!ScopedCheckpointJsonDecode::current())fail("CheckpointJsonContextRequired");
 empty_targets(state);Staging staging;ActorFoundationJsonVisitor c(raw);take(c.ordered_fields());
 take(fields(c,staging,selected));take(c.finish());attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_authority_time_fields(ProjectionWriter& sink,const MilitaryState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("MilitaryAuthorityTimeSelectedSchemaRequired");
 Staging staging;collect(state,staging);ActorFoundationProjectionArchive c(sink);take(fields(c,staging,selected));
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_authority_time(ProjectionWriter& sink,std::string_view name,const MilitaryState& state,Integer selected){
 return sink.object(name,[&](ProjectionWriter& fields){return emit_military_authority_time_fields(fields,state,selected);});
}
Outcome<BoundaryValue> encode_military_authority_time(const MilitaryState& state,Integer selected){return guard<BoundaryValue>([&]{
 BoundaryValue result;BoundaryProjectionWriter sink(result);take(emit_military_authority_time(sink,"",state,selected));take(sink.finish());return result;
});}
}
