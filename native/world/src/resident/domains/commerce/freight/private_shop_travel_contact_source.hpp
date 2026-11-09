// 普通路遇截停的来源合同候选：原Encounter真正插入后才可读，尚无World注册。
#pragma once
#include "resident/domains/travel/travel_contact_capture.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include <memory>
namespace hanmo::resident {
class CommonStore;
// 必须在原install_encounter前建立；其中将仅拥有值，不保存EncounterInstall的callback。
class PrivateShopTravelContactBoundary {
public:PrivateShopTravelContactBoundary()=default;
private:
 struct State;std::shared_ptr<const State> state_;
 friend Outcome<PrivateShopTravelContactBoundary> capture_private_shop_travel_contact_boundary(const CommonStore&,const TravelState&,const InteractionState&,const ContactHit&,const EncounterInstall&);
 friend struct PrivateShopTravelContactSourceAccess;
};
// 两端原Action/已付后运动快照仍是暂态值，真实EncounterRecord只来自同次安装返回后的原表。
struct PrivateShopTravelContactSource {
 WorldIdentity source_identity;
 EncounterRecord actual_encounter;
 TravelContactTruncationDraft actual_truncation;
};
// 原ContactBatch完整几何/当前版本和稳定命中次序须先验；本口不重新授权相遇或决定参与者。
// 捕获同Common资源身份、原两端、真实members/原player以及全局ID前界；不分ID、不运动。
Outcome<PrivateShopTravelContactBoundary> capture_private_shop_travel_contact_boundary(
 const CommonStore&,const TravelState&,const InteractionState&,
 const ContactHit& original_selected_hit,const EncounterInstall& original_install);
// 仅install_encounter成功返回后调用，按actual_truncation.original_encounter_id恰一匹配原表。
// motion.source_tick是Common.from；Encounter.created_tick是from+1。非交互resolved同to且原result
// 紧随encounter分配，交互仍pending/-1。两种都不能拿当前from冒record的created_tick。
// apply_motion中只暂持草稿并把实际e.id加入同一retained集合；记录尚未push时禁止消费此口。
// 失败/callback拒绝/未pin被裁均不提供来源；无退款/新收费/完成资格，不代永久货史。
Outcome<PrivateShopTravelContactSource> read_private_shop_travel_contact_installation(
 const CommonStore&,const TravelState&,const InteractionState&,
 const PrivateShopTravelContactBoundary&,const TravelContactTruncationDraft& actual_truncation);
}
