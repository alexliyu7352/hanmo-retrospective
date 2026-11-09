#include "session_rng_initialization.hpp"
#include "resident/model/common_rng.hpp"
#include "resident/model/common_string_hash.hpp"
namespace hanmo::resident {
namespace {
constexpr const char* original_backend="godot:4.7.2-stable (official)";
}
Outcome<OriginalSessionRngInitialization> derive_original_session_rng_initialization(
    const CommonState& common,const StableId& session,Integer generation,
    const SourceRef& contact,const SourceRef& initialization){
    if(session.empty()||generation<=0||!valid_source_ref(contact)||!valid_source_ref(initialization))
        return {false,{},common_failure("SessionRandomActualInitializationRequired",session).error};
    if(common.metadata.rng_backend!=original_backend)
        return {false,{},common_failure("SessionRandomBackendMismatch",session).error};
    const auto* battle=common.rng.find("battle");
    if(!battle)return {false,{},common_failure("UnknownRandomStream","battle").error};
    auto session_hash=godot_string_hash_utf8(session);
    if(!session_hash.ok)return {false,{},session_hash.error};
    auto contact_hash=godot_string_hash_utf8(contact.id);
    if(!contact_hash.ok)return {false,{},contact_hash.error};
    // Same bit formula as original create_military_battle/naval battle entry.
    // battle.state and mutable Common revisions are deliberately not seed inputs.
    const auto seed_bits=rng_detail::unsigned_bits(battle->seed)
        ^rng_detail::unsigned_bits(session_hash.value)^rng_detail::unsigned_bits(contact_hash.value);
    const auto initial_state=godot_pcg32_seed_state(rng_detail::signed_bits(seed_bits));
    return {true,{initialization,session,generation,contact,original_backend,
        std::to_string(seed_bits),std::to_string(rng_detail::unsigned_bits(initial_state)),0},{}};
}
Status validate_original_session_rng_initialization(const CommonState& common,const OriginalSessionRngInitialization& saved){
    auto expected=derive_original_session_rng_initialization(common,saved.session_ref,saved.generation,
        saved.entry_or_contact_source_ref,saved.initialization_source_ref);
    if(!expected.ok)return {false,{},expected.error};
    if(saved.algorithm_version!=expected.value.algorithm_version||saved.seed!=expected.value.seed
        ||saved.state!=expected.value.state||saved.draw_ordinal!=0)
        return common_failure("SessionRandomInitializationMismatch",saved.session_ref);
    return {true,{}, {}};
}
}
