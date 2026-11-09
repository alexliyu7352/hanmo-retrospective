// JSON/shape and checkpoint are generated from the same four original authority fields.
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "transport_authority_codec.hpp"
#include "movement_exact_detail.hpp"
#include <set>
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include <type_traits>
namespace hanmo::resident {namespace expense_authority_json {using namespace movement_detail;using V=BoundaryValue;
const V& get(const V& v,const char* key){const auto* p=v.find(key);if(!p)fail("TransportAuthorityFieldRequired",key);return *p;}
void fields(const V& v,std::initializer_list<const char*> names){const auto* d=v.as<V::Dict>();if(!d||d->size()!=names.size())fail("TransportAuthorityFieldsMismatch");auto n=names.begin();for(const auto& row:*d)if(row.first!=*n++)fail("TransportAuthorityFieldOrderMismatch",row.first);}
void read(const V& v,StableId& out){const auto* p=v.as<StableId>();if(!p)fail("TransportAuthorityStringRequired");if(auto* budget=ScopedCheckpointJsonDecode::current())budget->text(out,*p);else out=*p;}
void read(const V& v,Integer& out){const auto* p=v.as<Integer>();if(!p)fail("TransportAuthorityIntegerRequired");out=*p;}
void read(const V& v,std::optional<StableId>& out){if(v.as<std::monostate>()){out.reset();return;}if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<StableId>();out.emplace();read(v,*out);}
Status emit(ProjectionWriter& w,std::string_view name,const StableId& v){return w.field(name,v);}
Status emit(ProjectionWriter& w,std::string_view name,const Integer& v){return w.field(name,v);}
Status emit(ProjectionWriter& w,std::string_view name,const std::optional<StableId>& v){return v?emit(w,name,*v):w.null(name);}
void read(const V& v,PublicBudgetExpenseAuthority& out){fields(v,{"execution_ref","payer_wallet_id"});read(get(v,"execution_ref"),out.execution_ref);read(get(v,"payer_wallet_id"),out.payer_wallet_id);}
Status emit(ProjectionWriter& w,std::string_view name,const PublicBudgetExpenseAuthority& v){return w.object(name,[&](auto& c){emit(c,"execution_ref",v.execution_ref);emit(c,"payer_wallet_id",v.payer_wallet_id);return c.status();});}
bool same(const PublicBudgetExpenseAuthority& a,const PublicBudgetExpenseAuthority& z){return a.execution_ref==z.execution_ref&&a.payer_wallet_id==z.payer_wallet_id;}
void read(const V& v,PrivateMandateExpenseAuthority& out){fields(v,{"mandate_id","accepted_terms_revision","authority_epoch","payer_wallet_id"});read(get(v,"mandate_id"),out.mandate_id);read(get(v,"accepted_terms_revision"),out.accepted_terms_revision);read(get(v,"authority_epoch"),out.authority_epoch);read(get(v,"payer_wallet_id"),out.payer_wallet_id);}
Status emit(ProjectionWriter& w,std::string_view name,const PrivateMandateExpenseAuthority& v){return w.object(name,[&](auto& c){emit(c,"mandate_id",v.mandate_id);emit(c,"accepted_terms_revision",v.accepted_terms_revision);emit(c,"authority_epoch",v.authority_epoch);emit(c,"payer_wallet_id",v.payer_wallet_id);return c.status();});}
bool same(const PrivateMandateExpenseAuthority& a,const PrivateMandateExpenseAuthority& z){return a.mandate_id==z.mandate_id&&a.accepted_terms_revision==z.accepted_terms_revision&&a.authority_epoch==z.authority_epoch&&a.payer_wallet_id==z.payer_wallet_id;}
void read(const V& v,SelfFundedExpenseAuthority& out){fields(v,{"funding_terms_id","principal_actor_id","payer_wallet_id"});read(get(v,"funding_terms_id"),out.funding_terms_id);read(get(v,"principal_actor_id"),out.principal_actor_id);read(get(v,"payer_wallet_id"),out.payer_wallet_id);}
Status emit(ProjectionWriter& w,std::string_view name,const SelfFundedExpenseAuthority& v){return w.object(name,[&](auto& c){emit(c,"funding_terms_id",v.funding_terms_id);emit(c,"principal_actor_id",v.principal_actor_id);emit(c,"payer_wallet_id",v.payer_wallet_id);return c.status();});}
bool same(const SelfFundedExpenseAuthority& a,const SelfFundedExpenseAuthority& z){return a.funding_terms_id==z.funding_terms_id&&a.principal_actor_id==z.principal_actor_id&&a.payer_wallet_id==z.payer_wallet_id;}
void read(const V& v,HouseholdPurchaseExpenseAuthority& out){fields(v,{"household_id","accepted_policy_ref","policy_revision","purchase_plan_ref","purchase_ref","phase"});read(get(v,"household_id"),out.household_id);read(get(v,"accepted_policy_ref"),out.accepted_policy_ref);read(get(v,"policy_revision"),out.policy_revision);read(get(v,"purchase_plan_ref"),out.purchase_plan_ref);read(get(v,"purchase_ref"),out.purchase_ref);read(get(v,"phase"),out.phase);}
Status emit(ProjectionWriter& w,std::string_view name,const HouseholdPurchaseExpenseAuthority& v){return w.object(name,[&](auto& c){emit(c,"household_id",v.household_id);emit(c,"accepted_policy_ref",v.accepted_policy_ref);emit(c,"policy_revision",v.policy_revision);emit(c,"purchase_plan_ref",v.purchase_plan_ref);emit(c,"purchase_ref",v.purchase_ref);emit(c,"phase",v.phase);return c.status();});}
bool same(const HouseholdPurchaseExpenseAuthority& a,const HouseholdPurchaseExpenseAuthority& z){return a.household_id==z.household_id&&a.accepted_policy_ref==z.accepted_policy_ref&&a.policy_revision==z.policy_revision&&a.purchase_plan_ref==z.purchase_plan_ref&&a.purchase_ref==z.purchase_ref&&a.phase==z.phase;}
}
Outcome<TransportExpenseAuthority> decode_transport_expense_authority(const BoundaryValue& v){return movement_detail::guard<TransportExpenseAuthority>([&]{expense_authority_json::fields(v,{"variant","value"});Integer tag=0;expense_authority_json::read(expense_authority_json::get(v,"variant"),tag);if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<TransportExpenseAuthority>();TransportExpenseAuthority out;switch(tag){case 0:break;case 1:if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<PrivateMandateExpenseAuthority>();out.emplace<1>();break;case 2:if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<SelfFundedExpenseAuthority>();out.emplace<2>();break;case 3:if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<HouseholdPurchaseExpenseAuthority>();out.emplace<3>();break;default:movement_detail::fail("InvalidTransportExpenseAuthority");}std::visit([&](auto& x){expense_authority_json::read(expense_authority_json::get(v,"value"),x);},out);return out;});}
Outcome<TransportExpenseAuthority> decode_transport_expense_authority(const BoundaryValue& v,ScopedCheckpointJsonDecode& budget){
 if(ScopedCheckpointJsonDecode::current()!=&budget)return {false,{},common_failure("CheckpointJsonContextMismatch").error};
 static_assert(std::is_nothrow_move_constructible_v<TransportExpenseAuthority>&&std::is_nothrow_move_assignable_v<TransportExpenseAuthority>,"original finite authority transfer must not copy its charged strings");
 return decode_transport_expense_authority(v);
}
Status emit_transport_expense_authority_fields(ProjectionWriter& c,const TransportExpenseAuthority& v){c.field("variant",Integer(v.index()));return std::visit([&](const auto& x){return expense_authority_json::emit(c,"value",x);},v);}
Status emit_transport_expense_authority_projection(ProjectionWriter& w,std::string_view name,const TransportExpenseAuthority& v){return w.object(name,[&](auto& c){return emit_transport_expense_authority_fields(c,v);});}
Outcome<BoundaryValue> export_transport_expense_authority(const TransportExpenseAuthority& v){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter w(out);movement_detail::take(emit_transport_expense_authority_projection(w,"",v));movement_detail::take(w.finish());return out;});}
const TransportExpenseAuthority* read_commercial_group_funding(const GroupJourneyFunding& funding) noexcept{return std::get_if<TransportExpenseAuthority>(&funding);}
bool same_transport_expense_authority(const GroupJourneyFunding& funding,const TransportExpenseAuthority& expected){const auto* actual=read_commercial_group_funding(funding);return actual&&same_transport_expense_authority(*actual,expected);}
bool same_transport_expense_authority(const TransportExpenseAuthority& a,const TransportExpenseAuthority& z){if(a.index()!=z.index())return false;return std::visit([&](const auto& x){return expense_authority_json::same(x,std::get<std::decay_t<decltype(x)>>(z));},a);}
}
