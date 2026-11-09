#include "repair_service_internal.hpp"
#include "resident/domains/services/service_actual_termination_contract.hpp"
namespace hanmo::resident::repair_service_detail {
WorldMoment terminal_moment(const RepairServiceReadContext&x,const RepairServiceContractSource&t,const MomentOwnerSource&r){
 const auto&a=accepted_scope(x,t);if(!x.repair)fail("RepairOriginalTerminalReaderRequired",t.id,ErrorKind::unsupported_capability);if(r.source_schema!=1||(r.source_ref.kind!="repair_interrupted"&&r.source_ref.kind!="repair_unstarted_interrupted")||r.source_ref.domain!="crafting")fail("RepairInternalTerminalRequired",r.source_ref.id);
 const auto accepted=repair_accepted_source(a.id);RepairService scope{accepted,a.terms.instance_id,SourceFactTerms{accepted}};const auto result=take(read_repair_service_internal_termination(*x.repair,scope,{accepted,r.source_ref.id,1}));return result.unstarted?result.unstarted->occurred_at:result.started->occurred_at;
}
const RepairServiceTerminationSettlementSource&settlement(const RepairServiceReadContext&x,const MomentOwnerSource&r){
 if(r.source_schema!=1||!same_source_ref(r.source_ref,repair_service_termination_settled_source(r.source_ref.id)))fail("UnsupportedRepairSettlement",r.source_ref.id,ErrorKind::unsupported_capability);const auto*s=state(x).termination_settlements.find(r.source_ref.id);if(!s||s->id!=r.source_ref.id)fail("RepairOriginalSettlementRequired",r.source_ref.id);const auto&t=contract(x,s->contract_ref);const auto at=terminal_moment(x,t,s->actual_terminal_ref);
 if(compare(s->settled_at,at)||compare(s->settled_at,now(x))>0||header(x.common,r.source_ref)<=header(x.common,s->actual_terminal_ref.source_ref))fail("RepairOriginalSettlementBoundaryMismatch",s->id);
 for(const auto&other:state(x).termination_settlements.rows())if(other.first!=s->id&&same(other.second.contract_ref,s->contract_ref))fail("DuplicateRepairOriginalSettlement",s->id);
 std::size_t n=0;for(const auto&row:state(x).charges.rows())if(same(row.second.contract_ref,s->contract_ref)){const auto&q=charge(x,{repair_service_charge_source(row.first),1});if(n==s->preserved_charge_refs.size()||!same(s->preserved_charge_refs[n++],{repair_service_charge_source(q.id),1})||header(x.common,repair_service_charge_source(q.id))>=header(x.common,s->actual_terminal_ref.source_ref))fail("RepairSettlementEarnedChargeCoverageMismatch",s->id);}if(n!=s->preserved_charge_refs.size())fail("RepairSettlementEarnedChargeCoverageMismatch",s->id);
 const auto continuity=take(x.original.read_original_repair_terminal_continuity(t,s->actual_terminal_ref));if(continuity.size()!=s->custody_or_continuity_refs.size())fail("RepairSettlementOriginalCustodyCoverageMismatch",s->id);for(std::size_t i=0;i<continuity.size();++i){ref(continuity[i]);if(!same(continuity[i],s->custody_or_continuity_refs[i]))fail("RepairSettlementOriginalCustodyMismatch",s->id);}return *s;
}
void validate_repair_settlement_sources(const RepairServiceReadContext&x){
 for(const auto&row:state(x).termination_settlements.rows()){if(row.first!=row.second.id)fail("RepairSettlementKeyMismatch",row.first);settlement(x,{repair_service_termination_settled_source(row.first),1});}
}
}
namespace hanmo::resident {using namespace repair_service_detail;
Outcome<SourceRef>settle_original_repair_service_termination_in_candidate(CommonStore&c,ItemInstancesState&i,LocalEconomyState&e,const RepairServiceReadContext&x,const MomentOwnerSource&r,const MomentOwnerSource&terminal){try{
 writable(c,i,e,x);const auto&t=contract(x,r);const auto terminal_at=terminal_moment(x,t,terminal);auto&s=i.repair_service_contracts;
 for(const auto&row:s.termination_settlements.rows())if(same(row.second.contract_ref,r)){const auto&old=settlement(x,{repair_service_termination_settled_source(row.first),1});if(!same(old.actual_terminal_ref,terminal))fail("ConflictingRepairSettlementReplay",row.first);return {true,repair_service_termination_settled_source(row.first),{}};}
 if(compare(now(x),terminal_at))fail("RepairActualInternalTerminalBoundaryRequired",terminal.source_ref.id);
 RepairServiceTerminationSettlementSource result;result.id=take(c.next_id("repair_service_settlement"));result.contract_ref=r;result.actual_terminal_ref=terminal;result.settled_at=now(x);result.custody_or_continuity_refs=take(x.original.read_original_repair_terminal_continuity(t,terminal));
 for(const auto&row:s.charges.rows())if(same(row.second.contract_ref,r)){charge(x,{repair_service_charge_source(row.first),1});result.preserved_charge_refs.push_back({repair_service_charge_source(row.first),1});}
 const auto source=repair_service_termination_settled_source(result.id);append(c,source);if(!s.termination_settlements.insert(source.id,std::move(result)))fail("DuplicateRepairSettlement",source.id);settlement(x,{source,1});return {true,source,{}};
}catch(Failure&error){return {false,{},std::move(error.error)};}}
Status finalize_repair_service_acceptance_in_candidate(CommonStore&c,ItemInstancesState&i,const RepairServiceReadContext&x,const MomentOwnerSource&r){try{writable(c,i,x);accepted_scope(x,contract(x,r));take(validate_repair_service_contracts(x));return common_success();}catch(Failure&e){return {false,{},std::move(e.error)};}}
}
