// 外交目录只定义具名双方、地点和可协商条款；目录存在不等于协议已生效。
#pragma once
#include "resident/domains/organization/organization_model.hpp"
#include "diplomacy_v2_catalog.hpp"
namespace hanmo::resident {
// 首个通行出口只涵盖明确场所的个人行旅，不隐含公军、征调或政治归附。
struct DiplomaticPassageTerms {
    StableId id,name_key,grantor_organization_id,beneficiary_organization_id;
    std::vector<StableId> place_ids;
    Integer duration_ticks=0,expiry_notice_ticks=0;
};
// 正式使者责任可选；亲递没有配置这段时不会隐式生成Task或默认工时。
struct DiplomaticMissionTaskTerms {
    Integer acceptance_valid_ticks=0,delivery_work_ticks=0,report_work_ticks=0;
    bool return_report_required=false;
    StableId continuity_policy_id;
};
// 条款集合是签发方明确准许协商的边界；使者无权增填任意地点或永久期限。
struct DiplomaticMissionPolicy {
    StableId id,name_key,issuer_organization_id,target_organization_id;
    StableId issuer_site_id,target_site_id,target_office_id;
    std::vector<StableId> passage_terms_ids;
    Integer offer_valid_ticks=0;
    std::optional<DiplomaticMissionTaskTerms> task_terms;
};
// 独立可选目录尚未注册到世界；旧政治/治理目录保持原结构。
struct DiplomacyCatalog {
    Integer version=1;bool enabled=false;
    OrderedTable<DiplomaticPassageTerms> passage_terms;
    OrderedTable<DiplomaticMissionPolicy> mission_policies;
    std::optional<DiplomacyV2Catalog> v2;
};
// 纯结构门不给通行许可；签约时还必须逐场所核当前专项Office/Grant与真实接触。
Status validate_diplomacy_catalog(const CommonCatalog&,const OrganizationCatalog&,const DiplomacyCatalog&);
Outcome<DiplomacyCatalog> decode_diplomacy_catalog(const BoundaryValue&);
BoundaryValue encode_diplomacy_catalog(const DiplomacyCatalog&);
}
