// 财政窄门调用真实CommonStore、Office授权、政策历史与codec；不模拟征税收入。
#include "politics_fiscal.hpp"
#include "resident/kernel/politics_fiscal_integration.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include <cstdlib>
#include <iostream>
using namespace hanmo::resident;
namespace {
Integer checks=0;
// Release也保留断言及具名失败出口。
void check(bool value,const char* reason){++checks;if(!value){std::cerr<<"FAIL: "<<reason<<'\n';std::exit(1);}}
// 任何非预期业务拒绝都携带真实码退出。
template<class T>T need(Outcome<T> value){if(!value.ok){std::cerr<<value.error.original.code<<" "<<value.error.original.path<<'\n';std::exit(1);}return std::move(value.value);}
// 独立财政夹具无战斗，个人行动仍由真实CommonStore核验。
struct Activities final:ActivityRead{bool in_open_battle(const StableId&)const override{return false;}bool in_interaction(const StableId&,const StableId& = {})const override{return false;}};
// 一个真实地方Office、公钱公粮及分开的私人钱包；税源账故意不属于本域。
struct Fixture {
    std::shared_ptr<CommonCatalog> cc=std::make_shared<CommonCatalog>();CommonStore common{CommonState{},cc};
    OrganizationCatalog oc;OrganizationState org;PoliticsState politics;PoliticalFiscalState fiscal;PoliticalFiscalCatalog catalog{true};Activities activities;
    Fixture(){
        cc->organization_ids={"realm"};cc->places.insert("seat",{"seat","county",{}});cc->places.insert("other",{"other","county",{}});
        for(const auto& id:{StableId{"official"},StableId{"deputy"}}){ActorRecord actor;actor.id=id;actor.alive=actor.available=true;actor.hp=100;actor.birth_tick=-18*720;actor.location=SettlementLocation{"county","seat"};actor.wallet_id=id+".wallet";actor.inventory_id=id+".inventory";common.state().actors.insert(id,actor);common.state().wallets.insert(actor.wallet_id,{actor.wallet_id,{Owner::Kind::actor,id,{}},17,{}});common.state().inventories.insert(actor.inventory_id,{actor.inventory_id,{Owner::Kind::actor,id,{}},100,0,{}});}
        common.state().wallets.insert("treasury",{"treasury",{Owner::Kind::organization,"realm",{}},91,{}});common.state().inventories.insert("granary",{"granary",{Owner::Kind::organization,"realm",{}},100,0,{}});
        oc.enabled=true;OrganizationRecord realm;realm.id="realm";realm.kind="political";realm.headquarters_site="seat";realm.office_ids={"office"};oc.organizations.insert(realm.id,realm);
        OfficeState office;office.id="office";office.organization_id="realm";office.holder_actor_id="official";office.deputy_actor_id="deputy";office.grant_ids={"grant"};oc.offices.insert(office.id,office);
        Grant grant;grant.id="grant";grant.office_id=office.id;grant.allowed_command_kinds={"political_set_tax"};grant.scopes={{ScopeRef::Kind::site,"seat"}};grant.wallet_ids={"treasury"};grant.inventory_ids={"granary"};grant.amount_limit=100;grant.continuity_policy_id="continue_under_organization";oc.grants.insert(grant.id,grant);org=need(make_initial_organization(common,oc));
        Jurisdiction j;j.id="county";j.seat_place_id="seat";j.political_organization_id=j.controlling_organization_id="realm";j.office_id="office";j.public_wallet_id="treasury";j.public_inventory_ids={"granary"};politics.jurisdictions.insert(j.id,j);
    }
    // 命令每次绑定当前真实Office与政策revision。
    PoliticalSetTaxCommand command(Integer revision=0,FiscalTaxMode mode=FiscalTaxMode::lenient)const{return {{"realm","office",0,"official"},"county","granary",0,revision,mode};}
    // 直接使用S5将消费的真实只读实现。
    Outcome<std::optional<FiscalPolicySnapshot>> read(Integer due,const StableId& site="seat")const{return FiscalPolicyAccess{common,*cc,politics,fiscal,catalog}.fiscal_policy_for_due_tick(site,due);}
    // 应用沿原公权而非直接插入财政记录。
    Outcome<PoliticalFiscalEffects> apply(const PoliticalSetTaxCommand& command){return apply_political_set_tax(common,politics,fiscal,catalog,org,oc,activities,command.authority.actor_id,command);}
    // 冷候选核历史签发者及事实来源。
    Status valid()const{return validate_political_fiscal(common,*cc,politics,fiscal,catalog,org,oc);}
};
// 无政策、延后生效、按旧due_tick重查及换任均保留真实历史。
void history_and_authority(){
    Fixture f;check(!need(f.read(0)),"enabled capability alone does not levy normal tax");
    check(!f.read(1).ok&&!f.read(-1).ok&&!f.read(0,"unknown").ok,"future, negative and unknown assessments refused");
    auto first=need(f.apply(f.command()));check(first.policy_revision==1&&first.effective_from_tick==1&&!need(f.read(0)),"policy begins next tick only");need(f.valid());
    auto stale=f.apply(f.command());check(!stale.ok&&stale.error.original.code=="StaleFiscalPolicy","same tick policy revision cannot be overwritten");
    f.common.state().identity.tick=1;auto frozen=need(f.read(1));check(frozen&&frozen->mode==FiscalTaxMode::lenient&&frozen->source_fact_id==first.policy_fact_id,"first due freezes actual source");
    auto second=need(f.apply(f.command(1,FiscalTaxMode::surcharge)));check(need(f.read(1))->source_fact_id==first.policy_fact_id,"later decision never alters existing due policy");
    f.common.state().identity.tick=2;check(need(f.read(2))->source_fact_id==second.policy_fact_id&&need(f.read(1))->mode==FiscalTaxMode::lenient,"historical and present policies coexist");
    need(invalidate_office(f.common,f.org,f.oc,"office",2,second.policy_fact_id));need(f.valid());
    check(f.org.offices.find("office")->holder_actor_id==std::optional<StableId>{"deputy"}&&need(f.read(1))->source_fact_id==first.policy_fact_id,"real deputy succession preserves old fiscal policy");
    auto old=f.apply(f.command(2));check(!old.ok&&old.error.original.code=="OrganizationAuthorityDenied","old signer loses new policy power immediately");
    auto next=f.command(2,FiscalTaxMode::normal);next.authority={"realm","office",1,"deputy"};auto third=need(f.apply(next));need(f.valid());
    check(third.policy_revision==3&&f.common.wallet("official.wallet")->balance==17&&f.common.wallet("deputy.wallet")->balance==17&&f.common.wallet("treasury")->balance==91,"policy change never mints tax or debits private wallets");
    auto* record=f.fiscal.policies.find(first.policy_fact_id);record->issuer_actor_id="deputy";check(!f.valid().ok,"cannot replace historical signer with current officeholder");record->issuer_actor_id="official";record->authority_epoch=1;check(!f.valid().ok,"same tick later authority cannot backdate an earlier policy");record->authority_epoch=0;need(f.valid());
}
// 钱粮范围须同Grant覆盖；所有拒绝保持共同与财政状态原样。
void refusal_is_atomic(){
    Fixture f;auto before=encode_common_state(f.common.state());auto policy=encode_political_fiscal_state(f.fiscal);
    f.catalog.enabled=false;check(!f.apply(f.command()).ok&&!need(f.read(0)),"disabled fiscal capability does not create policy");f.catalog.enabled=true;
    auto* grant=f.oc.grants.find("grant");Grant split=*grant;split.id="split";split.wallet_ids.clear();grant->inventory_ids.clear();f.oc.grants.insert(split.id,split);f.org.offices.find("office")->grant_ids.push_back(split.id);
    auto denied=f.apply(f.command());check(!denied.ok&&denied.error.original.code=="FiscalGrantIncomplete","cannot splice treasury and granary grants");
    grant=f.oc.grants.find("grant");grant->inventory_ids={"granary"};
    auto wrong=f.command();wrong.public_inventory_id="official.inventory";check(!f.apply(wrong).ok,"private stock cannot be public tax recipient");
    f.common.state().actors.find("official")->location=SettlementLocation{"county","other"};denied=f.apply(f.command());check(!denied.ok&&denied.error.original.code=="FiscalPolicyRequiresLocalAuthority","remote authority requires real delivery workflow");f.common.state().actors.find("official")->location=SettlementLocation{"county","seat"};
    check(compare_boundary(before,encode_common_state(f.common.state())).exact()&&compare_boundary(policy,encode_political_fiscal_state(f.fiscal)).exact(),"all denied commands preserve state and global counter");
}
// 检查点不重签发政策；未知枚举、断裂来源和丢失revision拒绝。
void cold_history(){
    Fixture f;auto first=need(f.apply(f.command()));f.common.state().identity.tick=1;need(f.apply(f.command(1,FiscalTaxMode::normal)));need(f.valid());
    const auto before=encode_political_fiscal_state(f.fiscal);auto decoded=need(decode_political_fiscal_state(before));check(compare_boundary(before,encode_political_fiscal_state(decoded)).exact(),"strict JSON preserves fiscal history");
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);need(visit_political_fiscal_checkpoint(writer,f.fiscal));need(writer.finish());PoliticalFiscalState cold;const auto& input=bytes;CodecVisitor reader(input);need(visit_political_fiscal_checkpoint(reader,cold));need(reader.finish());check(compare_boundary(before,encode_political_fiscal_state(cold)).exact(),"binary fiscal cold state is exact");
    auto bad=before;std::get<BoundaryValue::Dict>(bad.data).emplace_back("fallback_normal",BoundaryValue{true});check(!decode_political_fiscal_state(bad).ok,"unknown defaults rejected");
    auto* record=f.fiscal.policies.find(first.policy_fact_id);record->snapshot.mode=static_cast<FiscalTaxMode>(9);check(!f.valid().ok&&!f.read(1).ok,"invalid fiscal mode cannot reach economy");record->snapshot.mode=FiscalTaxMode::lenient;
    record->snapshot.source_fact_id="invented";check(!f.valid().ok&&!f.read(1).ok,"missing actual source prevents assessment");record->snapshot.source_fact_id=first.policy_fact_id;
    record->snapshot.policy_revision=7;check(!f.valid().ok,"revision gaps rejected");record->snapshot.policy_revision=1;need(f.valid());
    auto fact=political_fiscal_dynamic_fact(f.fiscal,first.policy_fact_id);check(fact&&fact->kind=="political_fiscal_policy_changed"&&fact->tick==0,"dynamic fact describes policy decision, never collected tax");
}
}
// 真实命令边界与治所视图复验完整选择，缓存载荷仍由中央getter接入。
void business_and_view(){
    Fixture f;PoliticalFiscalWriteContext w{f.common,*f.cc,f.politics,f.fiscal,f.catalog,f.org,f.oc,f.activities};
    auto command=f.command();auto encoded=encode_political_set_tax(command);auto decoded=need(decode_political_set_tax(encoded));check(compare_boundary(encoded,encode_political_set_tax(decoded)).exact(),"tax command strict roundtrip");
    std::get<BoundaryValue::Dict>(encoded.data).emplace_back("collect_now",BoundaryValue{true});check(!decode_political_set_tax(encoded).ok,"tax policy command cannot smuggle immediate collection");
    auto quote=need(preview_political_fiscal_business(w.read(),"official",command));auto cold=need(decode_political_fiscal_quote(encode_political_fiscal_quote(quote)));check(compare_boundary(encode_political_fiscal_quote(quote),encode_political_fiscal_quote(cold)).exact(),"fiscal quote exact boundary");
    auto changed=command;changed.mode=FiscalTaxMode::surcharge;const auto before=encode_common_state(f.common.state());check(!apply_political_fiscal_business(w,"official",changed,quote).ok&&compare_boundary(before,encode_common_state(f.common.state())).exact(),"quote cannot switch lenient to surcharge");
    auto effects=need(apply_political_fiscal_business(w,"official",command,quote));need(validate_political_fiscal_receipt(w.read(),"official",0,effects));check(!validate_political_fiscal_receipt(w.read(),"deputy",0,effects).ok,"fiscal receipt cannot borrow another signer");
    check(compare_boundary(encode_political_fiscal_effects(effects),encode_political_fiscal_effects(need(decode_political_fiscal_effects(encode_political_fiscal_effects(effects))))).exact(),"fiscal effects strict roundtrip");
    auto view=need(query_political_fiscal_entries(f.common,*f.cc,f.politics,f.fiscal,f.catalog,f.org,f.oc,f.activities,"official","seat"));const auto& rows=boundary::array(boundary::field(view,"rows","/"),"/rows");check(rows.size()==1&&boundary::get_string(rows[0],"source_fact_id","/row")==effects.policy_fact_id,"local official reads actual latest policy source");
    auto private_view=need(query_political_fiscal_entries(f.common,*f.cc,f.politics,f.fiscal,f.catalog,f.org,f.oc,f.activities,"deputy","seat"));check(boundary::array(boundary::field(private_view,"rows","/"),"/rows").empty(),"unappointed deputy cannot inspect fiscal authority controls");
    check(!query_political_fiscal_entries(f.common,*f.cc,f.politics,f.fiscal,f.catalog,f.org,f.oc,f.activities,"official","other").ok,"fiscal query cannot inspect remote site");
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);need(visit_political_fiscal_effects(writer,effects));need(writer.finish());PoliticalFiscalEffects restored;const auto& input=bytes;CodecVisitor reader(input);need(visit_political_fiscal_effects(reader,restored));need(reader.finish());check(compare_boundary(encode_political_fiscal_effects(effects),encode_political_fiscal_effects(restored)).exact(),"fiscal effects binary exact");
}
// 世界财政阶段仍停在from，显式阶段读口只允许原单步to，不修改共同tick。
void phase_boundary(){
    Fixture f;auto source=need(f.apply(f.command()));DynamicStepInput step;step.from_tick=0;step.to_tick=1;
    FiscalPolicyAccess phase{f.common,*f.cc,f.politics,f.fiscal,f.catalog,step};auto policy=need(phase.fiscal_policy_for_due_tick("seat",1));
    check(policy&&policy->source_fact_id==source.policy_fact_id&&f.common.tick()==0,"real from-to phase can assess next tick without mutating world time");
    check(!f.read(1).ok&&!phase.fiscal_policy_for_due_tick("seat",2).ok,"ordinary read and phase read both reject beyond own boundary");
    step.to_tick=2;check(!FiscalPolicyAccess{f.common,*f.cc,f.politics,f.fiscal,f.catalog,step}.fiscal_policy_for_due_tick("seat",1).ok,"phase cannot silently widen to multiple future ticks");
    step.from_tick=1;step.to_tick=2;check(!FiscalPolicyAccess{f.common,*f.cc,f.politics,f.fiscal,f.catalog,step}.fiscal_policy_for_due_tick("seat",1).ok,"phase from must match current candidate");
    f.common.state().identity.tick=1;check(!phase.fiscal_policy_for_due_tick("seat",1).ok,"old phase access cannot be carried into another world step");
    f.fiscal.policies.erase(source.policy_fact_id);check(!f.valid().ok,"orphan common policy fact rejected on cold candidate");
}
// 缓存getter夹具对接真实财政薄叶，防同tick两次合法决定互换回执。
void cached_receipts(){
    Fixture f;PoliticalFiscalWriteContext w{f.common,*f.cc,f.politics,f.fiscal,f.catalog,f.org,f.oc,f.activities};std::map<StableId,PoliticalFiscalEffects> frozen;Integer sequence=0;
    auto record=[&](const PoliticalSetTaxCommand& command){auto quote=need(preview_political_fiscal_business(w.read(),"official",command));auto effects=need(apply_political_fiscal_business(w,"official",command,quote));auto result=std::make_shared<CommitResult>();result->kind="political_set_tax";result->tick=f.common.tick();result->revision=++f.common.state().identity.revision;result->result_id=need(f.common.next_id("result"));f.common.state().request_sequences.set("official",++sequence);f.common.state().history.push_back({result->tick,result->kind,"official",result->result_id,{}});f.common.state().results.insert("official:"+std::to_string(sequence),result);frozen[result->result_id]=effects;return result->result_id;};
    auto first=record(f.command());auto second=record(f.command(1,FiscalTaxMode::surcharge));PoliticalFiscalReceiptReader get=[&](const CommitResult& result)->const PoliticalFiscalEffects*{auto it=frozen.find(result.result_id);return it==frozen.end()?nullptr:&it->second;};need(validate_political_fiscal_business(w.read(),true,get));
    std::swap(frozen[first],frozen[second]);auto swapped=validate_political_fiscal_business(w.read(),true,get);check(!swapped.ok&&swapped.error.original.code=="PoliticalFiscalReceiptAllocationMismatch","same-tick valid fiscal receipts cannot swap command allocation");std::swap(frozen[first],frozen[second]);
    check(!validate_political_fiscal_business(w.read(),false,get).ok,"fiscal capability identity must match actual state");frozen.erase(first);check(!validate_political_fiscal_business(w.read(),true,get).ok,"missing typed receipt payload cannot silently survive cold restore");
}
// S5消费读口本身核真实目的公库，不依赖UI或调用顺序替它挡住损坏映射。
void public_destination_integrity(){
    Fixture f;need(f.apply(f.command()));f.common.state().identity.tick=1;
    f.common.state().wallets.find("treasury")->owner={Owner::Kind::actor,"official",{}};check(!f.read(1).ok,"fiscal read rejects treasury turned into actor private account");f.common.state().wallets.find("treasury")->owner={Owner::Kind::organization,"realm",{}};
    f.common.state().inventories.find("granary")->owner={Owner::Kind::organization,"other",{}};check(!f.read(1).ok,"fiscal read rejects foreign public stock");f.common.state().inventories.find("granary")->owner={Owner::Kind::organization,"realm",{}};
    f.politics.jurisdictions.find("county")->public_wallet_id="replacement";check(!f.read(1).ok,"historical snapshot cannot silently redirect to changed jurisdiction treasury");f.politics.jurisdictions.find("county")->public_wallet_id="treasury";
    need(f.read(1));f.politics.jurisdictions.find("county")->controlling_organization_id="occupier";auto occupation=f.read(1);check(!occupation.ok&&occupation.error.kind==ErrorKind::unsupported_capability&&occupation.error.original.code=="PoliticalFiscalOccupationUnsupported","unimplemented occupation cannot silently levy stable normal tax or pretend no policy");
}
// 本运行仅证明政策端真实规则及S5读接缝；实际税粮结算与世界中央登记另有验收。
int main(int argc,char** argv){if(argc==2&&std::string(argv[1])=="--codec-only"){cold_history();business_and_view();std::cout<<"political fiscal codec PASS: "<<checks<<" checks; exact shared authority wire, strict command/quote and cold policy values\n";return 0;}history_and_authority();refusal_is_atomic();cold_history();business_and_view();phase_boundary();cached_receipts();public_destination_integrity();std::cout<<"political fiscal PASS: "<<checks<<" checks; real authority, next-tick history, absent policy, old signer, no asset transfer, exact cold state\n";}
