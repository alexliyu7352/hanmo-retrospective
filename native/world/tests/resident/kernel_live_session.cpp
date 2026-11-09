// 真实单session的P2协议：纯报价无提交、旧请求回放、单步许可与恢复后旧身份拒绝。
#include "resident/kernel/boundary_dispatch.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
bool check(bool ok,const char* message){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<message<<'\n';}return ok;}
BoundaryValue read(const std::string& path){std::ifstream file(path);std::ostringstream text;text<<file.rdbuf();auto value=parse_boundary_json(text.str());if(!value.ok)throw std::runtime_error(value.error.original.code);return std::move(value.value);}
BoundaryValue object(std::initializer_list<std::pair<std::string,BoundaryValue>> fields){return BoundaryValue{BoundaryValue::Dict{fields}};}
bool okay(const BoundaryValue& result){auto* value=result.find("ok");return value&&value->as<bool>()&&*value->as<bool>();}
}
int main(int argc,char** argv){if(argc!=2)return 2;const std::string root=argv[1];auto index=read(root+"/oracle-index.json"),catalog=read(root+"/expanded-catalog.json");auto state=read(root+"/"+*index.find("initial_state")->find("file")->as<StableId>());
    auto session=create_resident_replay_session();if(!check(session->initialize(catalog,state).ok,"real install"))return 1;auto identity=session->identity().value;auto before=session->checkpoint();if(!check(before.ok,"initial checkpoint"))return 1;
    auto raw=*index.find("commands")->as<BoundaryValue::Array>()->front().find("command");
    LiveOperation preview{LiveOperationKind::preview,identity,object({{"actor_id",BoundaryValue{std::string{"actor.player"}}},{"kind",*raw.find("kind")},{"params",*raw.find("params")}})};
    auto quote=session->execute_live(preview);if(!check(quote.ok&&okay(quote.value.result),"pure live preview uses real core"))return 1;
    check(session->checkpoint().value==before.value,"preview does not publish or allocate IDs");
    LiveOperation command{LiveOperationKind::command,identity,raw};auto submitted=session->execute_live(command);if(!check(submitted.ok&&okay(submitted.value.result),"caller-bound command"))return 1;
    auto after=session->checkpoint();auto now=session->identity().value;check(now.revision==identity.revision+1&&now.tick==identity.tick,"travel starts without a halfday");
    auto repeated=session->execute_live(command);check(repeated.ok&&okay(repeated.value.result)&&compare_boundary(repeated.value.result,submitted.value.result).exact(),"old revision duplicate request still replays");check(session->checkpoint().value==after.value,"duplicate request never writes");
    auto stale=command;stale.source.execution_epoch+=100;auto denied=session->execute_live(stale);check(!denied.ok&&denied.error.original.code=="StaleWorld","wrong world epoch rejected");
    auto stale_preview=session->execute_live(preview);check(!stale_preview.ok&&stale_preview.error.original.code=="StalePreview","pure preview source revision rejected");
    auto context=session->execute_live({LiveOperationKind::query,now,object({{"query_id",BoundaryValue{std::string{"action_context"}}},{"args",object({})}})});if(!check(context.ok&&okay(context.value.result),"finite action context"))return 1;
    auto action=*context.value.result.find("action_id")->as<StableId>();auto parked=session->execute_live({LiveOperationKind::begin_run,now,object({{"run_id",BoundaryValue{std::string{"run:live"}}},{"actor_id",BoundaryValue{std::string{"actor.player"}}},{"action_id",BoundaryValue{action}},{"max_half_days",BoundaryValue{Integer{4}}},{"max_gross_outflow",BoundaryValue{Integer{100000}}}})});
    check(parked.ok&&okay(parked.value.result),"begin real run");check(session->checkpoint().value==after.value,"begin parks without changing world");
    LiveOperation grant{LiveOperationKind::grant,now,object({{"run_id",BoundaryValue{std::string{"run:live"}}},{"permit_seq",BoundaryValue{Integer{1}}},{"grant_half_days",BoundaryValue{Integer{1}}}})};auto stepped=session->execute_live(grant);check(stepped.ok&&okay(stepped.value.result)&&session->identity().value.tick==now.tick+1,"one real halfday grant");
    auto saved=session->checkpoint();auto again=session->execute_live(grant);check(again.ok&&okay(again.value.result)&&session->checkpoint().value==saved.value,"duplicate grant no extra advance");
    auto excessive=grant;*excessive.payload.find("grant_half_days")=BoundaryValue{Integer{2}};auto invalid=session->execute_live(excessive);check(!invalid.ok&&invalid.error.original.code=="InvalidRunPermit"&&session->checkpoint().value==saved.value,"live UI cannot grant multiple steps");
    auto cancel=session->execute_live({LiveOperationKind::cancel_run,now,object({{"run_id",BoundaryValue{std::string{"run:live"}}}})});check(cancel.ok&&okay(cancel.value.result)&&*cancel.value.result.find("stop")->as<StableId>()=="cancelled","cancel ignores stale revision after committed step");
    check(session->restore(saved.value).ok,"same world restores once");auto old=session->execute_live(command);check(!old.ok&&old.error.original.code=="StaleWorld","pre-restore live envelope rejected");
    std::cout<<"LIVE_SESSION_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;
}
