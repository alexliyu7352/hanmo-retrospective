// C有限只读投影：中央只传当前玩家；D包同源WorldIdentity，不回传世界、资产或领域引用。
#pragma once
#include "hanmo/resident/presentation_contract.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/domains/personal/personal_model.hpp"
namespace hanmo::resident {
struct JourneySpeakerView { StableId actor_id; std::string display_name; };
struct JourneyEncounterPeer { StableId encounter_id,provider_actor_id; std::string display_name; };
// encounter存在即为原sidebar早退；调用者不得再读party、军粮或旅行详细参数。
struct JourneyEncounterView {
    bool travel_active=false;
    std::optional<JourneyEncounterPeer> encounter;
    std::optional<JourneySpeakerView> dialogue_speaker;
};
struct JourneyLogisticsView { Integer available=0,required=0,mouth_count=0,full_ticks=0,shortage=0; };
struct JourneyDialogueOption { StableId id; std::string text; };
// optional保留原字段缺席；scene只有phase，选项页以当前条件后的可见顺序编号。
struct JourneyDialogueView {
    StableId phase="scene";
    std::optional<InteractionContext> context;
    std::optional<StableId> node_id,speaker;
    std::optional<DialogueOutcome> last_outcome;
    std::optional<std::string> text;
    std::optional<DialogueCommand> command;
    std::optional<StableId> pending_action_id;
    bool has_options=false;
    Integer cursor=0,total_rows=0;
    std::optional<Integer> next_cursor;
    std::vector<JourneyDialogueOption> options;
};
// 第一步只选真实路遇和当前对白说话人；没有会面不读取姓名、对白或军团。
Outcome<JourneyEncounterView> query_journey_encounter(const CommonRead&,const InteractionState&,const InteractionCatalog&,const PresentationRead&,const StableId& observer);
// 按原Combat→Siege→Encounter短路读取本人的真实占用；已关闭或无关会话不授予手动输入状态。
Outcome<bool> query_journey_input_need(const CommonRead&,const PersonalState&,const MilitaryState&,const InteractionState&,const StableId& observer);
// 仅实际active军团统帅可取自己的口粮汇总；无资格返回空，不泄露army ID/成员/钱柜/敌军。
Outcome<std::optional<JourneyLogisticsView>> query_own_army_logistics(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const StableId& observer);
// 复用all_conditions和PresentationRead；每页至多64选项，不泵对白、不授予命令权限。
Outcome<JourneyDialogueView> query_interaction_view(const CommonRead&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const PresentationRead&,const StableId& observer,Integer cursor=0);
} // 所有返回值深拥有，跨世界替换可安全释放；任何查询都不分配规则ID或消费RNG。
