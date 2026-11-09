// 原真实生前交接→新首长提名/本人答复；历史控制与辖区仅具名typed隔离源，未冒World运行。
#define POLITICAL_HEAD_HANDOVER_RECEIPTS_FIXTURE_ONLY
#include "political_head_handover_receipts_test.cpp"
#include "resident/domains/politics/successor_rounds/political_successor_round_rules.hpp"
namespace {
struct RoundFixture:HandoverReceiptFixture {
 PoliticalSuccessorRoundCatalog round_catalog;PoliticalSuccessorRoundState rounds;PoliticalSuccessionAccessionState old_accessions;StableId installed;
 RoundFixture(){catalog.offices.find("chief")->grant_ids.push_back("next.round");state.offices.find("chief")->grant_ids.push_back("next.round");Grant g;g.id="next.round";g.office_id="chief";g.scopes={{ScopeRef::Kind::site,"hall"}};g.allowed_command_kinds={political_successor_round_nominate_command,political_successor_round_withdraw_command};catalog.grants.insert(g.id,g);round_catalog.policies.insert("next",{"next","succession","next.round",4,8});}
 // 同一Common/原组织/两旧政治账，不生成另一份Office来源。
 PoliticalHeadSourceContext heads(){return {common,state,catalog,policies,nominations,old_accessions,&handover_catalog,&handovers};}
 PoliticalSuccessorRoundContext context(const PoliticalHeadSourceContext& h){return {common,*cc,state,catalog,economy,activity,h,round_catalog,rounds,{},[](const PoliticalSuccessionPolicy&,Integer,const StableId&){return common_success();},[this](Integer,const StableId& id)->Outcome<StableId>{auto i=historical_players.find(id);return i==historical_players.end()?Outcome<StableId>{false,{},common_failure("FixturePlayerSourceMissing").error}:Outcome<StableId>{true,i->second,{}};}};}
 void install(){auto offer=commit("leader",offer_command(),1);installed=commit("candidate",PoliticalAnswerHeadHandover{offer,true,0},1);check(valid().ok,"original live handover permanent results validate");}
 PoliticalNominateNextSuccessor propose(){return {{"city","chief",1,"candidate"},"next","leader",installed,1};}
 Outcome<PoliticalSuccessorRoundNominationQuote> preview(const StableId& actor,const PoliticalSuccessorRoundNominationCommand& command){auto h=heads();auto x=context(h);return preview_political_successor_round_nomination(x,actor,command);}
 Status validate_rounds(){auto h=heads();return validate_political_successor_round_nominations(context(h));}
 StableId submit_round(const StableId& actor,const PoliticalSuccessorRoundNominationCommand& command){common.state().metadata.player_actor_id=actor;auto h=heads();auto x=context(h);auto quote=take(preview_political_successor_round_nomination(x,actor,command));auto id=take(apply_political_successor_round_nomination_in_candidate(common,rounds,x,actor,command,quote));historical_players[id]=actor;check(validate_rounds().ok,"actual new round source validates with original Office unchanged");return id;}
};
}
#ifndef POLITICAL_SUCCESSOR_ROUND_FIXTURE_ONLY
int main(){try{
 RoundFixture f;auto initial=f.propose();initial.authority={"city","chief",0,"leader"};initial.expected_office_epoch=0;initial.candidate_actor_id="candidate";check(!f.preview("leader",initial).ok,"original author still uses original first-round制度");f.install();auto n=f.submit_round("candidate",f.propose());const auto* issued=f.rounds.nominations.find(n);check(issued->head_source_fact_id==f.installed&&issued->office_epoch==1&&f.state.offices.find("chief")->holder_actor_id==std::optional<StableId>{"candidate"},"new nomination freezes actual head source without handing over Office");auto yes=f.submit_round("leader",PoliticalAnswerNextSuccession{n,true});check(f.rounds.answers.find(yes)->accepted,"candidate personally accepts next generation willingness");auto no=f.submit_round("leader",PoliticalAnswerNextSuccession{n,false});check(f.rounds.answers.find(no)->previous_acceptance_fact_id==yes&&!f.preview("leader",PoliticalAnswerNextSuccession{n,true}).ok,"explicit renunciation permanently binds earlier real acceptance");auto again=f.submit_round("candidate",f.propose());f.submit_round("candidate",PoliticalWithdrawNextSuccession{{"city","chief",1,"candidate"},again});check(f.state.offices.find("chief")->epoch==1,"withdrawal never changes installed Office");
 RoundFixture stale;stale.install();auto wrong=stale.propose();wrong.expected_head_source_fact_id.clear();check(!stale.preview("candidate",wrong).ok,"today's holder alone cannot replace exact appointment source");
 RoundFixture expired;expired.install();auto offered=expired.submit_round("candidate",expired.propose());expired.common.state().identity.tick=4;expired.common.state().metadata.player_actor_id="leader";check(!expired.preview("leader",PoliticalAnswerNextSuccession{offered,true}).ok,"expired original request cannot revive");
 RoundFixture grant;grant.install();grant.catalog.grants.find("next.round")->valid_until=0;auto limited=grant.submit_round("candidate",grant.propose());grant.common.state().identity.tick=1;grant.common.state().metadata.player_actor_id="leader";check(!grant.preview("leader",PoliticalAnswerNextSuccession{limited,true}).ok,"acceptance rechecks dedicated Grant at actual later tick");
 auto saved=f.rounds.nominations.find(n)->head_source_fact_id;f.rounds.nominations.find(n)->head_source_fact_id=again;check(!f.validate_rounds().ok,"future new-round nomination cannot replace original appointment source");f.rounds.nominations.find(n)->head_source_fact_id=saved;
 auto h=f.heads();auto x=f.context(h);auto bad=h;bad.handovers=nullptr;bad.handover_catalog=nullptr;auto missing=f.context(bad);check(!validate_political_successor_round_nominations(missing).ok,"missing original completed head ledger rejects cold history");
 auto encoded=encode_political_successor_round_catalog(f.round_catalog);check(compare_boundary(encoded,encode_political_successor_round_catalog(take(decode_political_successor_round_catalog(encoded)))).exact(),"independent policy codec preserves strict original references");
 std::cout<<checks<<" new-round assertions; source fixture only\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
#endif
