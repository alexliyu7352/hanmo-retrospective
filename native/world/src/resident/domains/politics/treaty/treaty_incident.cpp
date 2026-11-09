#include "treaty_incident.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
std::size_t position(const CommonStore& c,const SourceRef& r){
 const auto* f=c.state().facts.find(r.id);if(!valid_source_ref(r)||!f||f->domain!=r.domain||f->kind!=r.kind)
  fail("TreatyIncidentActualFactMissing",r.id);
 const auto& order=c.state().facts.order();auto i=std::find(order.begin(),order.end(),r.id);
 if(i==order.end())fail("TreatyIncidentFactOrderMissing",r.id);return static_cast<std::size_t>(i-order.begin());
}
bool same_obligations(const std::vector<TreatyObligationRef>& a,const std::vector<TreatyObligationRef>& b){
 if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!same_treaty_obligation_ref(a[i],b[i]))return false;return true;
}
bool key(const TreatyIncident& a,const TreatyIncident& b){
 return a.treaty_id==b.treaty_id&&a.terms_revision==b.terms_revision&&a.term_id==b.term_id&&
  same_source_ref(a.source_fact_ref,b.source_fact_ref)&&a.incident_kind==b.incident_kind&&a.liable_party==b.liable_party;
}
TreatyIncidentOriginalView original(const TreatyIncidentReadContext& x,const StableId& treaty,Integer revision,
 const StableId& term,const SourceRef& source){
 if(treaty.empty()||revision<1||term.empty()||!valid_source_ref(source))fail("InvalidTreatyIncidentSource",treaty);
 auto view=take(x.originals.read_actual_treaty_incident(treaty,revision,term,source));
 if(view.treaty_id!=treaty||view.terms_revision!=revision||view.term_id!=term||
    !same_source_ref(view.actual_source,source)||view.liable_party.empty()||
    static_cast<unsigned>(view.kind)>static_cast<unsigned>(TreatyIncidentKind::deliberate_breach))
  fail("TreatyIncidentOriginalMismatch",treaty);
 take(encode_world_moment(view.occurred_at));const auto now=take(x.clock.read_organization_authority_now());
 if(compare(view.occurred_at,now)>0)fail("TreatyIncidentInFuture",source.id);
 position(x.common,source);
 for(std::size_t i=0;i<view.remaining_obligations.size();++i){
  const auto& r=view.remaining_obligations[i];take(validate_treaty_obligation_ref_shape(r));
  for(std::size_t j=0;j<i;++j)if(same_treaty_obligation_ref(r,view.remaining_obligations[j]))fail("TreatyIncidentDuplicateObligation",treaty);
  take(x.originals.validate_incident_obligation_at_source(r,view));
 }
 return view;
}
TreatyIncidentOriginalView validate_one(const TreatyIncidentReadContext& x,const TreatyIncident& row){
 take(validate_treaty_incident_shape(row));auto actual=original(x,row.treaty_id,row.terms_revision,row.term_id,row.source_fact_ref);
 if(row.occurrence_tick!=actual.occurred_at.half_day_tick||row.incident_kind!=actual.kind||
    row.liable_party!=actual.liable_party||!same_obligations(row.remaining_obligation_refs,actual.remaining_obligations))
  fail("TreatyIncidentOriginalMismatch",row.id);
 if(position(x.common,row.source_fact_ref)>=position(x.common,treaty_incident_source(row)))
  fail("TreatyIncidentBeforeActualSource",row.id);
 return actual;
}
}
bool same_treaty_obligation_ref(const TreatyObligationRef& a,const TreatyObligationRef& b){return a.owner_domain==b.owner_domain&&a.contract_id==b.contract_id&&a.term_id==b.term_id&&a.terms_revision==b.terms_revision;}
Status validate_treaty_obligation_ref_shape(const TreatyObligationRef& r){
 if(r.owner_domain.empty()||r.contract_id.empty()||r.term_id.empty()||r.terms_revision<1)return common_failure("InvalidTreatyObligationRef");
 return common_success();
}
Status validate_treaty_incident_shape(const TreatyIncident& r){try{
 if(r.id.empty()||r.treaty_id.empty()||r.terms_revision<1||r.term_id.empty()||!valid_source_ref(r.source_fact_ref)||
    r.occurrence_tick<0||r.liable_party.empty()||static_cast<unsigned>(r.incident_kind)>static_cast<unsigned>(TreatyIncidentKind::deliberate_breach))
  fail("InvalidTreatyIncident",r.id);
 for(std::size_t i=0;i<r.remaining_obligation_refs.size();++i){take(validate_treaty_obligation_ref_shape(r.remaining_obligation_refs[i]));
  for(std::size_t j=0;j<i;++j)if(same_treaty_obligation_ref(r.remaining_obligation_refs[i],r.remaining_obligation_refs[j]))fail("TreatyIncidentDuplicateObligation",r.id);}
 return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
SourceRef treaty_incident_source(const TreatyIncident& r){return {"politics","treaty_incident",r.id};}
Outcome<const TreatyIncident*> read_treaty_incident(const TreatyIncidentReadContext& x,const SourceRef& ref,Integer schema){try{
 if(schema!=1||ref.domain!="politics"||ref.kind!="treaty_incident")fail("TreatyIncidentSourceUnsupported",ref.id);
 const auto* r=x.incidents.find(ref.id);if(!r||r->id!=ref.id)fail("TreatyIncidentMissing",ref.id);
 validate_one(x,*r);return {true,r,{}};
 }catch(const Failure& f){return {false,nullptr,f.error};}}
Outcome<WorldMoment> read_treaty_incident_source_moment(const TreatyIncidentReadContext& x,const SourceRef& ref,Integer schema){try{
 const auto* r=take(read_treaty_incident(x,ref,schema));return {true,validate_one(x,*r).occurred_at,{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Status validate_treaty_incidents(const TreatyIncidentReadContext& x){try{
 for(const auto& row:x.incidents.rows()){
  if(row.first!=row.second.id)fail("TreatyIncidentIdentityMismatch",row.first);validate_one(x,row.second);
  for(const auto& other:x.incidents.rows())if(other.first!=row.first&&key(row.second,other.second))fail("TreatyIncidentDuplicateActualSource",row.first);
 }
 for(const auto& fact:x.common.state().facts.rows())if(fact.second.domain=="politics"&&fact.second.kind=="treaty_incident"&&!x.incidents.find(fact.first))
  fail("TreatyIncidentOrphanFact",fact.first);
 return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<SourceRef> record_treaty_incident_in_candidate(CommonStore& c,OrderedTable<TreatyIncident>& rows,
 const TreatyIncidentReadContext& x,const StableId& treaty,Integer revision,const StableId& term,const SourceRef& source){try{
 if(&c!=&x.common||&rows!=&x.incidents)fail("TreatyIncidentCandidateMismatch");
 auto actual=original(x,treaty,revision,term,source);
 TreatyIncident row;row.treaty_id=treaty;row.terms_revision=revision;row.term_id=term;row.source_fact_ref=source;
 row.occurrence_tick=actual.occurred_at.half_day_tick;row.incident_kind=actual.kind;row.liable_party=actual.liable_party;
 row.remaining_obligation_refs=actual.remaining_obligations;
 for(const auto& previous:rows.rows())if(key(previous.second,row)){
  validate_one(x,previous.second);return {true,treaty_incident_source(previous.second),{}};
 }
 // Actual consumers must adopt the event in its original candidate, rather than
// manufacture an old incident later from a newly discovered hidden fact.
 if(compare(actual.occurred_at,take(x.clock.read_organization_authority_now()))!=0)
  fail("TreatyIncidentOriginalCandidateRequired",source.id);
 row.id=take(c.next_id("treaty_incident"));const auto result=treaty_incident_source(row);
 take(c.append_fact({row.id,result.kind,result.domain}));
 if(!rows.insert(result.id,std::move(row)))fail("TreatyIncidentDuplicateId",result.id);
 validate_one(x,*rows.find(result.id));return {true,result,{}};
 }catch(const Failure& f){return {false,{},f.error};}}
}
