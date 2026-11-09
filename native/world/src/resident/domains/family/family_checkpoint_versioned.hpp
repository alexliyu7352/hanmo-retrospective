// 显式Family段版本是读写形状唯一输入；原公开旧口仍保持原字节。
#pragma once
#include "family_model.hpp"
#include "family_codec_version.hpp"
#include "family_projection.hpp"
namespace hanmo::resident {
Status visit_family_checkpoint_versioned(CodecVisitor&,FamilyState&,Integer family_codec_version,bool extended_skills=false);
Outcome<FamilyState> decode_family_seed_versioned(const BoundaryValue&,Integer family_codec_version,bool extended_skills=false,bool biological_birth=false);
Status emit_family_state_versioned_fields(ProjectionWriter&,const FamilyState&,Integer family_codec_version);
Status emit_family_state_versioned_projection(ProjectionWriter&,std::string_view,const FamilyState&,Integer family_codec_version);
Outcome<BoundaryValue> encode_family_state_versioned(const FamilyState&,Integer family_codec_version);
}
