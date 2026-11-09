// 新回应profile的正式Session消费者；仅在正常作者安装输入与同源新生产库就绪后运行。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/storage/common_codec.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;namespace fs=std::filesystem;
// Release同样保留检查，任何入口拒绝立即保存原失败与已经提交的命令轨迹。
void require(bool yes,const StableId& why){if(!yes)throw std::runtime_error(why);}
template<class T>T must(Outcome<T> value,const StableId& why){require(value.ok,why+": "+value.error.original.code);return std::move(value.value);}
StableId json(const V& value){return must(render_boundary_json(value),"encode evidence");}
V read(const fs::path& path){std::ifstream input(path);require(bool(input),"read "+path.string());std::ostringstream raw;raw<<input.rdbuf();return must(parse_boundary_json(raw.str()),"parse input");}
void write(const fs::path& path,const V& value){std::ofstream out(path);require(bool(out),"write "+path.string());out<<json(value)<<'\n';}
bool ok(const V& value){const auto* item=value.find("ok");return item&&item->as<bool>()&&*item->as<bool>();}
// 每次调用沿当前完整来源，不从全世界导出补造按钮权限。
V query(ResidentReplaySession& world,const StableId& id,const V& args=V{V::Dict{}}){LiveOperation op;op.source=must(world.identity(),"query identity");op.payload=boundary::obj({{"query_id",V{id}},{"args",args}});return must(world.execute_live(op),"query "+id).result;}
V payload(ResidentReplaySession& world,const StableId& kind,const V& params){const auto context=query(world,"command_context");return boundary::obj({{"kind",V{kind}},{"params",params},{"actor_id",boundary::field(context,"actor_id","context")},{"request_seq",boundary::field(context,"next_request_seq","context")},{"expected_revision",boundary::field(context,"revision","context")}});}
// 外层世界提交仍可分配原回执编号；只把领域钱物、时间和承诺当作须保持不变的内容。
V submit(ResidentReplaySession& world,const StableId& kind,const V& params,V::Array& trace){LiveOperation op;op.kind=LiveOperationKind::command;op.source=must(world.identity(),"command identity");op.payload=payload(world,kind,params);const auto result=must(world.execute_live(op),"command "+kind).result;trace.push_back(boundary::obj({{"command",op.payload},{"receipt",result}}));require(ok(result),"command refused "+kind+": "+json(result));return result;}
V preview(ResidentReplaySession& world,const StableId& kind,const V& params){const auto context=query(world,"command_context");LiveOperation op;op.kind=LiveOperationKind::preview;op.source=must(world.identity(),"preview identity");op.payload=boundary::obj({{"kind",V{kind}},{"params",params},{"actor_id",boundary::field(context,"actor_id","context")}});return must(world.execute_live(op),"preview "+kind).result;}
// 下列完整状态只作验证oracle，不用来创建或修改待测世界。
V state(ResidentReplaySession& world){return must(world.export_state(),"export evidence");}
const V::Array& responses(const V& world){return boundary::array(boundary::field(boundary::field(world,"task_offer_responses","world"),"responses","responses"),"responses");}
void no_domain_effect(const V& before,const V& after){for(const auto* key:{"tick","actors","parties","wallets","inventories","actions","facts","rng","tasks","dynamic_tasks","relations","knowledge"})require(compare_boundary(boundary::field(before,key,"before"),boundary::field(after,key,"after")).exact(),StableId{"response changed "}+key);}
// 按真实say/choice推进；重要action永不由此函数代确认。
void choice(ResidentReplaySession& world,V::Array& trace){for(int n=0;n<6;++n){auto view=query(world,"interaction_view");const auto phase=boundary::get_string(view,"phase","interaction");if(phase=="choice")return;require(phase=="say","expected say/choice: "+json(view));submit(world,"dialogue_next",V{V::Dict{}},trace);}throw std::runtime_error("dialogue choice bound");}
void choose(ResidentReplaySession& world,const StableId& option,V::Array& trace){choice(world,trace);submit(world,"dialogue_choose",boundary::obj({{"option_id",V{option}}}),trace);}
void meet(ResidentReplaySession& world,V::Array& trace){auto view=query(world,"interaction_view");if(view.find("context"))submit(world,"end_interaction",V{V::Dict{}},trace);submit(world,"talk",boundary::obj({{"role_id",V{"contacts::role.sender"}}}),trace);}
// 当前有限页是accept/decline参数唯一来源，不能自己填provider或远方context。
V action(ResidentReplaySession& world,const StableId& offer,const StableId& kind){const auto page=query(world,"interaction_tasks");for(const auto& row:boundary::array(boundary::field(page,"rows","page"),"rows"))if(boundary::get_string(row,"id","row")==offer)for(const auto& item:boundary::array(boundary::field(row,"actions","row"),"actions"))if(boundary::get_string(item,"command","action")==kind){require(*boundary::field(item,"available","action").as<bool>(),"current action unavailable: "+json(item));return boundary::field(item,"params","action");}throw std::runtime_error("missing current task action "+kind);}
// 保原native字节并在新Session恢复，绝不把导出JSON当同一银行重建。
Bytes save(ResidentReplaySession& world,const fs::path& path){auto bytes=must(world.checkpoint(),"checkpoint");std::ofstream out(path,std::ios::binary);require(bool(out),"bank open");out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));require(bool(out),"bank write");return bytes;}
void restore(std::unique_ptr<ResidentReplaySession>& world,const V& catalog,const Bytes& bytes){auto cold=create_resident_replay_session();auto loaded=cold->initialize_checkpoint(catalog,bytes);require(loaded.ok,"native restore: "+loaded.error.original.code);require(bytes==must(cold->checkpoint(),"restored checkpoint"),"native bytes changed on cold restore");world=std::move(cold);}
// 只消费原受领Action的实际剩余半日；本门不继续送信、旅行或领取报酬。
void finish_acceptance(ResidentReplaySession& world,V::Array& trace){for(int n=0;n<4;++n){auto value=query(world,"action_context");auto* id=value.find("action_id");if(!id||!id->as<StableId>()||id->as<StableId>()->empty())return;submit(world,"advance",V{V::Dict{}},trace);}throw std::runtime_error("acceptance action bound");}
}
// 调用者须传正常Catalog/NativeRequest；旧库明确unsupported属于缺前置，不改输入伪造成功。
int main(int argc,char** argv){fs::path output;V::Array trace;try{
    require(argc==4,"usage: task_response_live_probe CATALOG NATIVE_REQUEST OUTPUT");output=argv[3];fs::create_directories(output);const auto catalog=read(argv[1]);auto world=create_resident_replay_session();auto initialized=world->initialize_native(catalog,read(argv[2]));require(initialized.ok,"native initialization: "+initialized.error.original.code);
    const auto initial=state(*world);require(responses(initial).empty(),"new profile starts with invented responses");
    submit(*world,"move_local",boundary::obj({{"place_id",V{"society::place.school"}}}),trace);meet(*world,trace);
    auto unknown=query(*world,"interaction_tasks");require(boundary::array(boundary::field(unknown,"rows","page"),"rows").empty(),"author catalog leaked as personal knowledge");
    const auto context=boundary::field(query(*world,"interaction_view"),"context","interaction");const auto before_unknown=must(world->checkpoint(),"unknown preview checkpoint");
    V::Array unknown_quotes;for(const auto* id:{"contacts::offer.letter","contacts::offer.nonexistent"}){auto quote=preview(*world,"decline_offer",boundary::obj({{"offer_id",V{id}},{"context",context}}));require(!ok(quote)&&boundary::get_string(quote,"code","quote")=="SourceUnknown","unknown-source oracle: "+json(quote));unknown_quotes.push_back(std::move(quote));}
    require(before_unknown==must(world->checkpoint(),"after unknown preview"),"rejected preview changed world");write(output/"unknown-previews.json",V{std::move(unknown_quotes)});
    for(const auto* ask:{"ask_letter","ask_escort"}){choose(*world,ask,trace);const auto phase=boundary::get_string(query(*world,"interaction_view"),"phase","interaction");if(phase=="action")submit(*world,"dialogue_confirm",V{V::Dict{}},trace);else require(phase=="say","unexpected discovery phase");}
    choose(*world,"leave",trace);require(responses(state(*world)).empty(),"ordinary leave invented refusal");meet(*world,trace);
    choose(*world,"decline_letter",trace);require(boundary::get_string(query(*world,"interaction_view"),"phase","interaction")=="action"&&responses(state(*world)).empty(),"important choice bypassed confirmation");
    auto pending=save(*world,output/"pending-response.hmcb");restore(world,catalog,pending);auto quote=preview(*world,"dialogue_confirm",V{V::Dict{}});require(ok(quote)&&boundary::get_int(quote,"duration","quote")==0,"decline confirmation not immediate: "+json(quote));write(output/"confirm-preview.json",quote);
    const auto before=state(*world);submit(*world,"dialogue_confirm",V{V::Dict{}},trace);auto declined=state(*world);no_domain_effect(before,declined);require(responses(declined).size()==1&&boundary::get_string(responses(declined).front(),"offer_id","response")=="contacts::offer.letter","response applied to another promise");
    const auto first_response=boundary::field(declined,"task_offer_responses","world");auto declined_page=query(*world,"interaction_tasks");write(output/"declined-page.json",declined_page);bool caption=false;for(const auto& row:boundary::array(boundary::field(declined_page,"rows","page"),"rows")){const auto id=boundary::get_string(row,"id","row");if(id=="contacts::offer.letter")caption=row.find("response_text")!=nullptr;else require(!row.find("response_text"),"another offer falsely marked declined");}require(caption,"personal response missing from actual page");
    const auto bank=save(*world,output/"declined.hmcb");restore(world,catalog,bank);require(compare_boundary(first_response,boundary::field(state(*world),"task_offer_responses","world")).exact(),"response history changed after restore");
    auto repeated=action(*world,"contacts::offer.letter","decline_offer");const auto before_repeat=state(*world);submit(*world,"decline_offer",repeated,trace);const auto after_repeat=state(*world);no_domain_effect(before_repeat,after_repeat);require(compare_boundary(first_response,boundary::field(after_repeat,"task_offer_responses","world")).exact(),"repeat response rewrote its original source");
    choose(*world,"leave",trace);meet(*world,trace);const auto revisited=query(*world,"interaction_tasks");const auto pure=must(world->checkpoint(),"before page read");require(compare_boundary(revisited,query(*world,"interaction_tasks")).exact()&&pure==must(world->checkpoint(),"after page read"),"revisit page mutated world");write(output/"revisited-page.json",revisited);
    submit(*world,"accept_offer",action(*world,"contacts::offer.letter","accept_offer"),trace);finish_acceptance(*world,trace);meet(*world,trace);auto accepted_page=query(*world,"interaction_tasks");write(output/"accepted-page.json",accepted_page);bool accepted=false;for(const auto& row:boundary::array(boundary::field(accepted_page,"rows","page"),"rows"))if(boundary::get_string(row,"id","row")=="contacts::offer.letter")accepted=boundary::get_string(row,"status","row")=="active";require(accepted,"temporary refusal blocked original acceptance");
    require(compare_boundary(first_response,boundary::field(state(*world),"task_offer_responses","world")).exact(),"acceptance erased original response history");save(*world,output/"accepted-after-response.hmcb");write(output/"accepted-manifest.json",query(*world,"task_travel_manifest"));
    // 地图只复用这次实际受领源；回报地点仍是原学舍，不把到县城当作已亲手交信。
    const auto route=query(*world,"task_route_obligations",boundary::obj({{"destination_settlement_id",V{"society::settlement.qingchuan"}}}));write(output/"accepted-route-obligations.json",route);bool report=false;for(const auto& row:boundary::array(boundary::field(route,"rows","route"),"rows"))if(boundary::get_string(row,"place_id","row")=="society::place.school")report=boundary::get_string(row,"deadline_scope","row")=="report_completion";require(report,"accepted message report deadline not visible at original place");
    write(output/"commands.json",V{trace});write(output/"result.json",boundary::obj({{"ok",V{true}},{"ordinary_leave_is_not_refusal",V{true}},{"explicit_confirmation",V{true}},{"domain_effect_free",V{true}},{"native_cold_restore",V{true}},{"later_original_acceptance",V{true}},{"ui_or_gui_tested",V{false}}}));std::cout<<"task_response_live_probe: explicit personal response, confirmation, native restore and later acceptance passed\n";return 0;
}catch(const std::exception& error){if(!output.empty()){write(output/"failed-commands.json",V{trace});write(output/"failure.json",boundary::obj({{"ok",V{false}},{"error",V{StableId(error.what())}}}));}std::cerr<<error.what()<<'\n';return 1;}}
