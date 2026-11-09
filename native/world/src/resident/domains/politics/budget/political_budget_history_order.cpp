#include "political_budget_history_order.hpp"
#include <map>
#include <set>
#include <new>
#include <stdexcept>
namespace hanmo::resident {namespace {
const char* kind(PublicBudgetDecisionKind decision){switch(decision){
 case PublicBudgetDecisionKind::authorize:return "public_budget_authorized";
 case PublicBudgetDecisionKind::delegate:return "public_budget_delegated";
 case PublicBudgetDecisionKind::amend:return "public_budget_amended";
 case PublicBudgetDecisionKind::close:return "public_budget_closed";
 }return nullptr;}
bool budget_kind(const StableId& k){return k=="public_budget_authorized"||k=="public_budget_delegated"||k=="public_budget_amended"||k=="public_budget_closed";}
}
Outcome<std::vector<const BudgetAuthorizationSource*>> read_public_budget_authorizations_in_actual_order(const CommonStore& common,const PoliticalBudgetState& state){try{
 using Result=Outcome<std::vector<const BudgetAuthorizationSource*>>;
 auto fail=[](const char* code,const StableId& id={}){return Result{false,{},common_failure(code,id).error};};
 if(state.schema_version!=1&&state.schema_version!=2)return fail("UnsupportedPoliticalBudgetsSchema");
 if(state.authorization_history.size()>political_budget_history_limit)return fail("CheckpointCollectionTooLarge","political_budgets/authorization_history");
 std::map<StableId,const BudgetAuthorizationSource*> pending;
 std::set<StableId> covered;
 for(const auto& row:state.authorization_history){
  const auto valid=validate_budget_authorization_shape(row,state.schema_version);if(!valid.ok)return {false,{},valid.error};
  const auto* expected=kind(row.decision);
  if(!expected||row.approval_source.domain!="politics"||row.approval_source.kind!=expected)return fail("BudgetApprovalDecisionSourceMismatch",row.budget_id);
  const auto* header=common.state().facts.find(row.approval_source.id);
  if(!header||header->id!=row.approval_source.id||header->domain!=row.approval_source.domain||header->kind!=row.approval_source.kind)return fail("BudgetApprovalSourceMissing",row.budget_id);
  if(!pending.emplace(row.approval_source.id,&row).second)return fail("DuplicateBudgetApprovalSource",row.approval_source.id);
 }
 std::vector<const BudgetAuthorizationSource*> result;result.reserve(pending.size());
 for(const auto& id:common.state().facts.order()){
  const auto* header=common.state().facts.find(id);
  if(!header||header->id!=id)return fail("BudgetActualCommonOrderMalformed",id);
  if(header->domain!="politics"||!budget_kind(header->kind))continue;
  const auto row=pending.find(id);if(row==pending.end())return fail("BudgetApprovalPayloadMissingOrDuplicateHeader",id);
  result.push_back(row->second);covered.insert(id);pending.erase(row);
 }
 if(!pending.empty())return fail("BudgetApprovalMissingFromActualOrder",pending.begin()->first);
 // Rows omitted from order cannot disappear from the reciprocal Common check.
 for(const auto& pair:common.state().facts.rows())if(pair.second.domain=="politics"&&budget_kind(pair.second.kind)){
  if(pair.first!=pair.second.id)return fail("BudgetActualCommonHeaderIdentityMismatch",pair.first);
  if(!covered.count(pair.first))return fail("BudgetApprovalMissingFromActualOrder",pair.first);
 }
 return {true,std::move(result),{}};
 }catch(const std::bad_alloc&){return {false,{},common_failure("BudgetHistoryOrderResourceExhausted").error};}
 catch(const std::length_error&){return {false,{},common_failure("BudgetHistoryOrderResourceExhausted").error};}}
}
