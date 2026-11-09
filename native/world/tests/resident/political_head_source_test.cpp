// 新轮来源隔离门复用真实生前交接writer；未绑定部分用具名typed提交来源，不冒完整World运行。
#define POLITICAL_HEAD_HANDOVER_RECEIPTS_FIXTURE_ONLY
#include "political_head_handover_receipts_test.cpp"
#include "resident/domains/politics/successor_rounds/political_head_source.hpp"
namespace {
// 只借原初始与真实生前交接账；后继政策还未注册或启用。
struct HeadSourceFixture:HandoverReceiptFixture {
 PoliticalSuccessionNominationState nominations_for_head;PoliticalSuccessionAccessionState accessions_for_head;
 PoliticalHeadSourceContext head_context(){return {common,state,catalog,policies,nominations_for_head,accessions_for_head,&handover_catalog,&handovers};}
 StableId boundary(){auto id=take(common.next_id("political_successor_round_nomination"));take(common.append_fact({id,"political_successor_round_nominated",political_successor_rounds_domain}));return id;}
};
}
int main(){try{
 HeadSourceFixture f;auto initial=take(read_political_head_source(f.head_context(),"succession"));check(initial&&initial->actor_id=="leader"&&initial->origin==PoliticalHeadOrigin::authored&&initial->source_fact_id.empty(),"unchanged original office retains authored root");auto before=f.boundary();auto offered=f.commit("leader",f.offer_command(),1);auto accepted=f.commit("candidate",PoliticalAnswerHeadHandover{offered,true,0},1);auto* row=f.handovers.answers.find(accepted);check(f.valid().ok,"original full receipt boundary is proved before thin read");
 auto current=take(read_political_head_source(f.head_context(),"succession"));check(current&&current->actor_id=="candidate"&&current->predecessor_actor_id=="leader"&&current->office_epoch==1&&current->origin==PoliticalHeadOrigin::living_handover&&current->source_fact_id==accepted,"actual accepted source identifies installed successor");auto old=take(read_political_head_source_before(f.head_context(),"succession",0,before));check(old&&old->actor_id=="leader"&&old->office_epoch==0,"same tick later actual handover does not rewrite earlier new-round boundary");auto after=f.boundary();auto later=take(read_political_head_source_before(f.head_context(),"succession",0,after));check(later&&later->actor_id=="candidate","actual completed source is available only after original Office change");
 check(!read_political_head_source_before(f.head_context(),"succession",0,accepted).ok,"old acceptance is not an arbitrary new-round boundary");check(!read_political_head_source_before(f.head_context(),"succession",1,after).ok,"stable read rejects future clock");
 auto* change=f.state.facts.find(row->office_change_fact_id);const auto causes=change->cause_fact_ids;change->cause_fact_ids.clear();check(!read_political_head_source(f.head_context(),"succession").ok,"current holder alone is not legitimate succession source");change->cause_fact_ids=causes;row->accepted=false;check(!read_political_head_source(f.head_context(),"succession").ok,"declined offer cannot become installed source");row->accepted=true;
 auto without=f.head_context();without.handovers=nullptr;without.handover_catalog=nullptr;check(!read_political_head_source(without,"succession").ok,"missing enabled handover ledger cannot fall back to original holder");
 const auto saved=f.common.state().facts;auto h=*f.common.state().facts.find(after);f.common.state().facts.erase(after);f.common.state().facts.insert_at(0,after,h);check(!read_political_head_source_before(f.head_context(),"succession",0,after).ok,"Common-only moving boundary across installed change violates allocation side");f.common.state().facts=saved;
 auto vacant=take(f.common.next_id("organization"));take(f.common.append_fact({vacant,"organization_office_changed","organization"}));OrganizationFact v;v.id=vacant;v.kind="organization_office_changed";v.organization_id="city";v.office_id="chief";v.tick=f.common.tick();f.state.facts.insert(vacant,v);auto* office=f.state.offices.find("chief");office->holder_actor_id.reset();office->status=OfficeStatus::vacant;++office->epoch;auto empty=take(read_political_head_source(f.head_context(),"succession"));check(!empty,"real vacancy supplies no authority and does not restore earlier head");
 std::cout<<checks<<" source assertions; syntax-only until authorized behavior window\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
