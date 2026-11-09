// 财政必要域门使用显式typed政治读；真实钱货与旧欠期跑原财政规则，不代真实军事/World验收。
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
#include "resident/domains/local_economy/takeover/responsibility_execution.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/storage/common_codec.hpp"
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
// 组织存在性夹具不授予任何命令或付款权。
struct Organizations final:OrganizationRead {
 bool organization_exists(const StableId& id)const override{return id=="old"||id=="new"||id=="payer";}bool organization_active(const StableId& id)const override{return organization_exists(id);}
 bool institutional_obligation_allowed(const StableId&,const StableId&,const StableId&,const StableId&,Integer,Integer)const override{return false;}
 bool actor_accepts_duty(const StableId&,const StableId&)const override{return false;}std::optional<Integer> authority_epoch(const StableId&)const override{return {};}
 Outcome<AuthorityProof> authorize(const ActingAuthority&,const StableId&,const ScopeRef&,Integer)const override{return {false,{},common_failure("NoTestCommandAuthority").error};}
 std::optional<OrganizationMemberView> member_view(const StableId&,const StableId&)const override{return {};}
};
// 已有期按具名原来源读；占领后的新期明确无政策，不能重写原期。
struct Policies final:FiscalPolicyRead {
 FiscalPolicySnapshot original;Integer occupied_at=INT64_MAX;
 Outcome<std::optional<FiscalPolicySnapshot>> fiscal_policy_for_due_tick(const StableId&,Integer due)const override{return {true,due>=occupied_at||due<original.effective_from_tick?std::optional<FiscalPolicySnapshot>{}:original,{}};}
 Outcome<std::optional<FiscalPolicySnapshot>> fiscal_policy_for_assessment_source(const StableId& site,Integer due,const StableId& id)const override{if(site!=original.site_id||id!=original.source_fact_id||due<original.effective_from_tick)return {false,{},common_failure("BadTestPolicySource").error};return {true,original,{}};}
};
// 只表示原assessment实际已被登记，不构造批准；同tick先后仍由真实Common事实顺序判定。
struct PendingPolitics final:PoliticalTakeoverRead {
 const CommonStore* common=nullptr;StableId assessment;PoliticalTakeoverRegistrationSource registration;bool hidden=false;
 Outcome<PoliticalTakeoverApproval> political_takeover_source(const StableId&)const override{return {false,{},common_failure("NoTestApproval").error};}
 Outcome<std::optional<PoliticalResponsibilityExecution>> find_political_responsibility_for_execution(const PoliticalResponsibilityRef& ref,const WorldIdentity& identity)const override{
  if(hidden||ref.kind!=PoliticalResponsibilityKind::fiscal_assessment||ref.responsibility_id!=assessment)return {true,std::nullopt,{}};PoliticalResponsibilityExecution out;out.registration=registration;out.registration.source_identity=identity;out.registration_fact_id=registration.registration_fact_id;out.responsibility=ref;out.code="PoliticalResponsibilityPending";return {true,out,{}};
 }
 Outcome<PoliticalResponsibilityExecution> political_responsibility_for_execution(const StableId&,const PoliticalResponsibilityRef& ref,const WorldIdentity& identity)const override{auto value=find_political_responsibility_for_execution(ref,identity);if(!value.ok||!value.value)return {false,{},common_failure("MissingTestRegistration").error};return {true,*value.value,{}};}
 Outcome<std::optional<PoliticalResponsibilityExecution>> political_responsibility_before_fact(const PoliticalResponsibilityRef& ref,const StableId& id,Integer,const WorldIdentity& identity)const override{bool registered=false;for(const auto& row:common->state().facts.rows()){if(row.first==registration.registration_fact_id)registered=true;if(row.first==id)return registered?find_political_responsibility_for_execution(ref,identity):Outcome<std::optional<PoliticalResponsibilityExecution>>{true,std::nullopt,{}};}return {false,{},common_failure("UnknownTestExecutionFact").error};}
};
int main(){
 auto catalog=std::make_shared<CommonCatalog>();catalog->organization_ids={"old","new","payer"};catalog->items.insert("grain",{"grain",1,1});CommonState initial;const Owner old{Owner::Kind::organization,"old"},next{Owner::Kind::organization,"new"},payer{Owner::Kind::organization,"payer"};
 initial.wallets.insert("payer.cash",{"payer.cash",payer,50,{}});initial.wallets.insert("tax.cash",{"tax.cash",old,0,{}});initial.inventories.insert("payer.grain",{"payer.grain",payer,100,0,{}});initial.inventories.insert("tax.grain",{"tax.grain",old,100,0,{}});CommonStore common(std::move(initial),catalog);
 auto fact=[&](const StableId& prefix,const StableId& kind,const StableId& domain){auto id=common.next_id(prefix);assert(id.ok&&common.append_fact({id.value,kind,domain}).ok);return id.value;};
 LocalEconomyCatalog local;local.enabled=true;LocalEconomyState economy;FiscalSettlementCatalog taxes;FiscalDistrictProfile district;district.id="district";district.site_id="town";district.period_ticks=2;district.first_due_tick=2;district.sources.insert("source",{"source",{SubjectKind::organization,"payer"},"payer.cash","payer.grain","grain",{},100,0,0});taxes.districts.insert(district.id,district);Organizations organizations;Policies policies;policies.original={"district","town","old","tax.cash","tax.grain",fact("policy","political_fiscal_policy_changed","politics"),1,0,1,FiscalTaxMode::normal};auto made=make_initial_fiscal_settlement(common,economy,local,organizations,policies,taxes);assert(made.ok);auto fiscal=std::move(made.value);
 for(Integer tick=1;tick<=2;++tick){economy.last_step_tick=tick;auto result=step_fiscal_settlement(common,economy,local,organizations,policies,taxes,fiscal,{tick-1,tick,{},{},{}});assert(result.ok);common.state().identity.tick=tick;}
 assert(fiscal.assessments.size()==1&&fiscal.payments.size()==1&&common.wallet("payer.cash")->balance==0&&common.wallet("tax.cash")->balance==50);const auto debt=fiscal.assessments.rows().front().first;assert(fiscal.assessments.find(debt)->cash.original_due==100&&fiscal.assessments.find(debt)->cash.paid_amount==50);
 PendingPolitics politics;politics.common=&common;politics.assessment=debt;auto& r=politics.registration;r.occupation_fact_id=fact("military","military_occupation","military");r.registration_fact_id=fact("political_takeover_pending","political_public_responsibilities_pending","politics");r.jurisdiction_id="district";r.site_id="town";r.former_organization_id="old";r.successor_organization_id="new";r.occupation_tick=2;r.public_wallet_ids={"tax.cash"};r.public_inventory_ids={"tax.grain"};policies.occupied_at=2;
 assert(common.apply_resources({{SetWalletOwner{"tax.cash",next},SetInventoryOwner{"tax.grain",next}},{}}).ok);EconomicTakeoverReadContext read{common,economy,local,&fiscal,&taxes,nullptr,politics};
 // 原严格入口确实复现当前Owner误拒；接管读必须逐责任证明原收款机构，没有笼统放宽。
 auto legacy=validate_fiscal_settlement(common,economy,local,organizations,policies,taxes,fiscal);assert(!legacy.ok&&legacy.error.original.code=="InvalidFiscalAssessmentPolicy");auto historical=validate_fiscal_settlement(common,economy,local,organizations,policies,taxes,fiscal,&read);if(!historical.ok)std::cerr<<historical.error.original.code<<"\n";assert(historical.ok);
 const auto original=encode_fiscal_settlement_state(fiscal);auto decoded=decode_fiscal_settlement_state(original);assert(decoded.ok&&compare_boundary(original,encode_fiscal_settlement_state(decoded.value)).exact());
 assert(common.apply_resources({{WalletDelta{"tax.cash",-20,true},WalletDelta{"payer.cash",20,true}},{}}).ok);const auto assets=encode_common_state(common.state());const auto payments=fiscal.payments.size();economy.last_step_tick=3;auto pending=step_fiscal_settlement(common,economy,local,organizations,policies,taxes,fiscal,{2,3,{},{},{}},&read);if(!pending.ok)std::cerr<<pending.error.original.code<<"\n";assert(pending.ok&&fiscal.payments.size()==payments&&common.wallet("payer.cash")->balance==20&&common.wallet("tax.cash")->balance==30&&compare_boundary(assets,encode_common_state(common.state())).exact());common.state().identity.tick=3;
 economy.last_step_tick=4;pending=step_fiscal_settlement(common,economy,local,organizations,policies,taxes,fiscal,{3,4,{},{},{}},&read);assert(pending.ok&&fiscal.assessments.size()==1&&fiscal.periods.size()==2&&!fiscal.periods.rows().back().second.policy&&fiscal.assessments.find(debt)->cash.original_due==100&&fiscal.assessments.find(debt)->cash.paid_amount==50);common.state().identity.tick=4;
 politics.hidden=true;assert(!validate_fiscal_settlement(common,economy,local,organizations,policies,taxes,fiscal,&read).ok);politics.hidden=false;
 auto wrong=fiscal;wrong.assessments.find(debt)->cash.original_due=99;EconomicTakeoverReadContext wrong_read{common,economy,local,&wrong,&taxes,nullptr,politics};assert(!validate_fiscal_settlement(common,economy,local,organizations,policies,taxes,wrong,&wrong_read).ok);
 // 原应付日期伪装成占领同tick，实际评税事实却在占领之后，仍不能获旧债身份。
 wrong=fiscal;auto late=fact("fiscal_source_assessed","fiscal_source_assessed","local_economy");auto replaced=*wrong.assessments.find(debt);replaced.id=late;replaced.cash.id=late+".cash";replaced.grain.id=late+".grain";wrong.assessments=decltype(wrong.assessments){};wrong.assessments.insert(late,replaced);wrong.periods.rows().front().second.source_assessment_ids={late};for(auto& row:wrong.payments.rows())row.second.assessment_id=late;politics.assessment=late;auto after_capture=validate_fiscal_settlement(common,economy,local,organizations,policies,taxes,wrong,&wrong_read);assert(!after_capture.ok&&after_capture.error.original.code=="FiscalAssessmentAfterOccupation");politics.assessment=debt;
 // 即使伪造金额总和与头都匹配，登记之后的旧债收款仍须拒绝。
 wrong=fiscal;auto id=fact("fiscal_payment","fiscal_payment","local_economy");wrong.payments.insert(id,{id,debt,4,1,0});wrong.assessments.find(debt)->cash.paid_amount+=1;auto forged=validate_fiscal_settlement(common,economy,local,organizations,policies,taxes,wrong,&wrong_read);assert(!forged.ok&&forged.error.original.code=="EconomicFiscalCollectionPaused");
 std::cout<<"fiscal retained debt: exact original due/cold state, occupied receipt ownership, paused real collection, no new policy, missing registration and forged post-capture payment PASS\n";
}
