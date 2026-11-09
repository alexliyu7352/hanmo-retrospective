// 三命令必须保持闭集，重复键与伪造资金/本人授权在领域入口就拒绝。
#include "resident/domains/estate/employment/estate_employment_boundary.hpp"
#include "resident/domains/estate/employment/estate_employment_profile.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace hanmo::resident;
int main(){using V=BoundaryValue;using D=V::Dict;std::vector<EstateEmploymentCommand> commands{EstateEmploymentOffer{"policy","worker"},EstateEmploymentRequestOffer{"policy"},EstateEmploymentDecision{"contract","accept"},EstateEmploymentDecision{"contract","request_reply"}};for(const auto& command:commands){const auto kind=estate_employment_command_kind(command);auto raw=encode_estate_employment_command(command);auto result=decode_estate_employment_command(kind,raw);assert(result.ok&&estate_employment_command_kind(result.value)==kind&&compare_boundary(raw,encode_estate_employment_command(result.value)).exact());auto altered=raw;altered.as<D>()->emplace_back("payer_wallet_id",V{"stolen"});assert(!decode_estate_employment_command(kind,altered).ok);altered=raw;altered.as<D>()->push_back(altered.as<D>()->front());assert(!decode_estate_employment_command(kind,altered).ok);}
 assert(!decode_estate_employment_command("estate_employment_decide",V{D{{"contract_id",V{"contract"}},{"decision",V{"worker_dead"}}}}).ok);assert(!decode_estate_employment_command("estate_employment_request_offer",V{D{{"policy_id",V{""}}}}).ok);assert(!decode_estate_employment_command("estate_employment_pay",V{D{}}).ok);
 std::vector<std::pair<StableId,StableId>> versions{{"dynamic_world","local_society_v1"},{"local_economy","1"}};assert(validate_estate_employment_layout(false,versions).ok&&!validate_estate_employment_layout(true,versions).ok);versions.emplace_back("estate_employment","1");assert(validate_estate_employment_layout(true,versions).ok&&!validate_estate_employment_layout(false,versions).ok);versions.back().second="2";assert(!validate_estate_employment_layout(true,versions).ok);versions.back().second="1";versions.push_back(versions.back());assert(!validate_estate_employment_layout(true,versions).ok);versions.pop_back();versions[1].second="6";assert(!validate_estate_employment_layout(true,versions).ok);
 // 复用原EstateReceipt字节，但只能绑定本域实际本人命令事实，不能借旧estate kind洗来源。
 EstateEmploymentState state;EstateEmploymentContract contract;contract.id="estate_employment:1";contract.terms.estate_id="estate";contract.terms.employer_actor_id="employer";contract.terms.worker_actor_id="worker";contract.offered_tick=3;state.contracts.insert(contract.id,contract);EstateReceipt receipt{"estate",contract.id,{}};
 assert(validate_estate_employment_receipt(state,"estate_employment_offer","employer",3,receipt).ok);assert(!validate_estate_employment_receipt(state,"estate_fund","employer",3,receipt).ok);assert(!validate_estate_employment_receipt(state,"estate_employment_offer","worker",3,receipt).ok);assert(!validate_estate_employment_receipt(state,"estate_employment_offer","employer",4,receipt).ok);
 state.contracts.find(contract.id)->offer_from_provider=true;assert(validate_estate_employment_receipt(state,"estate_employment_request_offer","worker",3,receipt).ok);assert(!validate_estate_employment_receipt(state,"estate_employment_offer","employer",3,receipt).ok);
 EstateEmploymentDecisionFact decision;decision.id="estate_employment_decision:2";decision.contract_id=contract.id;decision.actor_id="worker";decision.tick=3;decision.provider_declared=true;decision.meeting.emplace();decision.meeting->actor_id="employer";state.decisions.insert(decision.id,decision);receipt.fact_id=decision.id;
 assert(validate_estate_employment_receipt(state,"estate_employment_decide","employer",3,receipt).ok);assert(!validate_estate_employment_receipt(state,"estate_employment_decide","worker",3,receipt).ok);state.decisions.find(decision.id)->actor_id.clear();assert(!validate_estate_employment_receipt(state,"estate_employment_decide","employer",3,receipt).ok);receipt.fact_id="estate_employment_service:3";assert(!validate_estate_employment_receipt(state,"estate_employment_decide","employer",3,receipt).ok);
 // 全局Common头不能被旧Estate解码器误认；仅已有准确新载荷的显式重载可分流。
 EstateEmploymentState routed;EstateEmploymentContract rc;rc.id="estate_employment:1";routed.contracts.insert(rc.id,rc);
 auto header=boundary::obj({{"kind",V{"estate_employment_offered"}},{"domain",V{"estate_employment"}}});
 auto world=boundary::obj({{"estates",V{D{}}},{"actions",V{D{}}},{"facts",V{D{{rc.id,header}}}}});
 assert(!decode_estate_seed(world).ok);auto old=decode_estate_seed(world,&routed);assert(old.ok&&old.value.facts.empty());EstateEmploymentState absent;assert(!decode_estate_seed(world,&absent).ok);auto altered=world;*altered.find("facts")->find(rc.id)->find("domain")=V{"estate"};assert(!decode_estate_seed(altered,&routed).ok);altered=world;*altered.find("facts")->find(rc.id)->find("kind")=V{"estate_employment_unknown"};assert(!decode_estate_seed(altered,&routed).ok);altered=world;altered.find("facts")->find(rc.id)->as<D>()->emplace_back("paid",V{true});assert(!decode_estate_seed(altered,&routed).ok);
 altered=world;altered.find("facts")->find(rc.id)->as<D>()->emplace_back("kind",V{"estate_employment_offered"});assert(!decode_estate_seed(altered,&routed).ok);
 // 同tick两个合法事实不能互换命令回执，缓存键也不能冒用别人的请求序。
 CommonState common;common.request_sequences.insert("employer",2);common.request_sequences.insert("worker",2);common.history={{3,"talk","employer","result:5",{}},{3,"estate_employment_offer","employer","result:9",{}}};
 assert(estate_employment_receipt_actor(common,"employer:2","estate_employment_offer","result:9",3,"estate_employment:6").ok);
 for(const auto& id:{"estate_employment:4","estate_employment:5","estate_employment:9","estate_employment:10","estate_employment:06"})assert(!estate_employment_receipt_actor(common,"employer:2","estate_employment_offer","result:9",3,id).ok);
 for(const auto& key:{"worker:2","employer:3","employer:02","employer:0"})assert(!estate_employment_receipt_actor(common,key,"estate_employment_offer","result:9",3,"estate_employment:6").ok);
 assert(!estate_employment_receipt_actor(common,"employer:2","estate_employment_offer","result:9",4,"estate_employment:6").ok);
 std::cout<<"employment command fields, explicit layout and exact old-shape receipt binding PASS\n";
}
