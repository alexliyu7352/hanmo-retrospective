// 本人有限询问页只展示已听到的答复；当前原say只给开始入口和真实耗时，不读取答案作为预览。
#pragma once
#include "contact_inquiry.hpp"
#include "resident/views/scene_presentation.hpp"
namespace hanmo::resident {
struct ContactInquiryViewAccess {
    const KnownContactLocationAccess& history;
    const InteractionState& interaction;
    const InteractionCatalog& interaction_catalog;
    const InteractionAccess& interaction_access;
    const ContactInquiryCatalog& catalog;
    const ContactInquiryState& state;
    const FamilyDeathOriginRead& deaths;
    const SceneCatalog& scene;
    const ContactInquiryTime2Context* exact=nullptr;
};
Outcome<BoundaryValue> query_contact_inquiries(const ContactInquiryViewAccess&,Integer cursor=0);
}
