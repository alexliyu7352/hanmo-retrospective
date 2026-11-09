// 条约审计的内部派生索引；只在一次读取中生成，不另存知情或生效权威。
#pragma once
#include "treaty_passage_model.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident::treaty_passage_detail {
struct DocumentKnowledge {
    StableId offer_id,reply_id,signature_id;
    Integer tick=0,sequence=0;
    std::vector<StableId> witnesses;
};
struct Audit {
    std::map<StableId,DocumentKnowledge> documents;
    std::map<StableId,std::vector<const TreatyPassageSignature*>> signatures;
};
bool same_identity(const WorldIdentity&,const WorldIdentity&);
bool same_authority(const ActingAuthority&,const ActingAuthority&);
bool same_control(const TreatyPassageControlSnapshot&,const TreatyPassageControlSnapshot&);
bool contact_valid(const CommonStore&,const TreatyPassageContact&);
TreatyPassageContact freeze_contact(const FaceToFaceContactProof&);
// 条款、回复、原Office及知情均沿实际实体读，未知ID不授任何权限。
Outcome<Audit> audit(const TreatyPassageContext&);
bool knows(const DocumentKnowledge&,const StableId& actor);
const Grant* ratification_grant(const OrganizationCatalog&,const OfficeState&,const DiplomaticMissionPolicy&,const DiplomaticPassageTerms&,TreatyPassageSide,Integer);
bool historical_grant(const OrganizationCatalog&,const StableId&,const ActingAuthority&,const DiplomaticMissionPolicy&,const DiplomaticPassageTerms&,TreatyPassageSide,Integer);
Outcome<std::vector<TreatyPassageControlSnapshot>> current_controls(const TreatyPassageContext&,const DiplomaticPassageTerms&);
}
