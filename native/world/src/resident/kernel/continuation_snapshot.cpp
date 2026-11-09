// HMCN1整数许可与HMCN2精确许可明确分派；同一世界仅作恢复绑定和领域停点权威。
#include "continuation_snapshot.hpp"
#include "world_bundle.hpp"
#include "world_timeline_codec.hpp"
#include "exact_rest_binding.hpp"
#include "exact_run_read.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/military/siege_world.hpp"
#include "resident/domains/organization/membership_offer.hpp"
#include <limits>
#include <stdexcept>
namespace hanmo::resident {
namespace {
constexpr std::size_t id_limit=256;
struct ContinuationFailure {RuleError error;};
template<class T> T take(Outcome<T> result){if(!result.ok)throw ContinuationFailure{std::move(result.error)};return std::move(result.value);}
void require(bool yes,const char* code="InvalidContinuationSnapshot"){if(!yes)throw ContinuationFailure{common_failure(code).error};}
void capacity(bool yes){if(!yes){auto e=common_failure("ContinuationCapacityExceeded").error;e.kind=ErrorKind::internal;throw ContinuationFailure{std::move(e)};}}
template<class T,class F> Outcome<T> guarded(F fn){try{return {true,fn(),{}};}catch(ContinuationFailure& e){return {false,{},std::move(e.error)};}catch(const std::bad_alloc&){auto e=common_failure("ContinuationCapacityExceeded").error;e.kind=ErrorKind::internal;return {false,{},std::move(e)};}catch(const std::length_error&){auto e=common_failure("ContinuationCapacityExceeded").error;e.kind=ErrorKind::internal;return {false,{},std::move(e)};}}
// 所有多字节标量显式小端，先检查范围再转换；旧版仍用原u64字符串长度。
void unsigned_number(Bytes& out,std::uint64_t value,unsigned width){for(unsigned n=0;n<width;++n)out.push_back(static_cast<std::uint8_t>(value>>(8*n)));}
void number(Bytes& out,Integer value){unsigned_number(out,static_cast<std::uint64_t>(value),8);}
void identifier(Bytes& out,const StableId& value,unsigned width=8){unsigned_number(out,value.size(),width);out.insert(out.end(),value.begin(),value.end());}
struct Reader {
    const Bytes& bytes;std::size_t at=0;bool valid=true;std::size_t trusted_limit=exact_continuation_snapshot_byte_limit;
    std::uint64_t unsigned_number(unsigned width){if(!valid||at>bytes.size()||width>bytes.size()-at){valid=false;return 0;}std::uint64_t value=0;for(unsigned n=0;n<width;++n)value|=std::uint64_t(bytes[at++])<<(8*n);return value;}
    Integer number(){auto value=unsigned_number(8);if(value>std::uint64_t(std::numeric_limits<Integer>::max())){valid=false;return 0;}return Integer(value);}
    StableId identifier(unsigned width=8){auto size=unsigned_number(width);if(!valid||size<1||size>id_limit||size>bytes.size()-at){valid=false;return {};}StableId value(bytes.begin()+at,bytes.begin()+at+std::size_t(size));at+=std::size_t(size);return value;}
};
bool valid_id(const StableId& v){return !v.empty()&&v.size()<=id_limit&&v.find('\0')==StableId::npos;}
// 新String256严格合法UTF-8；保旧版原字节语义，不以升级顺手改变HMCN1读数。
bool exact_id(const StableId& v){if(!valid_id(v))return false;for(std::size_t i=0;i<v.size();){auto lead=static_cast<unsigned char>(v[i++]);if(lead<0x80)continue;unsigned count=0;std::uint32_t point=0,minimum=0;if(lead>=0xc2&&lead<=0xdf){count=1;point=lead&0x1f;minimum=0x80;}else if(lead>=0xe0&&lead<=0xef){count=2;point=lead&0x0f;minimum=0x800;}else if(lead>=0xf0&&lead<=0xf4){count=3;point=lead&7;minimum=0x10000;}else return false;if(count>v.size()-i)return false;for(unsigned n=0;n<count;++n){auto byte=static_cast<unsigned char>(v[i++]);if((byte&0xc0)!=0x80)return false;point=(point<<6)|(byte&0x3f);}if(point<minimum||point>0x10ffff||(point>=0xd800&&point<=0xdfff))return false;}return true;}
// 物理stop是合同闭表，不依赖C++ enum当前布局；legacy数值保持原字节。
std::optional<std::uint8_t> stop_code(RunStop stop){switch(stop){case RunStop::awaiting_permit:return 0;case RunStop::awaiting_input:return 1;case RunStop::arrived:return 2;case RunStop::paused:return 3;case RunStop::stranded:return 4;case RunStop::blocked:return 5;case RunStop::cancelled:return 6;case RunStop::budget_exhausted:return 7;case RunStop::failed:return 8;case RunStop::money_budget_exhausted:return 9;case RunStop::action_replaced:return 10;case RunStop::restored:return 11;case RunStop::completed:return 12;}return {};}
std::optional<RunStop> read_stop(std::uint64_t code){switch(code){case 0:return RunStop::awaiting_permit;case 1:return RunStop::awaiting_input;case 2:return RunStop::arrived;case 3:return RunStop::paused;case 4:return RunStop::stranded;case 5:return RunStop::blocked;case 6:return RunStop::cancelled;case 7:return RunStop::budget_exhausted;case 8:return RunStop::failed;case 9:return RunStop::money_budget_exhausted;case 10:return RunStop::action_replaced;case 11:return RunStop::restored;case 12:return RunStop::completed;default:return {};}}
// 同一RWM2 visitor完整叶带u64长度；绝不把JSON串或工量冒充时长。
template<class T,class F> void write_leaf(Bytes& out,const T& value,F visit,std::size_t limit){T copy=value;Bytes leaf;CodecVisitor writer(leaf);take(visit(writer,copy));take(writer.finish());capacity(leaf.size()<=limit);capacity(limit>=8&&out.size()<=limit-8&&leaf.size()<=limit-out.size()-8);unsigned_number(out,leaf.size(),8);out.insert(out.end(),leaf.begin(),leaf.end());}
template<class T,class F> T read_leaf(Reader& read,F visit){auto size=read.number();require(read.valid&&size>0&&std::uint64_t(size)<=read.bytes.size()-read.at);capacity(std::uint64_t(size)<=read.trusted_limit);const Bytes leaf(read.bytes.begin()+read.at,read.bytes.begin()+read.at+std::size_t(size));read.at+=std::size_t(size);T value;CodecVisitor reader(leaf);take(visit(reader,value));take(reader.finish());return value;}
bool exact_selected(const WorldBundle& w){return w.catalog&&w.catalog->exact_time&&exact_time_action_allowed(*w.catalog->exact_time,"service","rest").ok&&w.world_timeline&&w.action_elapsed;}
// 真实原Started唯一定位，校验同一Action/owner/接受源，不能按整数start_tick补猜。
const LifeFixedServiceBoundary* exact_start(const WorldBundle& w,const ContinuationSnapshot& s){if(!exact_selected(w)||!s.action_started_source)return nullptr;const auto& ref=*s.action_started_source;if(ref.domain!="growth"||ref.kind!="growth_fixed_service_boundary"||ref.schema!=1)return nullptr;const auto* first=w.growth.fixed_service_boundaries.find(ref.id);if(!first||first->boundary_id!=ref.id||first->source_schema!=1||first->kind!=LifeFixedBoundaryKind::started||first->action_ref!=s.action_id||first->action_kind!="service"||first->boundary_ordinal!=0||first->previous_boundary_ref||first->original_outcome_ref||first->owner_ref.kind!=1||first->owner_ref.life_activity_id!=s.action_id||first->owner_ref.terms_kind!=0)return nullptr;std::size_t starts=0;for(const auto& row:w.growth.fixed_service_boundaries.rows())if(row.second.action_ref==s.action_id&&row.second.kind==LifeFixedBoundaryKind::started)++starts;if(starts!=1)return nullptr;const auto* command=std::get_if<LifeActualCommand>(&first->cause);const auto* accepted=command?std::get_if<SourceRef>(&command->accepted_business_ref):nullptr;if(!command||command->actor_id!=s.actor_id||command->command_binding.command_kind!="service"||!accepted||accepted->domain!="growth"||accepted->kind!="life_activity"||accepted->id!=s.action_id)return nullptr;return first;}
const ActionElapsedState* exact_elapsed(const WorldBundle& w,const StableId& action){return w.action_elapsed?find_fixed_elapsed(*w.action_elapsed,action):nullptr;}
const LifeActivityRecord* exact_activity(const WorldBundle& w,const ContinuationSnapshot& s){const auto* record=w.growth.life_activities.find(s.action_id);if(!record||record->view.action_id!=s.action_id||record->view.actor_id!=s.actor_id||record->view.kind!=LifeActivityKind::service||record->view.activity!="rest"||!std::holds_alternative<ServiceTerms>(record->terms))return nullptr;return record;}
// 当前人物收到的当面邀请可见；未到达的远方消息不参与继续判断。
bool pending_membership(const WorldBundle& w,const StableId& actor){if(!w.named_membership_enabled())return false;for(const auto& row:w.organization.membership_offers.rows()){const auto& offer=row.second;if(offer.target_actor_id==actor&&offer.status==MembershipOfferStatus::pending&&offer.created_tick<=w.common.tick()&&offer.expires_tick>w.common.tick()&&membership_fact_known(w.common,w.social,actor,offer.source_fact_id))return true;}return false;}
}
bool bounded_continuation_kind(const StableId& kind){return kind=="travel"||kind=="long_rest"||kind=="study_skill";}
Status validate_continuation_shape(const ContinuationSnapshot& s){return guarded<Done>([&]{
    require(valid_id(s.actor_id)&&valid_id(s.action_id)&&valid_id(s.action_kind)&&valid_id(s.wallet_id)&&stop_code(s.stop).has_value());
    require(!std::all_of(s.source_identity.world_id.begin(),s.source_identity.world_id.end(),[](auto value){return value==0;})&&s.source_identity.revision>=0);
    require(s.max_half_days>0&&s.max_gross_outflow>=0&&s.gross_outflow>=0&&s.gross_outflow<=s.max_gross_outflow);
    if(s.continuation_version==1){
        require(!s.exact_source_moment&&!s.exact_started_at&&!s.exact_committed_duration&&!s.action_started_source&&bounded_continuation_kind(s.action_kind));
        require(s.source_identity.execution_epoch>=0&&s.source_identity.tick>=0&&s.action_start_tick>=0&&s.started_tick>=s.action_start_tick&&s.started_tick<=s.source_identity.tick&&s.committed_half_days>=0&&s.committed_half_days<=s.max_half_days&&s.committed_half_days<=s.source_identity.tick-s.started_tick);
        return Done{};
    }
    require(s.continuation_version==2&&s.exact_source_moment&&s.exact_started_at&&s.exact_committed_duration&&s.action_started_source);
    require(exact_id(s.actor_id)&&exact_id(s.action_id)&&exact_id(s.action_kind)&&exact_id(s.wallet_id));const auto& ref=*s.action_started_source;
    require(exact_id(ref.domain)&&exact_id(ref.kind)&&exact_id(ref.id)&&ref.schema>0);
    take(encode_world_moment(*s.exact_source_moment));take(encode_world_moment(*s.exact_started_at));take(encode_canonical_fraction(s.exact_committed_duration->value));
    require(take(compare_world_moments(*s.exact_started_at,*s.exact_source_moment))<=0);
    auto elapsed=take(elapsed_world_duration(*s.exact_started_at,*s.exact_source_moment));auto maximum=take(normalize_fraction(std::to_string(s.max_half_days),"1"));
    require(take(compare_fractions(s.exact_committed_duration->value,elapsed.value))<=0&&take(compare_fractions(s.exact_committed_duration->value,maximum))<=0);
    return Done{};
});}
Status validate_continuation_snapshot(const WorldBundle& w,const ContinuationSnapshot& s){return guarded<Done>([&]{
    take(validate_continuation_shape(s));const auto& identity=w.common.state().identity;
    require(identity.world_id==s.source_identity.world_id&&identity.revision==s.source_identity.revision,"ContinuationSourceMismatch");
    const bool exact=s.continuation_version==2;require(exact==bool(w.catalog->exact_time),"ContinuationProfileMismatch");
    if(exact){require(w.world_timeline&&w.action_elapsed&&(exact_complete_selected(*w.catalog->exact_time)||exact_selected(w)),"ContinuationProfileMismatch");auto current=take(read_world_moment(w.common.tick(),*w.world_timeline));require(take(compare_world_moments(current,*s.exact_source_moment))==0,"ContinuationSourceMismatch");}
    else require(identity.tick==s.source_identity.tick,"ContinuationSourceMismatch");
    const auto* actor=w.common.actor(s.actor_id);const auto* action=w.common.action(s.action_id);const auto* wallet=w.common.wallet(s.wallet_id);
    require(s.actor_id==w.common.state().metadata.player_actor_id&&actor&&actor->wallet_id==s.wallet_id&&wallet&&wallet->owner.kind==Owner::Kind::actor&&wallet->owner.id==s.actor_id,"ContinuationActorMismatch");
    require(action&&action->actor_id==s.actor_id&&action->kind==s.action_kind,"ContinuationActionMismatch");
    if(exact&&exact_complete_selected(*w.catalog->exact_time)){
        auto access=take(w.complete_exact_time_bindings());require(bool(access),"ContinuationProfileMismatch");
        take(validate_complete_exact_continuation_source(w,access->reads(),s));
    }else if(exact){
        require(s.action_kind=="service","ContinuationProfileMismatch");take(validate_exact_rest_sources(w));const auto* first=exact_start(w,s);const auto* elapsed=exact_elapsed(w,s.action_id);const auto* record=exact_activity(w,s);
        require(first&&elapsed&&record&&record->view.phase==LifeActivityPhase::active&&!std::holds_alternative<ElapsedTerminal>(elapsed->timing),"ContinuationActionMismatch");
        require(take(compare_world_moments(first->occurred_at,elapsed->started_at))==0&&take(compare_world_moments(first->occurred_at,*s.exact_started_at))<=0,"ContinuationActionStartMismatch");
        require(action->start_tick==first->occurred_at.half_day_tick&&action->participants==record->view.participant_actor_ids&&actor->action_id==s.action_id,"ContinuationActionMismatch");
    }else{
        require(action->start_tick==s.action_start_tick,"ContinuationActionMismatch");
        // 原课程暂停解除人物占用，但同课程仍存在；唯一legacy原规则例外。
        bool paused=false;if(action->kind=="study_skill"){auto* value=w.growth.facts.find(action->id);auto* fact=value?std::get_if<StudyFact>(value):nullptr;paused=fact&&growth::study_paused(*fact);}
        require(actor->action_id==s.action_id||(actor->action_id.empty()&&paused),"ContinuationActionMismatch");
    }
    return Done{};
});}
Outcome<Bytes> encode_continuation_snapshot(const ContinuationSnapshot& s,std::size_t trusted_byte_limit){return guarded<Bytes>([&]{
    take(validate_continuation_shape(s));capacity(trusted_byte_limit>0);Bytes out{'H','M','C','N',static_cast<std::uint8_t>(s.continuation_version)};out.insert(out.end(),s.source_identity.world_id.begin(),s.source_identity.world_id.end());
    if(s.continuation_version==1){number(out,s.source_identity.execution_epoch);number(out,s.source_identity.revision);number(out,s.source_identity.tick);identifier(out,s.actor_id);identifier(out,s.action_id);identifier(out,s.action_kind);identifier(out,s.wallet_id);number(out,s.action_start_tick);number(out,s.started_tick);number(out,s.max_half_days);number(out,s.max_gross_outflow);number(out,s.committed_half_days);number(out,s.gross_outflow);number(out,*stop_code(s.stop));require(out.size()<=continuation_snapshot_byte_limit);capacity(out.size()<=trusted_byte_limit);return out;}
    number(out,s.source_identity.revision);write_leaf(out,*s.exact_source_moment,visit_world_moment2,trusted_byte_limit);identifier(out,s.actor_id,2);identifier(out,s.action_id,2);identifier(out,s.action_kind,2);identifier(out,s.wallet_id,2);
    const auto& ref=*s.action_started_source;identifier(out,ref.domain,2);identifier(out,ref.kind,2);identifier(out,ref.id,2);unsigned_number(out,ref.schema,4);write_leaf(out,*s.exact_started_at,visit_world_moment2,trusted_byte_limit);number(out,s.max_half_days);number(out,s.max_gross_outflow);write_leaf(out,*s.exact_committed_duration,visit_half_day_duration2,trusted_byte_limit);number(out,s.gross_outflow);out.push_back(*stop_code(s.stop));capacity(out.size()<=trusted_byte_limit);return out;
});}
Outcome<ContinuationSnapshot> decode_continuation_snapshot(const Bytes& bytes,std::size_t trusted_byte_limit){return guarded<ContinuationSnapshot>([&]{
    require(bytes.size()>=21&&bytes[0]=='H'&&bytes[1]=='M'&&bytes[2]=='C'&&bytes[3]=='N');const auto version=bytes[4];require(version==1||version==2);if(version==1)require(bytes.size()<=continuation_snapshot_byte_limit);capacity(trusted_byte_limit>0&&bytes.size()<=trusted_byte_limit);Reader read{bytes,21,true,trusted_byte_limit};ContinuationSnapshot s;s.continuation_version=version;std::copy(bytes.begin()+5,bytes.begin()+21,s.source_identity.world_id.begin());
    if(version==1){s.source_identity.execution_epoch=read.number();s.source_identity.revision=read.number();s.source_identity.tick=read.number();s.actor_id=read.identifier();s.action_id=read.identifier();s.action_kind=read.identifier();s.wallet_id=read.identifier();s.action_start_tick=read.number();s.started_tick=read.number();s.max_half_days=read.number();s.max_gross_outflow=read.number();s.committed_half_days=read.number();s.gross_outflow=read.number();auto stop=read_stop(read.unsigned_number(8));require(stop.has_value());s.stop=*stop;}
    else{s.source_identity.revision=read.number();s.exact_source_moment=read_leaf<WorldMoment>(read,visit_world_moment2);s.source_identity.tick=s.exact_source_moment->half_day_tick;s.actor_id=read.identifier(2);s.action_id=read.identifier(2);s.action_kind=read.identifier(2);s.wallet_id=read.identifier(2);ContinuationStartedSource source;source.domain=read.identifier(2);source.kind=read.identifier(2);source.id=read.identifier(2);source.schema=static_cast<std::uint32_t>(read.unsigned_number(4));s.action_started_source=std::move(source);s.exact_started_at=read_leaf<WorldMoment>(read,visit_world_moment2);s.started_tick=s.exact_started_at->half_day_tick;s.max_half_days=read.number();s.max_gross_outflow=read.number();s.exact_committed_duration=read_leaf<HalfDayDuration>(read,visit_half_day_duration2);s.gross_outflow=read.number();auto stop=read_stop(read.unsigned_number(1));require(stop.has_value());s.stop=*stop;
        // 旧整数读数只作兼容显示；始终由精确已用时长投影，不能用于续行余预算。
        auto rounded=take(ceil_half_day_duration(*s.exact_committed_duration));auto whole=take(normalize_fraction(std::to_string(rounded),"1"));s.committed_half_days=rounded-(take(compare_fractions(s.exact_committed_duration->value,whole))<0?1:0);
    }
    require(read.valid&&read.at==bytes.size());take(validate_continuation_shape(s));return s;
});}
// 兼容小调用只选编译期工程预算，真正槽外层可传其完整受信额度。
Outcome<Bytes> encode_continuation_snapshot(const ContinuationSnapshot& s){return encode_continuation_snapshot(s,s.continuation_version==1?continuation_snapshot_byte_limit:exact_continuation_snapshot_byte_limit);}
Outcome<ContinuationSnapshot> decode_continuation_snapshot(const Bytes& b){return decode_continuation_snapshot(b,b.size()>4&&b[4]==2?exact_continuation_snapshot_byte_limit:continuation_snapshot_byte_limit);}
// 完整profile的业务停点只经同一世界持有的原owner反读；技术失败不降成可继续许可。
Outcome<RunStop> read_original_continuation_stop(const WorldBundle& w,const ContinuationSnapshot& s){return guarded<RunStop>([&]{
    if(w.catalog&&w.catalog->exact_time&&exact_complete_selected(*w.catalog->exact_time)){
        require(s.continuation_version==2,"ContinuationProfileMismatch");
        const auto* actor=w.common.actor(s.actor_id);if(!actor||s.actor_id!=w.common.state().metadata.player_actor_id||!actor->alive||!actor->available||actor->wallet_id!=s.wallet_id)return RunStop::blocked;
        if(pending_encounter(w.interaction,s.actor_id)||w.services.in_open_battle(s.actor_id)||!siege_for_actor(w.military,s.actor_id).empty()||pending_membership(w,s.actor_id))return RunStop::awaiting_input;
        for(const auto& session:w.interaction.interactions)if(session.actor_id==s.actor_id&&!(session.phase=="waiting"&&session.pending_action_id==s.action_id))return RunStop::awaiting_input;
        auto access=take(w.complete_exact_time_bindings());require(bool(access),"ContinuationProfileMismatch");
        return take(read_complete_exact_continuation_stop(w,access->reads(),s));
    }
    return continuation_domain_stop(w,s);
});}
RunStop continuation_domain_stop(const WorldBundle& w,const ContinuationSnapshot& s){
    if(w.catalog&&w.catalog->exact_time&&exact_complete_selected(*w.catalog->exact_time)){auto stopped=read_original_continuation_stop(w,s);return stopped.ok?stopped.value:RunStop::blocked;}
    const auto* actor=w.common.actor(s.actor_id);if(!actor||s.actor_id!=w.common.state().metadata.player_actor_id||!actor->alive||!actor->available||actor->wallet_id!=s.wallet_id)return RunStop::blocked;
    if(pending_encounter(w.interaction,s.actor_id)||w.services.in_open_battle(s.actor_id)||!siege_for_actor(w.military,s.actor_id).empty()||pending_membership(w,s.actor_id))return RunStop::awaiting_input;
    for(const auto& session:w.interaction.interactions)if(session.actor_id==s.actor_id&&!(session.phase=="waiting"&&session.pending_action_id==s.action_id))return RunStop::awaiting_input;
    const auto* action=w.common.action(s.action_id);
    if(w.catalog->exact_time||s.continuation_version==2){
        if(s.continuation_version!=2||s.action_kind!="service"||!exact_selected(w))return RunStop::blocked;const auto* first=exact_start(w,s);const auto* elapsed=exact_elapsed(w,s.action_id);const auto* record=exact_activity(w,s);if(!first||!elapsed||!record)return RunStop::blocked;
        if(!action){if(!actor->action_id.empty())return RunStop::action_replaced;const auto* terminal=std::get_if<ElapsedTerminal>(&elapsed->timing);if(!terminal||terminal->outcome_ref!=s.action_id||record->view.phase!=LifeActivityPhase::completed||!record->view.success)return RunStop::blocked;auto completed=ExactRestTimeRead(w.growth).completed_rest_at(s.action_id);auto now=read_world_moment(w.common.tick(),*w.world_timeline);if(!completed.ok||!now.ok)return RunStop::blocked;auto since=compare_world_moments(first->occurred_at,completed.value);auto past=compare_world_moments(completed.value,now.value);auto settled=compare_world_moments(completed.value,elapsed->last_settled_at);if(!since.ok||!past.ok||!settled.ok||since.value>0||past.value>0||settled.value!=0)return RunStop::blocked;return RunStop::completed;}
        if(action->kind!=s.action_kind||action->actor_id!=s.actor_id||action->start_tick!=first->occurred_at.half_day_tick||actor->action_id!=s.action_id)return RunStop::action_replaced;
        if(record->view.phase!=LifeActivityPhase::active||std::holds_alternative<ElapsedTerminal>(elapsed->timing))return RunStop::blocked;
        // 本首片未选择业务暂停/恢复来源，不能仅见Suspended便发继续许可。
        return std::holds_alternative<ElapsedRunning>(elapsed->timing)?RunStop::awaiting_permit:RunStop::blocked;
    }
    if(s.continuation_version!=1)return RunStop::blocked;
    if(!action){
        if(!actor->action_id.empty())return RunStop::action_replaced;
        if(s.action_kind=="travel")return RunStop::arrived;
        const auto* value=w.growth.facts.find(s.action_id);if(!value)return RunStop::blocked;
        if(const auto* rest=std::get_if<RestFact>(value))return rest->phase=="completed"&&rest->stop_reason=="duration"?RunStop::completed:RunStop::blocked;
        if(const auto* study=std::get_if<StudyFact>(value))return study->phase=="completed"&&study->success?RunStop::completed:RunStop::blocked;
        return RunStop::blocked;
    }
    if(action->kind!=s.action_kind||action->actor_id!=s.actor_id||action->start_tick!=s.action_start_tick)return RunStop::action_replaced;
    if(s.action_kind=="study_skill"){auto* value=w.growth.facts.find(s.action_id);auto* fact=value?std::get_if<StudyFact>(value):nullptr;if(!fact)return RunStop::blocked;if(growth::study_paused(*fact))return RunStop::paused;if(fact->challenge&&growth::challenge_pending(*fact->challenge))return RunStop::awaiting_input;}
    if(actor->action_id!=s.action_id)return RunStop::action_replaced;
    if(s.action_kind=="travel"){auto state=travel_encounter(w.travel,s.action_id);if(!state.ok)return RunStop::blocked;if(state.value.second)return RunStop::paused;}
    return RunStop::awaiting_permit;
}
} // 恢复只安装停驻说明；不推进世界、不生成permit、不消费请求序号。
