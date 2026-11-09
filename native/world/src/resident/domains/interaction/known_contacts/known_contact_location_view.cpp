// 原作者地点名只在已知地点被选中后读取；原观察时刻不会因浏览刷新为当前时刻。
#include "known_contact_location_view.hpp"
#include "resident/views/common_presentation.hpp"
namespace hanmo::resident {namespace {
using V=BoundaryValue;
V identity(const WorldIdentity& s){StableId id;constexpr char hex[]="0123456789abcdef";for(auto b:s.world_id){id+=hex[b>>4];id+=hex[b&15];}return boundary::obj({{"world_id",V{id}},{"execution_epoch",V{s.execution_epoch}},{"revision",V{s.revision}},{"tick",V{s.tick}}});}
StableId source_text(const StableId& kind){
    if(kind==personal_appointment_accepted_kind)return "双方当面答允约期";
    if(kind==personal_appointment_cancelled_kind)return "双方当面取消原约";
    if(kind==personal_appointment_started_kind)return "双方实际开始会面";
    if(kind==personal_appointment_finished_kind)return "双方实际完成会面";
    if(kind==task_message_handoff_kind)return "本人实际交接原信件";
    if(kind==task_message_report_kind)return "本人当面返报或听取返报";
    return {};
}
}
Outcome<BoundaryValue> query_known_contact_locations(const KnownContactLocationAccess& read,const SceneCatalog& scene,Integer cursor){
    if(!read.appointments&&!read.messages)return {false,{},common_failure("KnownContactSourceProfileRequired").error};const auto& observer=read.common.state().metadata.player_actor_id;auto known=read_known_contact_locations(read,observer,cursor,16);if(!known.ok)return {false,{},known.error};CommonPresentation names(read.common.state(),read.common_catalog);V::Array rows;
    for(const auto& source:known.value.rows){
        auto name=names.display_name(source.subject_actor_id);if(!name.ok)return {false,{},name.error};const auto* place=scene.places.find(source.place_id);if(!place||place->id!=source.place_id)return {false,{},common_failure("KnownContactPlacePresentationMissing",source.place_id).error};auto place_name=names.text(place->name_key);if(!place_name.ok)return {false,{},place_name.error};auto label=source_text(source.source_kind);if(label.empty())return {false,{},common_failure("UnknownContactSourceKind",source.source_fact_id).error};
        rows.push_back(boundary::obj({{"subject_actor_id",V{source.subject_actor_id}},{"display_name",V{name.value}},{"place_id",V{source.place_id}},{"place_name",V{place_name.value}},{"observed_tick",V{source.observed_tick}},{"learned_tick",V{source.learned_tick}},{"source_fact_id",V{source.source_fact_id}},{"source_kind",V{source.source_kind}},{"source_text",V{label}},{"reliability",V{StableId{"witnessed"}}},{"current_location_confirmed",V{false}},{"explanation",V{"半日 "+std::to_string(source.observed_tick)+" 在此亲自接触；只证明当时所在，不能确认对方现在仍在此处。"}}}));
    }
    V::Dict page{{"ok",V{true}},{"source_identity",identity(known.value.source_identity)},{"observer_actor_id",V{observer}},{"cursor",V{cursor}},{"rows",V{std::move(rows)}}};if(known.value.next_cursor)page.emplace_back("next_cursor",V{*known.value.next_cursor});return {true,V{std::move(page)},{}};
}
}
