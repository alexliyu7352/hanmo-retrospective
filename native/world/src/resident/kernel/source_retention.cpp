#include "source_retention.hpp"
#include "world_bundle.hpp"
#include "world_copy_plan.hpp"
#include <algorithm>
#include <type_traits>
namespace hanmo::resident {namespace {
bool same_kind(const RetentionSourceKind& k,const SourceRef& r)noexcept{return k.domain==r.domain&&k.kind==r.kind;}
bool same_kind(const RetentionSourceKind& a,const RetentionSourceKind& b)noexcept{return a.domain==b.domain&&a.kind==b.kind;}
template<class A,class B>bool same_slot(const A& a,const B& b)noexcept{return a.domain==b.domain&&a.kind==b.kind&&a.slot==b.slot;}
const SourceRetentionRegistration* source_registration(const SourceRetentionRegistry& r,const SourceRef& source){for(const auto& x:r.sources)if(same_kind(x.source_kind,source))return &x;return nullptr;}
bool same_consumer(const ConsumerRef& c,const SourcePinBuildView& v)noexcept{return std::string_view(c.domain)==v.consumer_domain&&std::string_view(c.kind)==v.consumer_kind&&std::string_view(c.id)==v.consumer_id&&std::string_view(c.slot)==v.consumer_slot;}
bool same_pin_key(const SourcePin& p,const SourcePinBuildView& v)noexcept{return same_consumer(p.consumer,v)&&std::string_view(p.source.domain)==v.source_domain&&std::string_view(p.source.kind)==v.source_kind&&std::string_view(p.source.id)==v.source_id;}
StableId fresh_text(std::string_view value){return value.empty()?StableId{}:StableId(value.data(),value.size());}
std::size_t consumer_id_size(std::initializer_list<std::string_view> pieces){
    std::size_t size=0;for(const auto piece:pieces){if(piece.size()>SIZE_MAX-size)throw std::bad_alloc();size+=piece.size();}return size;
}
CheckpointMemoryCharge consumer_id_memory(std::size_t size){return CheckpointMemoryCharge(0);}
}
SourcePinConsumerId::SourcePinConsumerId(std::initializer_list<std::string_view> pieces)
    :SourcePinConsumerId(pieces,consumer_id_size(pieces)){}
SourcePinConsumerId::SourcePinConsumerId(std::initializer_list<std::string_view> pieces,std::size_t size)
    :memory_(consumer_id_memory(size)),value_(size,'\0'){
    auto out=value_.begin();for(const auto piece:pieces)out=std::copy(piece.begin(),piece.end(),out);
}
Status validate_source_retention_registry(const SourceRetentionRegistry& registry){
    // Compare borrowed original rows, not allocated tuple/set/map copies.
    if(registry.required_consumer_slots.empty())return common_failure("CompleteRetentionConsumersRequired");
    for(std::size_t i=0;i<registry.required_consumer_slots.size();++i){const auto& x=registry.required_consumer_slots[i];
        if(x.domain.empty()||x.kind.empty()||x.slot.empty())return common_failure("InvalidRequiredRetentionConsumer");
        for(std::size_t j=0;j<i;++j)if(same_slot(x,registry.required_consumer_slots[j]))return common_failure("InvalidRequiredRetentionConsumer");
    }
    for(std::size_t i=0;i<registry.sources.size();++i){const auto& x=registry.sources[i];
        if(x.source_kind.domain.empty()||x.source_kind.kind.empty()||!x.validate_actual_source||!x.owner_may_release)return common_failure("InvalidRetentionSourceRegistration");
        for(std::size_t j=0;j<i;++j)if(same_kind(x.source_kind,registry.sources[j].source_kind))return common_failure("InvalidRetentionSourceRegistration");
    }
    for(std::size_t i=0;i<registry.consumers.size();++i){const auto& x=registry.consumers[i];
        if(x.domain.empty()||x.kind.empty()||x.slot.empty()||!x.max_references_per_consumer||!x.enumerate_actual_pins||!x.validate_actual_pin||x.allowed_source_kinds.empty())return common_failure("InvalidRetentionConsumerRegistration");
        for(std::size_t j=0;j<i;++j)if(same_slot(x,registry.consumers[j]))return common_failure("InvalidRetentionConsumerRegistration");
        for(std::size_t j=0;j<x.allowed_source_kinds.size();++j){const auto& kind=x.allowed_source_kinds[j];bool known=false;
            for(const auto& owner:registry.sources)if(same_kind(kind,owner.source_kind)){known=true;break;}
            if(!known)return common_failure("UnknownRetentionSourceKind",kind.kind);
            for(std::size_t k=0;k<j;++k)if(same_kind(kind,x.allowed_source_kinds[k]))return common_failure("UnknownRetentionSourceKind",kind.kind);
        }
    }
    if(registry.consumers.size()!=registry.required_consumer_slots.size())return common_failure("IncompleteRetentionConsumerCoverage");
    for(const auto& required:registry.required_consumer_slots){bool found=false;for(const auto& slot:registry.consumers)if(same_slot(required,slot)){found=true;break;}if(!found)return common_failure("IncompleteRetentionConsumerCoverage");}
    return common_success();
}
class SourcePinCollector final:public SourcePinSink {
    const SourceRetentionRegistry& registry_;const WorldBundle& world_;
    const SourceConsumerRegistration& registration_;DerivedSourcePins& result_;
    Status status_=common_success();
    Status fail(Status s){status_=std::move(s);return status_;}
public:
    SourcePinCollector(const SourceRetentionRegistry& registry,const WorldBundle& world,const SourceConsumerRegistration& registration,DerivedSourcePins& result)
        :registry_(registry),world_(world),registration_(registration),result_(result){}
    const Status& status()const noexcept{return status_;}
    bool contains(const SourcePinBuildView& view)const noexcept override{for(const auto& pin:result_.pins)if(same_pin_key(pin,view))return true;return false;}
    Status append(const SourcePinBuildView& view)override{
        if(!status_.ok)return status_;
        (result_.payload_memory_).grow(0);
        SourcePin pin{{fresh_text(view.consumer_domain),fresh_text(view.consumer_kind),fresh_text(view.consumer_id),fresh_text(view.consumer_slot)},
                      {fresh_text(view.source_domain),fresh_text(view.source_kind),fresh_text(view.source_id)},view.reason};
        const auto reason=static_cast<Integer>(view.reason);
        if(view.consumer_domain!=std::string_view(registration_.domain)||view.consumer_kind!=std::string_view(registration_.kind)||view.consumer_slot!=std::string_view(registration_.slot)||view.consumer_id.empty()||view.source_domain.empty()||view.source_kind.empty()||view.source_id.empty()||reason<0||reason>4)
            return fail(common_failure("InvalidActualSourcePin",pin.consumer.id));
        bool allowed=false;for(const auto& kind:registration_.allowed_source_kinds)if(std::string_view(kind.domain)==view.source_domain&&std::string_view(kind.kind)==view.source_kind){allowed=true;break;}
        std::size_t count=0;for(const auto& pin:result_.pins)if(same_consumer(pin.consumer,view))++count;
        // Original ordering: maximum-reference failure precedes duplicate failure.
        if(!allowed||count>=registration_.max_references_per_consumer)return fail(common_failure("SourcePinRegistrationExceeded",pin.consumer.id));
        if(contains(view))return fail(common_failure("DuplicateActualSourcePin",pin.consumer.id));
        auto valid=registration_.validate_actual_pin(world_,pin);if(!valid.ok)return fail(std::move(valid));
        const auto* owner=source_registration(registry_,pin.source);if(!owner)return fail(common_failure("UnknownRetentionSourceKind",pin.source.kind));
        valid=owner->validate_actual_source(world_,pin.source);if(!valid.ok)return fail(std::move(valid));
        static_assert(std::is_nothrow_move_constructible_v<SourcePin>,"Tracked PIN shell growth must not copy ordinary payload");
        result_.pins.push_back(std::move(pin));return common_success();
    }
};
Outcome<DerivedSourcePins> collect_actual_source_pins(const SourceRetentionRegistry& registry,const WorldBundle& world){
    auto valid=validate_source_retention_registry(registry);if(!valid.ok)return {false,{},valid.error};
    DerivedSourcePins result;
    for(const auto& registration:registry.consumers){SourcePinCollector sink(registry,world,registration,result);valid=registration.enumerate_actual_pins(world,sink);
        // An enumerator cannot erase an earlier sink rejection by ignoring Status.
        if(!sink.status().ok)return {false,{},sink.status().error};if(!valid.ok)return {false,{},valid.error};
    }
    return {true,std::move(result),{}};
}
Outcome<bool> source_may_release(const SourceRetentionRegistry& registry,const WorldBundle& world,const SourceRef& source){
    auto valid=validate_source_retention_registry(registry);if(!valid.ok)return {false,false,valid.error};
    auto actual=collect_actual_source_pins(registry,world);if(!actual.ok)return {false,false,actual.error};
    const auto* owner=source_registration(registry,source);if(!owner)return {false,false,common_failure("UnknownRetentionSourceKind",source.kind).error};
    valid=owner->validate_actual_source(world,source);if(!valid.ok)return {false,false,valid.error};
    for(const auto& pin:actual.value.pins)if(same_source_ref(pin.source,source))return {true,false,{}};
    return owner->owner_may_release(world,source);
}
Outcome<SourceReleaseCandidates> prepare_source_release_candidates(const SourceRetentionRegistry& registry,const WorldBundle& world,const std::vector<SourceRef>& candidates){
    auto actual=collect_actual_source_pins(registry,world);if(!actual.ok)return {false,{},actual.error};
    SourceReleaseCandidates result;
    for(std::size_t i=0;i<candidates.size();++i){const auto& source=candidates[i];
        if(!valid_source_ref(source))return {false,{},common_failure("DuplicateSourceReleaseCandidate",source.id).error};
        for(std::size_t j=0;j<i;++j)if(same_source_ref(source,candidates[j]))return {false,{},common_failure("DuplicateSourceReleaseCandidate",source.id).error};
        const auto* owner=source_registration(registry,source);if(!owner)return {false,{},common_failure("UnknownRetentionSourceKind",source.kind).error};
        auto valid=owner->validate_actual_source(world,source);if(!valid.ok)return {false,{},valid.error};
        bool pinned=false;for(const auto& pin:actual.value.pins)if(same_source_ref(pin.source,source)){pinned=true;break;}if(pinned)continue;
        auto allowed=owner->owner_may_release(world,source);if(!allowed.ok)return {false,{},allowed.error};
        if(allowed.value){(result.payload_memory_).grow(0);
            static_assert(std::is_nothrow_move_constructible_v<SourceRef>,"Tracked source shell growth must not copy ordinary payload");
            result.sources.push_back(SourceRef{checkpoint_fresh_string_copy(source.domain),checkpoint_fresh_string_copy(source.kind),checkpoint_fresh_string_copy(source.id)});
        }
    }
    return {true,std::move(result),{}};
}
}
