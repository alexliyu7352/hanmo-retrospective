// 仅适配原字段表的输出/读取方向，不新增任何持久字段或业务分支。
#pragma once
#include "politics_projection.hpp"
#include "resident/domains/local_economy/local_economy_codec_detail.hpp"
namespace hanmo::resident::politics_projection_detail {
template<class Archive> bool reading(Archive& archive){
 if constexpr(requires{archive.codec.reading();})return archive.codec.reading();
 else if constexpr(requires{archive.reading();})return archive.reading();
 else return std::is_same_v<std::remove_cvref_t<Archive>,dynamic_economy_codec::Decoder>;
}
template<class T,class Emit> BoundaryValue encode_fields(const T& value,Emit emit){
 BoundaryValue out;BoundaryProjectionWriter writer(out);
 auto result=writer.object("",[&](auto& fields){return emit(fields,value);});
 if(result.ok)result=writer.finish();if(!result.ok)throw boundary::DecodeFailure{result.error};return out;
}
}
