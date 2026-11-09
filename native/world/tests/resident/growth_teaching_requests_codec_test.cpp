// 仅教学永久来源的本域JSON/typed二进制，不声称已接World保存或正式授徒。
#include "resident/domains/growth/growth_teaching_requests_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}}
int main(){try{
    using namespace boundary;using V=BoundaryValue;
    auto shared=std::make_shared<CommonCatalog>();shared->actor_ids={"teacher","student"};shared->personal_skill_kinds.insert("heart","heart");shared->places.insert("school",{"school","town",{}});RoleDefinition role;role.id="learner";role.place_id="school";shared->roles.insert(role.id,role);
    InteractionCatalog interactions;interactions.common=shared;DialogueDefinition d;d.id="learning";d.context_role="learner";DialogueNode n;n.id="ask";n.kind="say";d.nodes.push_back(n);interactions.dialogues.push_back(d);
    auto definition=obj({{"id",V{"request"}},{"teacher_actor_id",V{"teacher"}},{"student_actor_id",V{"student"}},{"skill_id",V{"heart"}},{"dialogue_id",V{"learning"}},{"node_id",V{"ask"}}});auto defs=obj({{"request",definition},{"passive.old",V{false}}});auto config=obj({{"profile",V{"authored_teaching_requests_v1"}},{"request_ids",V{V::Array{V{"request"}}}}});
    auto catalog=decode_growth_teaching_request_catalog(defs,config,*shared,interactions);check(catalog.ok&&catalog.value.definitions.size()==1,"explicit selected request decoded; unused passive row does not become active");
    auto bad=config;*bad.find("profile")=V{"unknown"};check(!decode_growth_teaching_request_catalog(defs,bad,*shared,interactions).ok,"unknown profile rejected");bad=config;bad.as<V::Dict>()->emplace_back("accept_all",V{true});check(!decode_growth_teaching_request_catalog(defs,bad,*shared,interactions).ok,"author cannot add automatic acceptance flag");
    bad=config;*bad.find("request_ids")=V{V::Array{V{"request"},V{"request"}}};check(!decode_growth_teaching_request_catalog(defs,bad,*shared,interactions).ok,"duplicate selected request rejected");bad=config;*bad.find("request_ids")=V{V::Array{V{"missing"}}};check(!decode_growth_teaching_request_catalog(defs,bad,*shared,interactions).ok,"missing selected request rejected");
    GrowthTeachingAgreement source{"request","teacher","student","heart","learning","ask","say","meeting",{"school","student","learner"},10,9007199254740995LL,{"accepted_revision","request_id","context","skill_id","accepted_tick","teacher_actor_id","student_actor_id","dialogue_id","node_id","node_kind","session_id"}};
    const auto encoded=encode_growth_teaching_agreement(source);auto decoded=decode_growth_teaching_agreement(encoded);check(decoded.ok&&decoded.value.accepted_revision==source.accepted_revision&&compare_boundary(encoded,encode_growth_teaching_agreement(decoded.value)).exact(),"permanent source and integer above double precision round trip exactly");
    auto injected=encoded;injected.as<V::Dict>()->emplace_back("execution_epoch",V{Integer{2}});check(!decode_growth_teaching_agreement(injected).ok,"temporary proof epoch cannot be persisted as permission");injected=encoded;*injected.find("accepted_revision")=V{double(4)};check(!decode_growth_teaching_agreement(injected).ok,"floating revision cannot masquerade as source integer");
    auto missing=encoded;auto* rows=missing.as<V::Dict>();rows->erase(std::remove_if(rows->begin(),rows->end(),[](const auto& row){return row.first=="student_actor_id";}),rows->end());check(!decode_growth_teaching_agreement(missing).ok,"missing named student rejected");
    CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(visit_growth_teaching_agreement(writer,source).ok&&writer.finish().ok,"typed source write");const auto original=bytes;GrowthTeachingAgreement restored;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes));check(visit_growth_teaching_agreement(reader,restored).ok&&reader.finish().ok&&compare_boundary(encoded,encode_growth_teaching_agreement(restored)).exact(),"typed cold source read exactly restores fields and order");
    for(const auto length:{std::size_t{0},original.size()/2,original.size()-1}){auto cut=original;cut.resize(length);GrowthTeachingAgreement item;CodecVisitor partial(static_cast<const CodecVisitor::Buffer&>(cut));auto read=visit_growth_teaching_agreement(partial,item);check(!read.ok||!partial.finish().ok,"truncated source cannot become a valid agreement");}
    auto tail=original;tail.push_back(0);GrowthTeachingAgreement item;CodecVisitor extra(static_cast<const CodecVisitor::Buffer&>(tail));visit_growth_teaching_agreement(extra,item);check(!extra.finish().ok,"trailing private permission bytes rejected");
    std::cout<<"growth_teaching_requests_codec checks="<<checks<<" failures=0; World integration pending\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
