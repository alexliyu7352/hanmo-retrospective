#include "growth_source_pins.hpp"
#include "interaction_source_pins.hpp"
#include "common_command_sources.hpp"
#include "resident/storage/common_command_publication.hpp"
#include "resident/domains/family/control_continuation.hpp"
#include "resident/domains/family/control_retirement_cancel.hpp"
#include "native_control_continuation.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "resident/domains/growth/growth_command_result_retention.hpp"
#include "resident/domains/director/director_command_acceptance.hpp"
#include "resident/domains/tasks/task_claim_establishment.hpp"
#include "resident/domains/tasks/task_quantity_time2.hpp"
#include "player_plan_integration.hpp"
#include "native_interaction_catalog.hpp"
#include "resident/domains/interaction/interaction_response_sources.hpp"
#include <charconv>
#include <limits>
namespace hanmo::resident {
Outcome<CommonCommandSourceView> read_common_command_source(const CommonStore& common,const SourceRef& source){
    if(source.domain!="common"||source.kind!="command_result"||source.id.empty())return {false,{},common_failure("InvalidCommonCommandSource",source.id).error};
    return read_common_command_source(common,std::string_view(source.id));
}
Outcome<CommonCommandSourceView> read_common_command_source(const CommonStore& common,std::string_view result_id){
    if(result_id.empty())return {false,{},common_failure("InvalidCommonCommandSource").error};
    CommonCommandSourceView view;
    for(const auto& row:common.state().results.rows())if(row.second&&row.second->result_id==result_id){
        if(view.result)return {false,{},common_failure("AliasedCommonCommandSource",StableId(result_id)).error};view.result=row.second.get();view.cache_key=row.first;
    }
    for(const auto& row:common.state().history)if(row.result_id==result_id){
        if(view.history)return {false,{},common_failure("DuplicateCommonCommandHistory",StableId(result_id)).error};view.history=&row;
    }
    if(!view.result||!view.history||!view.result->ok||view.result->kind!=view.history->kind||view.result->tick!=view.history->tick||view.result->revision<1||view.result->revision>common.revision()||view.result->tick>common.tick()||!common.actor(view.history->actor_id))return {false,{},common_failure("ActualCommonCommandSourceRequired",StableId(result_id)).error};
    const auto& actor=view.history->actor_id;
    if(view.cache_key.size()<=actor.size()+1||view.cache_key.substr(0,actor.size())!=actor||view.cache_key[actor.size()]!=':')return {false,{},common_failure("ActualCommonCommandCacheKeyRequired",StableId(result_id)).error};
    const auto sequence=view.cache_key.substr(actor.size()+1);
    const auto parsed=std::from_chars(sequence.data(),sequence.data()+sequence.size(),view.request_seq);
    const auto* current_sequence=common.state().request_sequences.find(actor);
    if(sequence.front()<'1'||sequence.front()>'9'||parsed.ec!=std::errc{}||parsed.ptr!=sequence.data()+sequence.size()||view.request_seq<1||!current_sequence||*current_sequence<view.request_seq)return {false,{},common_failure("ActualCommonCommandSequenceRequired",StableId(result_id)).error};
    return {true,view,{}};
}
// 只有原业务写入成功后的同候选调用可封口；失败仍由外层丢弃整个世界候选。
Outcome<std::shared_ptr<const CommitResult>> seal_complete_command_result_in_candidate(
    WorldBundle& world,const StableId& actor,Integer sequence,const StableId& kind,const ReceiptDetails& details){
    if(!world.catalog||!world.catalog->exact_time||!exact_complete_selected(*world.catalog->exact_time)||
       !world.active_exact_write_context()||!world.common.actor_condition_capture_identity()||!world.read_stamps)
        return {false,{},common_failure("CompleteCommandActualCaptureRequired").error};
    auto& state=world.common.state();
    const auto* previous=state.request_sequences.find(actor);
    if(!world.common.actor(actor)||kind.empty()||!previous||sequence<1||sequence>2147483647||sequence<=*previous)
        return {false,{},common_failure("CompleteCommandActualSequenceRequired",actor).error};
    const StableId key=actor+":"+std::to_string(sequence);
    if(state.results.find(key))return {false,{},common_failure("CompleteCommandResultAlreadySealed",key).error};
    auto now=world.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
    if(now.value.half_day_tick!=world.common.tick())
        return {false,{},common_failure("CompleteCommandActualMomentMismatch",actor).error};
    ReadStampWriteBoundary boundary(*world.read_stamps);
    auto publication =
        reserve_complete_command_publication_revision(world.common);
    if (!publication.ok)
        return {false, {}, std::move(publication.error)};
    state.request_sequences.set(actor, sequence);
    auto id=world.common.next_id("result");if(!id.ok)return {false,{},std::move(id.error)};
    auto receipt=checkpoint_make_shared<CommitResult>();
    receipt->kind=kind;receipt->tick=world.common.tick();receipt->revision=world.common.revision();
    receipt->result_id=std::move(id.value);
    // Only the private candidate copy is mutable. The self-reference uses the
    // actual Common result ID/revision, before any immutable result is stored.
    auto sealed_details=checkpoint_make_shared<ReceiptDetails>(details);
    auto* continuation=std::get_if<ContinuationBoundaryReceipt>(&sealed_details->data);
    if((kind=="stabilize_continuation_boundary")!=(continuation!=nullptr))
        return {false,{},common_failure("ContinuationBoundaryReceiptKindMismatch",receipt->result_id).error};
    if(continuation){
        auto same_moment=compare_world_moments(continuation->actual_at,now.value);
        if(!same_moment.ok)return {false,{},std::move(same_moment.error)};
        if(same_moment.value)return {false,{},common_failure("ContinuationBoundaryActualMomentMismatch",receipt->result_id).error};
        // The original self-reference is a new owned field, not part of the
        // pre-seal DTO copy. Retain its fresh bytes before the K3 binder copies.


        auto self_memory=CheckpointCopyLease::reserve();
        sealed_details->copy_memory=CheckpointCopyLease::hold_both(sealed_details->copy_memory,self_memory);
        auto bound=bind_continuation_boundary_receipt_seal_in_candidate(world.common,*continuation,*receipt);
        if(!bound.ok)return {false,{},std::move(bound.error)};
    }
    receipt->details=std::move(sealed_details);
    state.results.set(key,receipt);
    state.history.push_back({world.common.tick(),kind,actor,receipt->result_id,{}});
    world.note_write_boundary();
    return {true,std::move(receipt),{}};
}
SourceRetentionRegistration make_common_command_source_registration(){
    return {{"common","command_result"},
        [](const WorldBundle& world,const SourceRef& source){auto actual=read_common_command_source(world.common,source);return actual.ok?common_success():Status{false,{},actual.error};},
        [](const WorldBundle& world,const SourceRef& source)->Outcome<bool>{auto actual=read_common_command_source(world.common,source);if(!actual.ok)return {false,false,actual.error};return {true,true,{}};}};
}
namespace {
// 只按原result的规范十进制int64序号比较；不以字串或浮点排列同版本结果。
std::optional<Integer> command_result_serial(std::string_view id) {
    constexpr std::string_view prefix = "result:";
    if (id.substr(0, prefix.size()) != prefix)
        return {};
    const auto suffix = id.substr(prefix.size());
    if (suffix.empty() || suffix.front() < '1' || suffix.front() > '9')
        return {};
    Integer number = 0;
    const auto parsed =
        std::from_chars(suffix.data(), suffix.data() + suffix.size(), number);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != suffix.data() + suffix.size() || number < 1)
        return {};
    return number;
}
class ActualCommandPins final:public GrowthCommandResultSink {
    const WorldBundle& world_;
public:
    CheckpointVector<std::string_view> result_ids;
    explicit ActualCommandPins(const WorldBundle& world):world_(world){}
    bool contains(std::string_view id)const {for(const auto& actual:result_ids)if(actual==id)return true;return false;}
    Status add(std::string_view actor,std::string_view kind,Integer sequence,std::string_view id,std::optional<Integer> revision={}){
        const auto* request=world_.common.state().request_sequences.find(StableId(actor));
        const StableId key=StableId(actor)+":"+std::to_string(sequence);
        const auto* cached=world_.common.state().results.find(key);
        if(sequence<1||!request||*request<sequence||!cached||!*cached||(*cached)->result_id!=id||(*cached)->kind!=kind||(revision&&(*cached)->revision!=*revision))return common_failure("PinnedCommonCommandIdentityMismatch",StableId(id));
        auto original=read_common_command_source(world_.common,{"common","command_result",StableId(id)});if(!original.ok)return {false,{},original.error};
        if(original.value.result!=cached->get()||original.value.history->actor_id!=actor||original.value.cache_key!=key)return common_failure("PinnedCommonCommandSourceMismatch",StableId(id));
        if(!contains(id))result_ids.push_back(id);return common_success();
    }
    Status actual_reference(const GrowthCommandResultReference& ref)override{return add(ref.actor_id,ref.command_kind,ref.request_seq,ref.result_id,ref.committed_revision);}
    // The original owner has already validated the reference and its causal
    // payload. Preserve the actual cached result/history identity, including
    // shared Advance results whose requester is not the completed Task actor.
    Status original_result(const SourceRef& ref){
        auto original=read_common_command_source(world_.common,ref);
        if(!original.ok)return {false,{},std::move(original.error)};
        return add(original.value.history->actor_id,original.value.result->kind,
                   original.value.request_seq,original.value.result->result_id,
                   original.value.result->revision);
    }
};
class InteractionCommandPins final:public InteractionResponseSourcePinSink,public InteractionSessionLifeSourcePinSink {
    ActualCommandPins& pins_;
public:
    explicit InteractionCommandPins(ActualCommandPins&pins):pins_(pins){}
    Status original_source(const StableId&,InteractionSessionLifePinSlot slot,Integer,
                           const SourceRef&ref,Integer schema)override{
        if(slot!=InteractionSessionLifePinSlot::preserved_result){
            if(ref.domain=="common")return common_failure("InteractionLifeNonResultPinMismatch",ref.id,ErrorKind::internal);
            return common_success();
        }
        if(ref.domain!="common")return common_failure("InteractionLifeResultPinMismatch",ref.id,ErrorKind::internal);
        return original_source(ref,schema);
    }
    Status original_source(const SourceRef&ref,Integer schema)override{
        // The original response visitor has verified the opening/response
        // closure. This collector owns only Common command-result retention.
        if(ref.domain!="common")return common_success();
        if(ref.kind!="command_result"||schema!=1)return common_failure("InteractionCommonResultPinMismatch",ref.id,ErrorKind::internal);
        return pins_.original_result(ref);
    }
};
class ContinuationCommandPins final:public ContinuationBoundarySourceSink {
    const WorldBundle& world_;ActualCommandPins& pins_;
public:
    ContinuationCommandPins(const WorldBundle& world,ActualCommandPins& pins):world_(world),pins_(pins){}
    Status sealed_command(const SourceRef& source)override{return pins_.original_result(source);}
    Status original_consequence(const MomentOwnerSource& source)override{
        // This collector only owns the Common result/history eviction set.
        // Other source owners remain in the full derived source-PIN registry.
        if(source.source_ref.domain!="common")return common_success();
        if(source.source_ref.kind!="command_result"||source.source_schema!=1)
            return common_failure("ContinuationCommonResultPinMismatch",source.source_ref.id,ErrorKind::internal);
        return pins_.original_result(source.source_ref);
    }
    Status captured_candidate_prefix(const CommonActualFactPrefix& prefix)override{
        auto actual=read_actual_common_fact_prefix(world_.common,prefix);
        return actual.ok?common_success():Status{false,{},std::move(actual.error)};
    }
};
Status collect(const WorldBundle& world,ActualCommandPins& pins){
    if(!world.catalog->exact_time||!exact_complete_selected(*world.catalog->exact_time))return common_failure("CompleteCommandRetentionSelectionRequired");
    auto valid=visit_growth_command_result_references(world.growth,world.growth_tactic_lessons_enabled()?&world.growth_tactic_lessons:nullptr,world.personal_recovery_enabled()?&world.personal_recovery:nullptr,pins);if(!valid.ok)return valid;
    if(world.director.subject2)for(const auto& row:world.director.subject2->adoption_results.rows())if(row.second.accepted_command){
        const auto& command=*row.second.accepted_command;
        if(!world.catalog->director)return common_failure("DirectorCommandSourceCatalogRequired");
        auto actual=read_director_accepted_command(world.common,world.director,*world.catalog->director,world.catalog->identity,{row.second.subject.id,row.second.registration_epoch,row.second.decision_seq,command.command_kind,command.request_seq,command.result_id});if(!actual.ok)return {false,{},actual.error};
        valid=pins.add(command.actor_id,command.command_kind,command.request_seq,command.result_id);if(!valid.ok)return valid;
    }
    if(world.tasks.claim_establishment){
        for(const auto& row:world.tasks.claim_establishment->claim_establishment_sources.rows()){
            auto original=read_task_claim_established_source(world.common,world.tasks,world.affairs,
                {{"tasks","task_claim_established",row.first},1});
            if(!original.ok)return {false,{},std::move(original.error)};
            valid=pins.original_result(original.value->committed_result_ref);if(!valid.ok)return valid;
        }
    }
    if(world.tasks.exact_quantities){
        // Only original Task source validation grants retention. A bare ref
        // cannot keep an unrelated Common result alive or stand in for a seal.
        valid=validate_exact_quantity_result_bindings(world.common,world.tasks);if(!valid.ok)return valid;
        for(const auto& row:world.tasks.tasks.rows())if(row.second.exact_quantity){
            const auto& original=*row.second.exact_quantity;
            if(original.original_acceptance_result_ref){valid=pins.original_result(*original.original_acceptance_result_ref);if(!valid.ok)return valid;}
            if(original.original_report_result_ref){valid=pins.original_result(*original.original_report_result_ref);if(!valid.ok)return valid;}
        }
        for(const auto& row:world.tasks.transfers.rows())if(row.second.original_completion_result_ref){
            valid=pins.original_result(*row.second.original_completion_result_ref);if(!valid.ok)return valid;
        }
        for(const auto& row:world.tasks.deliveries.rows())if(row.second.original_completion_result_ref){
            valid=pins.original_result(*row.second.original_completion_result_ref);if(!valid.ok)return valid;
        }
    }
    valid=validate_native_control_continuation_layout(world);if(!valid.ok)return valid;
    if(world.control_continuation){
        const auto& controls=*world.control_continuation;
        // Activation rows already retain their actual result identity. A
        // future cache eviction must not erase the original control switch.
        for(const auto& row:controls.activation_records.rows()){
            const auto& activation=row.second;
            valid=pins.add(activation.predecessor_actor_id,activation.command_kind,
                activation.request_origin.request_seq,activation.command_result_ref,
                activation.committed_revision);if(!valid.ok)return valid;
        }
        if(world.catalog->control_continuation->selected_version==3){
            ContinuationCommandPins original(world,pins);
            // Preserve every real generation source, including cancellation
            // before any local segment. Do not retain only the latest slice.
            for(const auto& row:controls.pending_continuations.rows()){
                valid=visit_continuation_pending_generation_source_refs(world.common,controls,
                    row.first,original);if(!valid.ok)return valid;
            }
            for(const auto& row:world.common.state().results.rows())if(row.second&&row.second->details&&
                std::holds_alternative<ContinuationBoundaryReceipt>(row.second->details->data)){
                valid=visit_continuation_boundary_receipt_source_refs(world.common,controls,
                    {"common","command_result",row.second->result_id},original);
                if(!valid.ok)return valid;
            }
        }
    }
    valid=validate_native_interaction_response_layout(*world.catalog,world.interaction);if(!valid.ok)return valid;
    if(world.interaction.response_sources){
        InteractionCommandPins original(pins);
        valid=visit_interaction_response_source_pins(world.common,world.interaction,*world.catalog->interaction,original);if(!valid.ok)return valid;
        // This is the stable post-seal retention path. The full original Life
        // walk also verifies non-Common dependencies; only actual Common
        // results enter this cache-pruning set. Release PINs use all five slots.
        valid=visit_complete_interaction_life_pins(world,original);if(!valid.ok)return valid;
    }
    // Existing Plan pins continue using the original owner; they are never lost
    // merely because the complete profile adds other persistent consumers.
    if(world.player_plan_enabled())for(const auto& row:world.common.state().results.rows())if(row.second&&player_plan_pins_result(world.player_plan,row.second->result_id)){
        auto actual=read_common_command_source(world.common,{"common","command_result",row.second->result_id});if(!actual.ok)return {false,{},actual.error};
        valid=pins.add(actual.value.history->actor_id,row.second->kind,actual.value.request_seq,row.second->result_id,row.second->revision);if(!valid.ok)return valid;
    }
    return common_success();
}
}
Status validate_complete_command_source_pins(const WorldBundle& world){ActualCommandPins pins(world);auto valid=collect(world,pins);if(!valid.ok)return valid;return validate_growth_original_fact_prefixes(world);}
Status prune_complete_command_sources(WorldBundle& world){
    ActualCommandPins pins(world);auto valid=collect(world,pins);if(!valid.ok)return valid;
    auto& state=world.common.state();std::size_t unpinned=0;
    for(const auto& row:state.results.rows())if(!row.second||!pins.contains(row.second->result_id))++unpinned;
    while (unpinned > 64) {
        const StableId *oldest = nullptr;
        Integer revision = std::numeric_limits<Integer>::max(), serial = 0;
        for (const auto &row : state.results.rows())
            if (row.second && !pins.contains(row.second->result_id)) {
                const auto number =
                    command_result_serial(row.second->result_id);
                if (!number || *number >= state.next_instance_id)
                    return common_failure("InvalidCachedResult",
                                          "results/" + row.first);
                if (!oldest || row.second->revision < revision ||
                    (row.second->revision == revision && *number < serial)) {
                    revision = row.second->revision;
                    serial = *number;
                    oldest = &row.first;
                }
            }
        if (!oldest)
            return common_failure("ResultEvictionFailed", {},
                                  ErrorKind::internal);
        StableId key = *oldest;
        state.results.erase(key);
        --unpinned;
    }
    unpinned=0;for(const auto& row:state.history)if(!pins.contains(row.result_id))++unpinned;
    for(auto it=state.history.begin();unpinned>2000&&it!=state.history.end();){if(pins.contains(it->result_id)){++it;continue;}it=state.history.erase(it);--unpinned;}
    return common_success();
}
}
