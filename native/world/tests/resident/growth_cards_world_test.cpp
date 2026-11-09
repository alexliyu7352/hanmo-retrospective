// 正式安装目录的卡片完整World冷边界；两个进程分别生产和消费原银行，不替代Main或GUI。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/checkpoint/checkpoint_envelope.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;using namespace boundary;Integer checks=0;
// Release也保留每个正式边界的错误信息。
void check(bool value,const StableId& label){++checks;if(!value)throw std::runtime_error(label);}
template<class T>T must(Outcome<T> value,const StableId& label){check(value.ok,label+": "+value.error.original.code+" "+value.error.original.path);return std::move(value.value);}
void must(Status value,const StableId& label){check(value.ok,label+": "+value.error.original.code+" "+value.error.original.path);}
// 只读指定原件；输出由调用者指定独立证据目录，不覆盖旧银行。
StableId read(const StableId& path){std::ifstream file(path,std::ios::binary);check(bool(file),"read "+path);std::ostringstream result;result<<file.rdbuf();check(!file.bad(),"complete read "+path);return result.str();}
void write(const StableId& path,const StableId& bytes){std::ifstream prior(path,std::ios::binary);check(!prior,"evidence path must be fresh "+path);std::ofstream file(path,std::ios::binary);file.write(bytes.data(),bytes.size());check(bool(file),"complete write "+path);}
StableId json(const V& value){return must(render_boundary_json(value),"render JSON");}
V parsed(const StableId& path){return must(parse_boundary_json(read(path)),"parse "+path);}
// 工厂必须为正式核心；不安装任何简化替身。
std::unique_ptr<ResidentReplaySession> session(){auto result=create_resident_replay_session();check(bool(result),"production session factory");return result;}
void absent(ResidentReplaySession& world){check(world.identity().error.original.code=="NoWorld","failure leaves no identity");check(world.checkpoint().error.original.code=="NoWorld","failure leaves no bank");check(world.export_state().error.original.code=="NoWorld","failure leaves no state");}
// 查询仍走公开身份guard，跨局已见只影响展示。
V page(ResidentReplaySession& world,bool discovered=false){LiveOperation call;call.kind=LiveOperationKind::query;call.source=must(world.identity(),"query identity");V::Array ids;if(discovered)ids.push_back(V{"lifecards::card.settle_breath"});call.payload=obj({{"query_id",V{"growth_cards"}},{"args",obj({{"offset",V{Integer(0)}},{"limit",V{Integer(32)}},{"discovered",V{ids}}})}});return must(world.execute_live(call),"actual card query").result;}
// 当前真实新情景具逐课、区域生活、具名组织和S0共同注册；精确声明growth10。
std::vector<CheckpointDomain> domains(){return {{"common",7},{"travel",3},{"military",4},{"interaction",4},{"social",1},{"estate",2},{"tasks",1},{"family",1},{"personal",1},{"growth",10},{"local_economy",3},{"organization",3},{"affairs",2},{"director",4},{"dynamic_tasks",2},{"dynamic_social",2},{"player_plan",1}};}
// 冷消费者核完整值和银行，不靠若干卡片字段掩盖其他领域变化。
void exact(ResidentReplaySession& world,const StableId& state,const Bytes& bytes){check(json(must(world.export_state(),"full export"))==state,"all exported state bytes unchanged");check(must(world.checkpoint(),"full checkpoint")==bytes,"all typed checkpoint bytes unchanged");}
// 重签封装摘要后的坏marker必须在标题及安装前被拒，活世界也保持原值。
void marker_rejections(ResidentReplaySession& live,const V& catalog,const Bytes& bytes,const StableId& state){const auto envelope=must(inspect_checkpoint(bytes,domains()),"exact full domain versions");for(const auto* kind:{"missing","duplicate","wrong"}){auto bad=envelope;auto& modules=bad.identity.catalog.module_versions;auto found=std::find_if(modules.begin(),modules.end(),[](const auto& row){return row.first=="growth_cards";});check(found!=modules.end(),"explicit card marker present");if(StableId{kind}=="missing")modules.erase(found);else if(StableId{kind}=="duplicate")modules.push_back(*found);else found->second="999";const auto altered=must(encode_checkpoint(bad),"encode resigned bad marker");auto cold=session();auto title=cold->inspect_checkpoint_selection(altered);check(!title.ok&&title.error.original.code=="CheckpointProfileMismatch",StableId{"title rejects card marker "}+kind+" actual "+title.error.original.code);auto install=cold->initialize_checkpoint(catalog,altered);check(!install.ok,"bad card marker cannot install");absent(*cold);const auto before=must(live.identity(),"identity before failed restore");check(!live.restore(altered).ok,"bad marker cannot replace live world");const auto after=must(live.identity(),"identity after failed restore");check(before.world_id==after.world_id&&before.execution_epoch==after.execution_epoch&&before.revision==after.revision&&before.tick==after.tick,"failed restore keeps complete identity");exact(live,state,bytes);}}
// 正式完整JSON不能漏掉显式卡片块，拒绝后不得部分安装。
void missing_block(const V& catalog,const StableId& state){auto bad=must(parse_boundary_json(state),"parse bad-input copy");auto& fields=*bad.as<V::Dict>();const auto before=fields.size();fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& row){return row.first=="growth_cards";}),fields.end());check(fields.size()+1==before,"actual state has one explicit growth_cards block");auto cold=session();auto result=cold->initialize(catalog,bad);check(!result.ok,"missing enabled card block rejected: "+result.error.original.code);absent(*cold);}
}
// producer接实际installed reader输出；consumer在另一进程只读原银行和同源目录。
int main(int argc,char** argv){try{check(argc==5,"Usage: growth_cards_world_test produce|consume PREPARED REQUEST EVIDENCE_DIR");const StableId mode=argv[1],dir=argv[4];const auto catalog=parsed(argv[2]),request=parsed(argv[3]);check(get_bool(catalog,"ok","")&&catalog.find("pack_set_digest"),"actual installed prepared pack set required");check(get_string(request,"scenario_id","")=="lifecards::scenario.qingchuan_card_life","explicit authored card scenario");auto live=session();
    if(mode=="produce"){must(live->initialize_native(catalog,request),"formal native card initialization");const auto state=json(must(live->export_state(),"full initial state"));const auto bytes=must(live->checkpoint(),"initial full bank");const auto view=page(*live);check(get_int(view,"total","")==2&&array(field(view,"cards",""),"").size()==2,"two actual authored card rows");for(const auto& row:array(field(view,"cards",""),""))check(!get_bool(row,"learned","")&&!get_bool(row,"acquired_this_world",""),"untrained apprentice owns no initial ability or card");const auto discovered=page(*live,true);for(const auto& row:array(field(discovered,"cards",""),""))check(!get_bool(row,"learned","")&&!get_bool(row,"acquired_this_world",""),"cross-world seen input grants no current capability");exact(*live,state,bytes);auto title=session();must(title->inspect_checkpoint_selection(bytes),"uninstalled title inspects real card bank");absent(*title);must(title->initialize_checkpoint(catalog,bytes),"fresh-session cold restore");exact(*title,state,bytes);check(json(page(*title))==json(view),"restored query exactly preserved");marker_rejections(*live,catalog,bytes,state);missing_block(catalog,state);write(dir+"/initial.json",state);write(dir+"/initial.hmcheckpoint",StableId(bytes.begin(),bytes.end()));write(dir+"/view.json",json(view));write(dir+"/prepared.json",json(catalog));write(dir+"/request.json",json(request));}
    else{check(mode=="consume","known mode");const auto raw=read(dir+"/initial.hmcheckpoint");Bytes bytes(raw.begin(),raw.end());must(live->inspect_checkpoint_selection(bytes),"new process title inspection");absent(*live);must(live->initialize_checkpoint(catalog,bytes),"new process installs original full bank");exact(*live,read(dir+"/initial.json"),bytes);check(json(page(*live))==read(dir+"/view.json"),"new process query identical");}
    std::cout<<"GROWTH_CARDS_WORLD "<<mode<<" "<<checks<<" checks 0 failures\n";return 0;
}catch(const boundary::DecodeFailure& error){std::cerr<<"GROWTH_CARDS_WORLD FAIL "<<error.error.original.code<<" "<<error.error.original.path<<"\n";return 1;}catch(const std::exception& error){std::cerr<<"GROWTH_CARDS_WORLD FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}}
