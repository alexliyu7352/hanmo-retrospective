#pragma once
#include "social_event_original_source.hpp"
#include "resident/domains/family/birth_due_execution_codec_fields.hpp"
namespace hanmo::resident::social_event_application_codec {
template<class C>Status fields(C&c,SocialEventOriginalApplication&r){
 c.object("original_period_opening",[&](auto&x){return birth_due_codec::moment_source(x,r.original_period_opening);});
 c.object("occurred_at",[&](auto&x){return family_life4_codec::moment(x,r.occurred_at);});
 if(!c.status().ok)return c.status();auto valid=validate_social_event_original_application_shape(r);return valid.ok?c.status():c.reject(valid.error.original.code);
}
}
