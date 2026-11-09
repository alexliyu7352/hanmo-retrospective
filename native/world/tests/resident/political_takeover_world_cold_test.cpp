// 实际自然approved银行的独立坏档门；只改隔离候选，不覆盖原银行或生产目录。
#define main borrowed_political_journey_main
#include "political_takeover_journey.cpp"
#undef main
#include "resident/kernel/world_codec.hpp"
#include "resident/kernel/native_initial_state.hpp"
#include <functional>
namespace {
WorldBundle bundle(const V& catalog,const Bytes& bank){auto selected=take(inspect_world_checkpoint_selection(bank),"original title");const auto& raw=field(selected,"catalog");CatalogIdentity expected;expected.content_id=text(raw,"content_id");expected.content_version=text(raw,"content_version");expected.content_digest=text(raw,"content_digest");expected.ruleset_id=text(raw,"ruleset_id");expected.schema_sha256=text(raw,"schema_sha256");expected.artifact_sha256=text(raw,"artifact_sha256");if(raw.find("pack_set_digest"))expected.pack_set_digest=text(raw,"pack_set_digest");for(const auto& row:boundary::array(field(raw,"module_versions"),"modules"))expected.module_versions.emplace_back(text(row,"name"),text(row,"version"));auto prepared=take(prepare_native_catalog(catalog,text(selected,"scenario_id"),expected),"original prepared catalog");WorldBundle world(prepared,take(decode_world_checkpoint(bank,*prepared),"original full bank"));take(world.validate(),"original complete World");return WorldBundle(world);}
}
int main(int argc,char** argv){try{
 check(argc==4,"usage: political_takeover_world_cold_test PREPARED APPROVED_BANK FRESH_OUTPUT");const auto catalog=read(argv[1]);const auto bank=read_bank(argv[2]);const fs::path out=argv[3];check(!fs::exists(out),"fresh output required");fs::create_directories(out);auto original=bundle(catalog,bank);auto session=create_resident_replay_session();take(session->initialize_checkpoint(catalog,bank),"original session");check(!original.political_takeover.registrations.empty()&&!original.political_takeover.approvals.empty()&&!original.political_takeover.decisions.empty(),"natural approved source required");A results;
 auto reject=[&](const StableId& name,const std::function<void(WorldBundle&)>& mutate){WorldBundle candidate(original);mutate(candidate);auto encoded=encode_world_checkpoint(candidate);if(!encoded.ok){const StableId expected=name=="economic sidecar version mismatch"?"EconomicTakeoverExecutionVersionMismatch":name=="task sidecar version mismatch"?"UnsupportedTaskTakeoverExecutionVersion":"UnsupportedPoliticalTakeoverVersion";check(name.find("version mismatch")!=StableId::npos&&encoded.error.original.code==expected,"only exact domain version rejection may precede writing "+name+":"+encoded.error.original.code);check(take(session->checkpoint(),"unchanged after encoder refusal")==bank,"encoder refusal leaves original World");results.push_back(boundary::obj({{"case",V{name}},{"stage",V{"checkpoint writer"}},{"code",V{encoded.error.original.code}}}));return;}check(name.find("version mismatch")==StableId::npos,"paired version rejected by writer");auto cold=create_resident_replay_session();auto restored=cold->initialize_checkpoint(catalog,encoded.value);check(!restored.ok&&restored.error.original.code!="AlreadyInstalled","new Session cold must reject actual corrupt source "+name);check(take(session->checkpoint(),"unchanged session after "+name)==bank,"independent bad cold leaves original World untouched "+name);results.push_back(boundary::obj({{"case",V{name}},{"code",V{restored.error.original.code}}}));};
 const auto registration=original.political_takeover.registrations.rows().front().first;const auto approval=original.political_takeover.approvals.rows().front().first;const auto decision=original.political_takeover.decisions.rows().front().first;
 reject("missing entire registration",[&](auto& w){w.political_takeover.registrations.erase(registration);});
 reject("future registration",[&](auto& w){w.political_takeover.registrations.find(registration)->tick=w.common.tick()+1;});
 reject("wrong occupation source",[&](auto& w){w.political_takeover.registrations.find(registration)->occupation_fact_id=approval;});
 reject("removed original responsibilities",[&](auto& w){w.political_takeover.registrations.find(registration)->responsibilities.clear();});
 reject("missing approval payload",[&](auto& w){w.political_takeover.approvals.erase(approval);});
 reject("missing decision payload",[&](auto& w){w.political_takeover.decisions.erase(decision);});
 reject("redirected payment",[&](auto& w){w.political_takeover.decisions.find(decision)->decision.payment_wallet_id=w.common.actor(player)->wallet_id;});
 reject("forged private beneficiary",[&](auto& w){w.political_takeover.decisions.find(decision)->decision.original_beneficiaries.push_back({SubjectKind::actor,player});});
 reject("fake approval header",[&](auto& w){w.common.state().facts.find(approval)->kind="political_public_responsibilities_pending";});
 reject("political version mismatch",[&](auto& w){w.political_takeover.version=2;});
 reject("economic sidecar version mismatch",[&](auto& w){w.economic_takeover_execution.version=2;});
 reject("task sidecar version mismatch",[&](auto& w){check(bool(w.tasks.takeover_execution),"paired Task sidecar present");w.tasks.takeover_execution->version=2;});
 check(take(encode_world_checkpoint(original),"unchanged original source")==bank,"original source not edited by bad candidates");write(out/"result.json",boundary::obj({{"ok",V{true}},{"cases",V{results}},{"checks",V{Integer(checks)}}}));std::cout<<"POLITICAL_WORLD_COLD checks="<<checks<<" cases="<<results.size()<<" failures=0\n";return 0;
 }catch(const std::exception& error){std::cerr<<"POLITICAL_WORLD_COLD_FAIL "<<error.what()<<'\n';return 1;}}
