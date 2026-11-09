// 本域来源往返与坏输入；尚不证明World版本注册或课程结算。
#include "resident/domains/growth/growth_study_teaching_requests_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {int checks=0;void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}}
int main(){try{
 using namespace boundary;using V=BoundaryValue;
 auto common=std::make_shared<CommonCatalog>();common->extended_skills=true;common->actor_ids={"teacher","student"};common->places.insert("school",{"school","town",{}});
 common->roles.insert("student.role",{"student.role","student","school",{},{},{},6});common->roles.insert("teacher.role",{"teacher.role","teacher","school",{},{},{"study_skill"},6});
 GrowthCatalog growth;GrowthRole role;GrowthCourse course;course.skill_id="medicine";course.mode="teacher";course.wallet_id="teacher.wallet";role.courses.push_back(course);growth.roles.insert("teacher.role",role);
 InteractionCatalog interactions;interactions.common=common;DialogueDefinition d;d.id="request";d.context_role="student.role";DialogueNode n;n.id="ask";n.kind="say";d.nodes.push_back(n);interactions.dialogues.push_back(d);
 auto definition=obj({{"id",V{"medicine.request"}},{"teacher_actor_id",V{"teacher"}},{"student_actor_id",V{"student"}},{"skill_id",V{"medicine"}},{"course_role_id",V{"teacher.role"}},{"dialogue_id",V{"request"}},{"node_id",V{"ask"}},{"duration",V{Integer{2}}},{"payment_policy",V{"student_personal_tuition_v1"}},{"tariff",V{"ordinary_next_level_v1"}}});
 auto defs=obj({{"medicine.request",definition},{"unused.old",V{false}}});auto config=obj({{"profile",V{"authored_study_teaching_requests_v1"}},{"request_ids",V{V::Array{V{"medicine.request"}}}}});
 auto catalog=decode_growth_study_teaching_catalog(defs,config,*common,growth,interactions);check(catalog.ok&&catalog.value.definitions.size()==1,"selected actual course declaration must decode without activating passive definitions");
 auto bad=config;*bad.find("profile")=V{"authored_teaching_requests_v1"};check(!decode_growth_study_teaching_catalog(defs,bad,*common,growth,interactions).ok,"personal skill request profile cannot activate ordinary tuition");
 bad=config;bad.as<V::Dict>()->emplace_back("accept_all",V{true});check(!decode_growth_study_teaching_catalog(defs,bad,*common,growth,interactions).ok,"automatic student acceptance rejected");
 bad=config;*bad.find("request_ids")=V{V::Array{V{"medicine.request"},V{"medicine.request"}}};check(!decode_growth_study_teaching_catalog(defs,bad,*common,growth,interactions).ok,"duplicate selection rejected");
 bad=config;*bad.find("request_ids")=V{V::Array{V{"missing"}}};check(!decode_growth_study_teaching_catalog(defs,bad,*common,growth,interactions).ok,"missing selected request rejected");
 auto baddefs=defs;*baddefs.find("medicine.request")->find("duration")=V{double{2}};check(!decode_growth_study_teaching_catalog(baddefs,config,*common,growth,interactions).ok,"floating duration rejected");
 baddefs=defs;*baddefs.find("medicine.request")->find("duration")=V{Integer{7}};check(!decode_growth_study_teaching_catalog(baddefs,config,*common,growth,interactions).ok,"unbounded course request rejected");
 baddefs=defs;*baddefs.find("medicine.request")->find("tariff")=V{"teacher_discretion"};check(!decode_growth_study_teaching_catalog(baddefs,config,*common,growth,interactions).ok,"unagreed fee formula rejected");
 common->extended_skills=false;check(!decode_growth_study_teaching_catalog(defs,config,*common,growth,interactions).ok,"old skill profile cannot activate medicine implicitly");common->extended_skills=true;
 GrowthStudyTeachingAgreement source;source.request_id="medicine.request";source.teacher_actor_id="teacher";source.student_actor_id="student";source.skill_id="medicine";source.course_role_id="teacher.role";source.dialogue_id="request";source.node_id="ask";source.node_kind="say";source.session_id="meeting";source.payment_policy="student_personal_tuition_v1";source.tariff="ordinary_next_level_v1";source.request_context={"school","student","student.role"};source.duration=2;source.accepted_tick=10;source.accepted_revision=9007199254740995LL;source.field_order={"accepted_revision","request_id","request_context","duration","tariff","payment_policy","teacher_actor_id","student_actor_id","skill_id","course_role_id","dialogue_id","node_id","node_kind","session_id","accepted_tick"};
 auto encoded=encode_growth_study_teaching_agreement(source);auto decoded=decode_growth_study_teaching_agreement(encoded);check(decoded.ok&&decoded.value.accepted_revision==source.accepted_revision&&compare_boundary(encoded,encode_growth_study_teaching_agreement(decoded.value)).exact(),"source JSON preserves request, fee policy, integer precision and field order");
 auto injected=encoded;injected.as<V::Dict>()->emplace_back("execution_epoch",V{Integer{1}});check(!decode_growth_study_teaching_agreement(injected).ok,"temporary source epoch cannot be persisted as permission");
 injected=encoded;*injected.find("accepted_revision")=V{double{4}};check(!decode_growth_study_teaching_agreement(injected).ok,"floating history revision rejected");
 injected=encoded;auto* rows=injected.as<V::Dict>();rows->erase(std::remove_if(rows->begin(),rows->end(),[](const auto& row){return row.first=="payment_policy";}),rows->end());check(!decode_growth_study_teaching_agreement(injected).ok,"missing explicit student payment policy rejected");
 CodecVisitor::Buffer bytes;CodecVisitor writer(bytes);check(visit_growth_study_teaching_agreement(writer,source).ok&&writer.finish().ok,"typed source writes");GrowthStudyTeachingAgreement restored;CodecVisitor reader(static_cast<const CodecVisitor::Buffer&>(bytes));check(visit_growth_study_teaching_agreement(reader,restored).ok&&reader.finish().ok&&compare_boundary(encoded,encode_growth_study_teaching_agreement(restored)).exact(),"typed cold source preserves all accepted terms and ordering");
 for(const auto length:{std::size_t{0},bytes.size()/2,bytes.size()-1}){auto cut=bytes;cut.resize(length);GrowthStudyTeachingAgreement row;CodecVisitor partial(static_cast<const CodecVisitor::Buffer&>(cut));auto decoded=visit_growth_study_teaching_agreement(partial,row);check(!decoded.ok||!partial.finish().ok,"truncated accepted source rejected");}
 auto tail=bytes;tail.push_back(0);GrowthStudyTeachingAgreement row;CodecVisitor extra(static_cast<const CodecVisitor::Buffer&>(tail));visit_growth_study_teaching_agreement(extra,row);check(!extra.finish().ok,"trailing unauthorized source bytes rejected");
 std::cout<<"STUDY_TEACHING_CODEC "<<checks<<" checks 0 failures; outer World registration pending\n";return 0;
}catch(const std::exception& e){std::cerr<<"STUDY_TEACHING_CODEC "<<checks<<" checks 1 failure: "<<e.what()<<'\n';return 1;}}
