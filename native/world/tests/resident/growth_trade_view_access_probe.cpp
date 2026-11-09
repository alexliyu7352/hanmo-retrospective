// 正式商户作者输入的读边界：远方库存拒绝，既有交易与本人装货收据保持可读。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/storage/common_codec.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;Integer checks=0;
void check(bool value,const StableId& message){++checks;if(!value)throw std::runtime_error(message);}
template<class T>T take(Outcome<T> result,const StableId& label){if(!result.ok)throw std::runtime_error(label+":"+result.error.original.code);return std::move(result.value);}
V read(const std::filesystem::path& path){std::ifstream f(path);if(!f)throw std::runtime_error("missing actual author input");std::ostringstream s;s<<f.rdbuf();return take(parse_boundary_json(s.str()),"parse");}
Outcome<LiveOperationResult> query(ResidentReplaySession& world,const StableId& id,const V& args=V{V::Dict{}}){LiveOperation op;op.source=take(world.identity(),"query identity");op.payload=boundary::obj({{"query_id",V{id}},{"args",args}});return world.execute_live(op);}
bool ok(const V& value){auto* v=value.find("ok");return v&&v->as<bool>()&&*v->as<bool>();}
V command(ResidentReplaySession& world,const StableId& kind,const V& params=V{V::Dict{}},bool preview=false){auto context=take(query(world,"command_context"),"context").result;LiveOperation op;op.kind=preview?LiveOperationKind::preview:LiveOperationKind::command;op.source=take(world.identity(),"command identity");op.payload=boundary::obj({{"kind",V{kind}},{"params",params},{"actor_id",boundary::field(context,"actor_id","context")}});if(!preview){op.payload.as<V::Dict>()->emplace_back("request_seq",boundary::field(context,"next_request_seq","context"));op.payload.as<V::Dict>()->emplace_back("expected_revision",boundary::field(context,"revision","context"));}auto result=take(world.execute_live(op),kind).result;if(!ok(result))throw std::runtime_error(kind+":"+take(render_boundary_json(result),"receipt"));return result;}
void finish(ResidentReplaySession& world){for(int i=0;i<8;++i){auto a=take(query(world,"action_context"),"action").result;if(boundary::get_string(a,"action_id","action").empty())return;command(world,"advance");}throw std::runtime_error("action did not finish");}
V args(const StableId& shop="society::shop.grain",const StableId& provider="society::actor.merchant",const StableId& action=""){return boundary::obj({{"shop_id",V{shop}},{"provider_actor_id",V{provider}},{"receipt_action_id",V{action}}});}
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("usage: probe actual-catalog actual-request");auto world=create_resident_replay_session();auto initialized=world->initialize_native(read(argv[1]),read(argv[2]));if(!initialized.ok)throw std::runtime_error("initialize:"+initialized.error.original.code);
    auto bank=take(world->checkpoint(),"before refused read");auto far=query(*world,"trade_view",args());check(!far.ok,"far shop inventory must not be visible by guessed shop ID");check(bank==take(world->checkpoint(),"after refused read"),"refused view must preserve bank bytes");
    command(*world,"move_local",boundary::obj({{"place_id",V{"society::place.grain"}}}));finish(*world);check(!query(*world,"trade_view",args()).ok,"same place without current trade contact is not a shop conversation");
    command(*world,"talk",boundary::obj({{"role_id",V{"society::role.merchant"}}}));command(*world,"dialogue_next");command(*world,"dialogue_choose",boundary::obj({{"option_id",V{"trade"}}}));command(*world,"dialogue_confirm");auto session=take(query(*world,"interaction_view"),"trade session").result;check(boundary::get_string(session,"phase","session")=="trade","actual merchant dialogue opens trade");
    auto page=take(query(*world,"trade_view",args()),"current shop view").result;check(!boundary::array(boundary::field(page,"items","page"),"items").empty(),"current shop inventory remains readable");check(!query(*world,"trade_view",args("society::shop.grain","society::actor.tutor")).ok,"guessed provider cannot read current shop");check(!query(*world,"trade_view",args("society::shop.sword_counter")).ok,"merchant conversation cannot read another shop");
    auto request=boundary::obj({{"shop_id",V{"society::shop.grain"}},{"item_id",V{"society::item.millet"}},{"quantity",V{Integer{1}}},{"side",V{"buy"}},{"payer_wallet_id",boundary::field(page,"personal_wallet_id","page")},{"context",boundary::field(session,"context","session")}});auto quoted=command(*world,"trade",request,true);request.as<V::Dict>()->emplace_back("quoted_total",boundary::field(quoted,"total","quote"));request.as<V::Dict>()->emplace_back("market_revision",boundary::field(quoted,"market_revision","quote"));command(*world,"trade",request);auto action=take(query(*world,"action_context"),"own trade action").result;auto id=boundary::get_string(action,"action_id","action");check(!id.empty(),"real purchase has an unfinished own action");auto pending=take(query(*world,"trade_view",args("society::shop.grain","society::actor.merchant",id)),"own loading view").result;check(boundary::boolean(boundary::field(pending,"receipt_action_pending","page"),"pending"),"own loading receipt remains readable under busy contact");finish(*world);
    bank=take(world->checkpoint(),"before current pure view");take(query(*world,"trade_view",args()),"completed trade view");check(bank==take(world->checkpoint(),"after current pure view"),"valid view preserves exact bank bytes");command(*world,"close_trade");check(!query(*world,"trade_view",args()).ok,"closed trade session cannot retain stock access");
    std::cout<<"GROWTH_TRADE_VIEW_ACCESS "<<checks<<" checks 0 failures; actual author commands; no state injection\n";return 0;
}catch(const std::exception& e){std::cerr<<"GROWTH_TRADE_VIEW_ACCESS "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
