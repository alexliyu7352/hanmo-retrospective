// 正式1.4目录的有限共享边界消费者；真人决定走原生命令，坏来源只进入独立冷候选。
#include "resident/kernel/boundary_dispatch.hpp"
#include "resident/kernel/native_initial_state.hpp"
#include "resident/kernel/world_codec.hpp"
#include "resident/domains/organization/membership_offer.hpp"
#include <picosha2.h>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace hanmo::resident;
namespace {
using V=BoundaryValue;
Integer checks=0;

// 发布构建也保留断言及具体首错，不把NDEBUG下的空执行当作通过。
void require(bool condition,const StableId& label){++checks;if(!condition)throw std::runtime_error(label);}
// 保留领域首错及路径，失败时不改写成笼统测试错误。
StableId error_text(const RuleError& error){return error.original.code+" at "+error.original.path;}
// 失败不得读取默认载荷；成功才移动原生产结果。
template<class T>T must(Outcome<T> result,const StableId& label){require(result.ok,label+": "+error_text(result.error));return std::move(result.value);}
// 无载荷的生产状态也须明确成功，不默许安装或编码失败。
void must(Status result,const StableId& label){require(result.ok,label+": "+error_text(result.error));}
// 小型命令对象保调用者字段顺序，不承载另一份游戏状态。
V object(std::initializer_list<std::pair<StableId,V>> fields){return V{V::Dict{fields}};}
// 拒绝缺ok或类型不符的DTO，不把外层调用成功视作规则成功。
bool okay(const V& value){const auto* ok=value.find("ok");return ok&&ok->as<bool>()&&*ok->as<bool>();}
// 只用于身份摘要和失败诊断，绝不用于热世界重建。
StableId text(const V& value){return must(render_boundary_json(value),"Render diagnostic");}
// 往返同时核对值及原字段顺序，不能只比序列化后的无序对象。
void same(const V& expected,const V& actual,const StableId& label){require(compare_boundary(expected,actual).exact(),label);}
// 输入必须是真实Catalog.read_prepared产物，不能包装作者JSON冒充准入结果。
V read_catalog(const StableId& path){std::ifstream input(path,std::ios::binary);require(bool(input),"Cannot open prepared catalog: "+path);std::ostringstream bytes;bytes<<input.rdbuf();require(!input.bad(),"Cannot read prepared catalog");auto value=must(parse_boundary_json(bytes.str()),"Parse prepared catalog");require(okay(value)&&value.find("data"),"Catalog.read_prepared output required; raw content index is not accepted");return value;}

// 正式情景由实际命名空间解析；不猜演员ID，也不生成测试专用目录或开局。
StableId scenario_id(const V& catalog,const StableId& suffix){const auto& data=boundary::field(catalog,"data","catalog");StableId found;for(const auto& row:boundary::object(boundary::field(data,"scenarios","data"),"scenarios"))if(row.first.size()>=suffix.size()&&row.first.compare(row.first.size()-suffix.size(),suffix.size(),suffix)==0){require(found.empty(),"Ambiguous formal scenario "+suffix);found=row.first;}require(!found.empty(),"Missing formal 1.4 scenario "+suffix);return found;}

// 与原生冷入口一致地计算完整身份，缺模块或错误目录摘要不能用测试默认值补齐。
CatalogIdentity catalog_identity(const V& catalog,const StableId& scenario){
    const auto& data=boundary::field(catalog,"data","catalog");const auto& selected=boundary::field(boundary::field(data,"scenarios","data"),scenario,"scenarios");const auto& dynamic=boundary::field(selected,"dynamic_world",scenario);
    require(dynamic.find("regional_life")&&boundary::get_bool(boundary::field(dynamic,"organization","dynamic_world"),"named_membership_enabled","organization"),"Formal scenario lacks named membership and regional life");
    CatalogIdentity id;id.content_id=boundary::get_string(catalog,"content_id","catalog");id.content_version=boundary::get_string(catalog,"content_version","catalog");id.content_digest=boundary::get_string(catalog,"content_digest","catalog");id.schema_sha256=boundary::get_string(catalog,"schema_sha256","catalog");id.ruleset_id="hanmo.native.local_society/1";id.artifact_sha256=picosha2::hash256_hex_string(text(catalog));if(const auto* digest=catalog.find("pack_set_digest"))id.pack_set_digest=boundary::string(*digest,"pack_set_digest");
    const bool population=dynamic.find("population_military"),siege=dynamic.find("siege_integration");id.module_versions={{"dynamic_world","local_society_v1"},{"local_economy","3"},{"organization","3"},{"affairs","1"},{"director","3"}};if(population)id.module_versions.emplace_back("population_military",siege?"2":"1");id.module_versions.emplace_back("regional_life","1");if(siege)id.module_versions.emplace_back("siege_integration","1");id.module_versions.emplace_back("named_membership","1");return id;
}
// 请求逐项携带真实身份，包括可选包锁；不放宽冷目录匹配。
V identity_value(const CatalogIdentity& id){V::Array modules;for(const auto& row:id.module_versions)modules.push_back(object({{"name",V{row.first}},{"version",V{row.second}}}));auto value=object({{"content_id",V{id.content_id}},{"content_version",V{id.content_version}},{"content_digest",V{id.content_digest}},{"schema_sha256",V{id.schema_sha256}},{"ruleset_id",V{id.ruleset_id}},{"artifact_sha256",V{id.artifact_sha256}},{"module_versions",V{std::move(modules)}}});if(id.pack_set_digest)value.as<V::Dict>()->emplace_back("pack_set_digest",V{*id.pack_set_digest});return value;}

// Fixture只持有正式session和只读typed快照；所有正向行为都经生产live入口。
struct Fixture {
    const V& catalog;StableId scenario;std::shared_ptr<const PreparedCatalog> prepared;std::unique_ptr<ResidentReplaySession> session;
    // 只从正式目录及其既定玩家创建新局，不覆盖初态资源或社会关系。
    Fixture(const V& input,const StableId& suffix):catalog(input),scenario(scenario_id(input,suffix)),session(create_resident_replay_session()){
        require(bool(session),"Resident session factory missing");auto identity=catalog_identity(catalog,scenario);prepared=must(prepare_native_catalog(catalog,scenario,identity),"Admit unchanged formal catalog");const auto& selected=boundary::field(boundary::field(boundary::field(catalog,"data","catalog"),"scenarios","data"),scenario,"scenarios");
        auto request=object({{"scenario_id",V{scenario}},{"world_seed",boundary::field(selected,"seed_text",scenario)},{"founder_selection",boundary::field(selected,"player_actor",scenario)},{"expected_catalog_identity",identity_value(identity)}});must(session->initialize_native(catalog,request),"Initialize native formal membership world");
    }
    // 解码副本只能观察或用于负向冷准入，绝不回写活世界。
    std::unique_ptr<WorldBundle> snapshot(){auto value=must(session->export_state(),"Export actual world");auto seed=must(decode_world_seed(value,*prepared),"Decode actual world");auto world=std::make_unique<WorldBundle>(prepared,std::move(seed));must(world->validate(),"Validate actual world");return world;}
    // 查询使用当前实际world和epoch；来源陈旧由生产入口拒绝。
    V query(const StableId& id,V args){auto identity=must(session->identity(),"Query identity");auto result=must(session->execute_live({LiveOperationKind::query,identity,object({{"query_id",V{id}},{"args",std::move(args)}})}),"Live query "+id);require(okay(result.result),"Live query rejected: "+text(result.result));return std::move(result.result);}
    // 真实当前人物、序号与版本来自原生世界；NPC身份只用于明确的负向越权测试。
    V raw_command(const StableId& kind,V params,const StableId& explicit_actor={}){auto world=snapshot();const auto actor=explicit_actor.empty()?world->common.state().metadata.player_actor_id:explicit_actor;const auto* sequence=world->common.state().request_sequences.find(actor);require(sequence,"Actor has no command sequence");return object({{"kind",V{kind}},{"params",std::move(params)},{"actor_id",V{actor}},{"request_seq",V{*sequence+1}},{"expected_revision",V{world->common.revision()}}});}
    // 正向行为只有当前玩家可提交；失败立即保留原始回执停止该用例。
    V command(const StableId& kind,V params){auto raw=raw_command(kind,std::move(params));auto identity=must(session->identity(),"Command identity");auto result=must(session->execute_live({LiveOperationKind::command,identity,raw}),"Live command "+kind);require(okay(result.result),"Live command rejected: "+text(result.result));return std::move(result.result);}
    // 场所来自真实位置解析，不把测试指定地点当作接洽权限。
    V entries(){auto world=snapshot();auto where=must(world->common.resolved_location(world->common.state().metadata.player_actor_id),"Player location");const auto* place=std::get_if<SettlementLocation>(&where);require(place,"Formal player must remain at a real local place");return query("named_membership_entries",object({{"place_id",V{place->place_id}}}));}
    // 只消费正式有限投影实际给出的按钮，不在测试中绕过接洽、知情或本人选择。
    std::optional<V> action(const StableId& kind,const StableId& parameter={},const StableId& expected={}){auto view=entries();for(const auto& row:boundary::array(boundary::field(view,"rows","membership"),"rows"))for(const auto& candidate:boundary::array(boundary::field(row,"actions","row"),"actions")){if(boundary::get_string(candidate,"command","action")!=kind||!boundary::get_bool(candidate,"available","action"))continue;const auto& params=boundary::field(candidate,"params","action");if(!parameter.empty()&&boundary::get_string(params,parameter,"params")!=expected)continue;return candidate;}return {};}
    // 原样提交有限按钮的命令与条款，不替换它冻结的目标或来源。
    V click(const V& action){return command(boundary::get_string(action,"command","action"),boundary::field(action,"params","action"));}
};

// 正向回执必须逐值逐字段序往返；旧共同schema明确拒绝新variant而不是错读尾部。
void receipt_roundtrip(const V& json){
    auto decoded=must(decode_resident_receipt(json),"Decode actual membership receipt");same(json,must(encode_resident_receipt({true,decoded,{}}),"Encode actual membership receipt"),"Actual JSON receipt changed on roundtrip");
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);writer.schema_features=common_checkpoint::typed_resource_references|common_checkpoint::named_membership_receipts;must(visit_resident_receipt(writer,decoded),"Write actual typed membership receipt");must(writer.finish(),"Finish typed receipt");
    std::shared_ptr<const CommitResult> recovered;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes));reader.schema_features=writer.schema_features;must(visit_resident_receipt(reader,recovered),"Read actual typed membership receipt");must(reader.finish(),"Finish typed receipt read");same(json,must(encode_resident_receipt({true,recovered,{}}),"Encode recovered receipt"),"Typed receipt changed values or field order");
    std::shared_ptr<const CommitResult> unsupported;CodecVisitor old_reader(static_cast<const CodecVisitor::Buffer&>(bytes));old_reader.schema_features=common_checkpoint::typed_resource_references;auto denied=visit_resident_receipt(old_reader,unsupported);require(!denied.ok&&denied.error.original.code=="InvalidMembershipReceipt","Old schema accepted named receipt tag");
    CodecVisitor::Buffer old_bytes;CodecVisitor old_writer(old_bytes);old_writer.schema_features=common_checkpoint::typed_resource_references;require(!visit_resident_receipt(old_writer,decoded).ok,"Old schema wrote named receipt tag");
}

// 冷装载新session只消费真实checkpoint；世界状态与再编码字节必须完全一致。
void checkpoint_roundtrip(Fixture& fixture){auto before=must(fixture.session->export_state(),"Roundtrip source state");auto bytes=must(fixture.session->checkpoint(),"Roundtrip source checkpoint");auto cold=create_resident_replay_session();require(bool(cold)&&!cold->identity().ok,"Cold session was already installed");auto selected=must(cold->inspect_checkpoint_selection(bytes),"Inspect named cold selection");require(boundary::get_string(selected,"scenario_id","selection")==fixture.scenario,"Cold selection chose another scenario");must(cold->initialize_checkpoint(fixture.catalog,bytes),"Initialize named checkpoint");same(before,must(cold->export_state(),"Export restored world"),"Cold world values or order changed");require(bytes==must(cold->checkpoint(),"Reencode restored checkpoint"),"Cold checkpoint bytes changed");}

// 只在独立typed副本中篡改一条缓存，重新算外壳摘要，证明拒绝来自语义来源校验。
void bad_receipt(Fixture& fixture,const StableId& result_id,const StableId& label,const std::function<void(MembershipOfferEffects&)>& mutate){
    auto world=fixture.snapshot();bool changed=false;for(auto& row:world->common.state().results.rows())if(row.second&&row.second->result_id==result_id){auto receipt=std::make_shared<CommitResult>(*row.second);require(bool(receipt->details),"Missing actual receipt details");auto details=std::make_shared<ReceiptDetails>(*receipt->details);auto* effect=std::get_if<MembershipOfferEffects>(&details->data);require(effect,"Actual membership receipt has wrong variant");mutate(*effect);receipt->details=std::move(details);row.second=std::move(receipt);changed=true;break;}require(changed,"Actual cached result missing: "+label);
    auto bytes=must(encode_world_checkpoint(*world),"Encode corrupt-source checkpoint: "+label);auto cold=create_resident_replay_session();require(bool(cold),"Corrupt checkpoint session missing");auto admitted=cold->initialize_checkpoint(fixture.catalog,bytes);require(!admitted.ok&&admitted.error.original.code=="MembershipReceiptSourceMismatch",label+" was not rejected by source validation: "+error_text(admitted.error));require(!cold->identity().ok,"Rejected cold candidate was installed: "+label);
}

// 两个同tick同请求者的完整合法载荷互换仍须拒绝，不能仅靠来源存在或重复集合检查。
void swapped_receipts(Fixture& fixture,const StableId& first,const StableId& second){
    auto world=fixture.snapshot();std::shared_ptr<const ReceiptDetails> a,b;
    for(const auto& row:world->common.state().results.rows()){if(row.second->result_id==first)a=row.second->details;if(row.second->result_id==second)b=row.second->details;}
    require(a&&b,"Both actual same-tick receipts required");
    for(auto& row:world->common.state().results.rows())if(row.second->result_id==first||row.second->result_id==second){auto copy=std::make_shared<CommitResult>(*row.second);copy->details=copy->result_id==first?b:a;row.second=std::move(copy);}
    auto bytes=must(encode_world_checkpoint(*world),"Encode swapped actual receipt payloads");auto cold=create_resident_replay_session();auto denied=cold->initialize_checkpoint(fixture.catalog,bytes);
    require(!denied.ok&&denied.error.original.code=="MembershipReceiptSourceMismatch"&&!cold->identity().ok,"Same-tick full payload swap was admitted");
}

// NPC预览和提交都必须在正式live边界拒绝，余额、知识、事实和序号逐字节保持。
void deny_impersonation(Fixture& fixture,const MembershipOffer& offer){
    auto params=object({{"actor_id",V{offer.target_actor_id}},{"offer_id",V{offer.id}},{"expected_offer_revision",V{offer.revision}},{"answer",V{StableId("accept")}},{"source_fact_id",V{offer.source_fact_id}}});auto raw=fixture.raw_command("organization_answer_membership",params,offer.target_actor_id);auto before=must(fixture.session->checkpoint(),"Before NPC impersonation");auto identity=must(fixture.session->identity(),"NPC impersonation identity");
    auto preview=must(fixture.session->execute_live({LiveOperationKind::preview,identity,object({{"actor_id",V{offer.target_actor_id}},{"kind",V{StableId("organization_answer_membership")}},{"params",params}})}),"External NPC preview");require(!okay(preview.result)&&boundary::get_string(preview.result,"code","preview")=="MembershipPlayerAuthorityRequired","Live NPC answer preview was not authority-denied");
    auto command=must(fixture.session->execute_live({LiveOperationKind::command,identity,raw}),"External NPC answer");require(!okay(command.result)&&boundary::get_string(command.result,"code","receipt")=="MembershipPlayerAuthorityRequired","Live NPC answer was not authority-denied");require(before==must(fixture.session->checkpoint(),"After NPC impersonation"),"Denied NPC preview/command changed checkpoint");
}

// 玩家代表实际发出两份邀请；负向用另一份真实来源，避免只证明未知ID会失败。
void issuer_boundary(const V& catalog){
    Fixture fixture(catalog,"scenario.society_membership_funded");auto first=fixture.action("organization_offer_membership");require(bool(first),"Formal funded scenario has no lawful invitation button");auto receipt=fixture.click(*first);receipt_roundtrip(receipt);const auto offer_id=boundary::get_string(boundary::field(receipt,"details","receipt"),"offer_id","details");
    auto second=fixture.action("organization_offer_membership");require(bool(second),"Formal funded scenario has no second lawful invitation");auto second_receipt=fixture.click(*second);receipt_roundtrip(second_receipt);const auto other_id=boundary::get_string(boundary::field(second_receipt,"details","receipt"),"offer_id","details");require(other_id!=offer_id,"Second invitation reused first offer");auto world=fixture.snapshot();const auto* offer=world->organization.membership_offers.find(offer_id);const auto* other=world->organization.membership_offers.find(other_id);require(offer&&other,"Actual invitation sources missing");deny_impersonation(fixture,*offer);checkpoint_roundtrip(fixture);
    const auto result_id=boundary::get_string(receipt,"result_id","receipt"),other_fact=other->source_fact_id;
    swapped_receipts(fixture,result_id,boundary::get_string(second_receipt,"result_id","receipt"));
    bad_receipt(fixture,result_id,"offer receipt rebound to another real offer",[&](auto& effect){effect.offer_id=other_id;});
    bad_receipt(fixture,result_id,"offer receipt rebound to another real fact",[&](auto& effect){effect.fact_ids={other_fact};});
    bad_receipt(fixture,result_id,"offer receipt receipt_ids rebound",[&](auto& effect){effect.receipt_ids={other_fact};});
    auto withdrawal=fixture.action("organization_withdraw_membership","offer_id",other_id);require(bool(withdrawal),"Actual issuer cannot withdraw pending invitation");auto withdrawn=fixture.click(*withdrawal);receipt_roundtrip(withdrawn);checkpoint_roundtrip(fixture);
    bad_receipt(fixture,boundary::get_string(withdrawn,"result_id","receipt"),"withdrawal receipt rebound to another real offer",[&](auto& effect){effect.offer_id=offer_id;});
    // 原NPC自己在有限复评点作决定；live越权拒绝不能堵住这条内部路径。
    bool decided=false;for(Integer step=0;step<24;++step){fixture.command("advance",object({}));auto next=fixture.snapshot();const auto* actual=next->organization.membership_offers.find(offer_id);require(actual,"Real pending invitation disappeared");if(actual->status==MembershipOfferStatus::accepted||actual->status==MembershipOfferStatus::declined){const auto* decision=next->organization.membership_decisions.find(actual->decision_fact_id);require(decision&&decision->target_actor_id==actual->target_actor_id,"Director result lost actual target decision");decided=true;break;}}
    require(decided,"Real Director did not answer its invitation within authored offer lifetime");checkpoint_roundtrip(fixture);
}

// 招聘NPC正常提出邀请，玩家本人先暂缓后接受；不直接添同意、成员或工资记录。
void recipient_boundary(const V& catalog){
    Fixture fixture(catalog,"scenario.society_membership_recipient");std::optional<V> defer;
    for(Integer step=0;step<24&&!defer;++step){defer=fixture.action("organization_answer_membership","answer","defer");if(!defer)fixture.command("advance",object({}));}
    require(bool(defer),"Actual recruitment post did not present player invitation");auto deferred=fixture.click(*defer);receipt_roundtrip(deferred);checkpoint_roundtrip(fixture);const auto& deferred_details=boundary::field(deferred,"details","receipt");const auto offer_id=boundary::get_string(deferred_details,"offer_id","details"),defer_id=boundary::get_string(deferred_details,"decision_id","details");
    std::optional<V> accept;auto deferred_world=fixture.snapshot();const auto* pending=deferred_world->organization.membership_offers.find(offer_id);require(pending,"Deferred invitation disappeared");const auto expiry=pending->expires_tick;
    // 本人暂缓不会打断招揽者的真实等候行动；只在原到期前等待第一个可接洽窗口。
    while(must(fixture.session->identity(),"Recipient contact identity").tick<expiry&&!accept){fixture.command("advance",object({}));accept=fixture.action("organization_answer_membership","answer","accept");}
    require(bool(accept),"Player cannot accept at an actual free contact boundary before original offer expiry");auto accepted=fixture.click(*accept);receipt_roundtrip(accepted);checkpoint_roundtrip(fixture);auto world=fixture.snapshot();const auto* offer=world->organization.membership_offers.find(offer_id);require(offer&&offer->status==MembershipOfferStatus::accepted,"Player acceptance did not establish accepted offer");const auto* member=world->organization.members.find(offer->member_episode_id);require(member&&member->episode_source&&member->actor_id==world->common.state().metadata.player_actor_id,"Actual player episode missing");require(world->local_economy.membership_wage_bindings.find(member->episode_source->wage_binding_id),"Actual accepted player lacks original wage binding");
    const auto result_id=boundary::get_string(accepted,"result_id","receipt"),source=offer->source_fact_id;
    bad_receipt(fixture,result_id,"accepted receipt rebound to real earlier defer",[&](auto& effect){effect.decision_id=defer_id;});
    bad_receipt(fixture,result_id,"accepted receipt lost required member episode",[](auto& effect){effect.member_episode_id.clear();});
    bad_receipt(fixture,result_id,"accepted receipt rebound to offered fact",[&](auto& effect){effect.fact_ids={source};});
    bad_receipt(fixture,boundary::get_string(deferred,"result_id","receipt"),"defer receipt falsely claims accepted member",[&](auto& effect){effect.member_episode_id=member->id;});
    // 同一成功请求仍重放原回执，不重复追加身份、义务或支付。
    auto snapshot=fixture.snapshot();StableId result_key;for(const auto& row:snapshot->common.state().results.rows())if(row.second&&row.second->result_id==result_id)result_key=row.first;const auto split=result_key.rfind(':');require(split!=StableId::npos,"Accepted request key missing");auto raw=fixture.raw_command("organization_answer_membership",boundary::field(*accept,"params","action"));*raw.find("request_seq")=V{Integer(std::stoll(result_key.substr(split+1)))};auto before=must(fixture.session->checkpoint(),"Before accepted request replay");auto repeated=must(fixture.session->execute_live({LiveOperationKind::command,must(fixture.session->identity(),"Replay identity"),raw}),"Replay accepted player request");same(accepted,repeated.result,"Duplicate accepted request changed receipt");require(before==must(fixture.session->checkpoint(),"After accepted replay"),"Duplicate accepted request changed membership world");
}
}

// 仅此有限消费者；使用最终中央二进制与未修改的正式prepared目录，输出真实检查数。
int main(int argc,char** argv){try{if(argc==2&&StableId(argv[1])=="--help"){std::cout<<"--catalog PREPARED_1_4_JSON\nUses Catalog.read_prepared output, funded and player-recipient scenarios, actual live commands and fresh in-process cold admission. No synthetic gameplay, direct acceptance, Main UI or separate-process claim.\n";return 0;}require(argc==3&&StableId(argv[1])=="--catalog","Usage: --catalog PREPARED_1_4_JSON");auto catalog=read_catalog(argv[2]);issuer_boundary(catalog);recipient_boundary(catalog);std::cout<<"NAMED_MEMBERSHIP_BOUNDARY_CHECKS="<<checks<<" FAILURES=0\n";return 0;}catch(const std::exception& error){std::cerr<<"named_membership_boundary: "<<error.what()<<" (checks="<<checks<<")\n";return 1;}}
