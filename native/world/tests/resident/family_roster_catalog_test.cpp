// 新名单与生成原型的定向目录门；只运行真实decoder/同一规则守卫，不借旧ABI库模拟完整收养。
#include "resident/domains/family/family_model.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
using V=BoundaryValue;Integer checks=0;
// Release保留所有断言，失败显示正式拒绝码。
void check(bool value,const StableId& label){++checks;if(!value)throw std::runtime_error(label);}
// 三个作者人物中third可以是尚未启用的旧十项原型，不应阻止新情景目录读取。
V input(){using namespace boundary;auto attributes=obj({{"administration",V{Integer{20}}},{"charisma",V{Integer{20}}},{"intellect",V{Integer{20}}},{"leadership",V{Integer{20}}},{"martial",V{Integer{20}}}});V::Dict skills;for(const auto* id:{"archery","cavalry","commerce","debate","infantry","martial","negotiation","rites","tactics","transport"})skills.emplace_back(id,V{Integer{0}});V::Dict actors;for(const auto* id:{"hero","peer","third"})actors.emplace_back(id,obj({{"attributes",attributes},{"skills",V{skills}}}));return obj({{"actors",V{std::move(actors)}},{"roles",V{V::Dict{}}},{"texts",V{V::Dict{}}},{"required_capabilities",V{V::Array{}}},{"scenarios",obj({{"scenario",obj({{"player_actor",V{"hero"}},{"playable_actor_ids",string_array({"hero","peer","third"})},{"dynamic_world",obj({{"founder_policy",V{"selected_existing_actor_roster_v1"}}})}})}})}});}
// 明确作者六项有不同XP，用于证明没有默补零或串字段。
void extend(V& data,const StableId& id){Integer xp=100;for(const auto* name:{"naval","espionage","fortification","agriculture","medicine","forging"}){data.find("actors")->find(id)->find("skills")->as<V::Dict>()->emplace_back(name,V{xp});xp+=100;}}
// 同一初态人物集合由真实CommonState提供，调用的是validate_family实际使用的只读守卫。
Status founder(const FamilyCatalog& catalog,const StableId& id){CommonState common;for(const auto* person:{"hero","peer","third"}){ActorRecord actor;actor.id=person;common.actors.insert(person,actor);}FamilyState family;family.founder_actor_id=id;return validate_family_founder(common,family,catalog);}
}
// 保留旧双人限制，接受新1..4096策略，原型实际使用按选定技能profile前置拒绝。
int main(){try{using namespace boundary;
    auto data=input();auto decoded=decode_family_catalog(data,"scenario");check(decoded.ok,"new three-person family catalog decodes");check(decoded.value.founder_selection==FamilyFounderSelection::configured_roster&&founder(decoded.value,"peer").ok&&founder(decoded.value,"third").ok,"selected existing founder matches new roster");
    auto catalog=decoded.value;catalog.founder_selection=FamilyFounderSelection::legacy_pair;check(!founder(catalog,"peer").ok,"old typed mode still rejects three-person list");catalog=decoded.value;catalog.allowed_founder_actor_ids.clear();check(!founder(catalog,"hero").ok,"new typed mode cannot downgrade empty list to fixed founder");
    *data.find("scenarios")->find("scenario")->find("playable_actor_ids")=string_array({"hero"});decoded=decode_family_catalog(data,"scenario");check(decoded.ok&&founder(decoded.value,"hero").ok,"single configured founder is legal");
    *data.find("scenarios")->find("scenario")->find("dynamic_world")->find("founder_policy")=V{"selected_existing_actor_v1"};check(!decode_family_catalog(data,"scenario").ok,"old policy still rejects one-person list");
    *data.find("scenarios")->find("scenario")->find("playable_actor_ids")=string_array({"hero","peer"});decoded=decode_family_catalog(data,"scenario");check(decoded.ok&&founder(decoded.value,"peer").ok&&!founder(decoded.value,"third").ok,"old two-person policy remains strict");
    data=input();data.find("scenarios")->find("scenario")->as<V::Dict>()->emplace_back("skill_profile",V{"full_sixteen_v1"});extend(data,"hero");extend(data,"peer");const auto before=render_boundary_json(data);decoded=decode_family_catalog(data,"scenario");check(decoded.ok&&decoded.value.extended_skills,"selected full skill profile decoded");check(render_boundary_json(data).value==before.value,"decoder preserves full author input");
    const auto* actual=decoded.value.prototypes.find("hero");check(actual&&actual->skills.extended&&actual->skills.extended->naval==100&&actual->skills.extended->forging==600,"prototype retains actual six XP values");check(actual->skills_order.size()==11&&actual->skills_order.back()=="extended"&&actual->skills.extended->field_order.size()==6,"flat author order maps to nested cold block");
    check(!decoded.value.prototypes.find("third")->skills.extended,"unused old ten-skill prototype stays unexpanded");auto guard=family_prototype_skill_eligibility(decoded.value,*decoded.value.prototypes.find("third"));check(!guard.ok&&guard.error.original.code=="FamilyPrototypeSkillsRequired","using missing six-skill source fails before creation");check(family_prototype_skill_eligibility(decoded.value,*actual).ok,"using complete explicit prototype succeeds");
    auto old=decoded.value;old.extended_skills=false;check(family_prototype_skill_eligibility(old,*old.prototypes.find("third")).ok&&!family_prototype_skill_eligibility(old,*old.prototypes.find("hero")).ok,"old generation retains ten-skill profile");
    auto partial=input();partial.find("actors")->find("third")->find("skills")->as<V::Dict>()->emplace_back("naval",V{Integer{1}});check(!decode_family_catalog(partial,"scenario").ok,"partial extension does not silently fill other skills");
    auto malformed=data;*malformed.find("actors")->find("hero")->find("skills")->find("forging")=V{Integer{1201}};check(!decode_family_catalog(malformed,"scenario").ok,"out of range prototype XP rejected");
    std::cout<<"FAMILY_ROSTER_CATALOG "<<checks<<" checks 0 failures\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAMILY_ROSTER_CATALOG FAIL "<<error.what()<<" after "<<checks<<" checks\n";return 1;}}
