// 只编码显式作者意愿，任何未知版本/字段/重复键都不能充当本人同意。
#include "party_recruitment_consent.hpp"
#include "resident/storage/common_codec.hpp"
#include <set>
namespace hanmo::resident {namespace {
// first-match读取之前检查每层唯一键，不允许隐藏另一份decision。
void unique(const BoundaryValue& value,const StableId& path){std::set<StableId> seen;for(const auto& field:boundary::object(value,path))if(!seen.insert(field.first).second)boundary::fail("DuplicatePartyRecruitmentField",path+"/"+field.first);}
}
Outcome<PartyRecruitmentConsentCatalog> decode_party_recruitment_consent_catalog(const BoundaryValue& value,const InteractionCatalog& catalog){try{
    using namespace boundary;const StableId root="party_recruitment_consents";unique(value,root);fields(value,{"version","declarations"},{},root);PartyRecruitmentConsentCatalog out;out.version=get_int(value,"version",root);if(out.version!=1)unsupported("UnsupportedPartyRecruitmentConsentVersion",root);const auto& entries=array(field(value,"declarations",root),root+"/declarations");if(entries.size()>4096)fail("PartyRecruitmentDeclarationLimit",root);
    for(const auto& row:entries){const auto path=root+"/declarations";unique(row,path);fields(row,{"id","leader_actor_id","companion_actor_id","role_id","dialogue_id","node_id","decision"},{},path);out.declarations.push_back({get_string(row,"id",path),get_string(row,"leader_actor_id",path),get_string(row,"companion_actor_id",path),get_string(row,"role_id",path),get_string(row,"dialogue_id",path),get_string(row,"node_id",path),get_string(row,"decision",path)});}
    auto valid=validate_party_recruitment_consent_catalog(out,catalog);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
BoundaryValue encode_party_recruitment_consent_catalog(const PartyRecruitmentConsentCatalog& value){using V=BoundaryValue;V::Array rows;for(const auto& row:value.declarations)rows.push_back(boundary::obj({{"id",V{row.id}},{"leader_actor_id",V{row.leader_actor_id}},{"companion_actor_id",V{row.companion_actor_id}},{"role_id",V{row.role_id}},{"dialogue_id",V{row.dialogue_id}},{"node_id",V{row.node_id}},{"decision",V{row.decision}}}));return boundary::obj({{"version",V{value.version}},{"declarations",V{std::move(rows)}}});}
}
