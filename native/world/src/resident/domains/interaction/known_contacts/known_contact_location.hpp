// 已知去处只派生本人实际接触的旧来源，不存第二位置账，也不读取远方人物今日坐标。
#pragma once
#include "known_contact_location_contract.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_presence.hpp"
#include "resident/domains/interaction/appointments/personal_appointment.hpp"
#include "resident/domains/tasks/message_handoffs/task_message_report.hpp"
namespace hanmo::resident {
class CurrentMomentRead;
struct ContactInquiryState;struct ContactInquiryCatalog;
// 借用对象必须来自同一个已经完整通过Family/Creation/Birth、军事及Task冷验的World。
struct AppointmentContactHistory {
    const InteractionState& interaction;
    const InteractionCatalog& interaction_catalog;
    const PersonalAppointmentCatalog& catalog;
    const PersonalAppointmentState& state;
    const FamilyDeathOriginRead& death_origins;
};
struct MessageContactHistory {
    const TaskState& tasks;
    const TaskCatalog& catalog;
    const TaskMessageHandoffState& state;
};
struct KnownContactLocationAccess {
    const CommonStore& common;
    const CommonCatalog& common_catalog;
    const AppointmentContactHistory* appointments=nullptr;
    const MessageContactHistory* messages=nullptr;
    // Exact callers borrow the actual selected appointment owner and phase clock.
    const PersonalAppointmentTime2Context* exact_appointments=nullptr;
    const CurrentMomentRead* actual_clock=nullptr;
};
struct KnownContactLocations {
    WorldIdentity source_identity;
    StableId observer_actor_id;
    Integer cursor=0;
    std::optional<Integer> next_cursor;
    std::vector<KnownContactLocation> rows;
};
// 最多50条，按原Common事实次序倒序；第三方、无来源人物与未来约期不会进入列表。
Outcome<KnownContactLocations> read_known_contact_locations(const KnownContactLocationAccess&,const StableId& observer,Integer cursor=0,Integer limit=16);
// 精确查询仍限观察者本人旧接触，未知ID和真实但未接触人物均返回空，不泄漏存在性。
Outcome<std::optional<KnownContactLocation>> read_known_contact_location(const KnownContactLocationAccess&,const StableId& observer,const StableId& subject);
// 仅真实contact_inquiry_started原头可请求开工前截面；全局编号和Common顺序双核，不拿当前latest倒填旧答复。
Outcome<std::optional<KnownContactLocation>> read_known_contact_location_before_inquiry(const KnownContactLocationAccess&,const StableId& observer,const StableId& subject,Integer start_tick,const StableId& start_fact_id,const DynamicStepInput* phase=nullptr);
// The actual inquiry start row and immutable event supply the only exact cut.
Outcome<std::optional<KnownContactLocation>> read_known_contact_location_before_inquiry2(const KnownContactLocationAccess&,const ContactInquiryState&,const ContactInquiryCatalog&,const StableId& start_fact_id);
}
