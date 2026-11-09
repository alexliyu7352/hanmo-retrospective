// 原 exact Travel/4 或 /5 确定派生 local-move/1，不从 Field 或表内容推断。
#pragma once
#include "resident/domains/travel/local_move_codec.hpp"
namespace hanmo::resident {
struct WorldBundle; struct CommonState; struct PreparedCatalog;
inline constexpr const char* local_move_module="local_move_sources";
inline constexpr const char* local_move_derivation_key="local_move_sources_derived_from";
Outcome<Integer> selected_native_local_move_version(const CatalogIdentity&);
Status append_native_local_move_module(const BoundaryValue&,CatalogIdentity&);
Status validate_native_local_move_headers(const CommonState&,const CatalogIdentity&,const MovementExactState*);
Status validate_native_local_move_layout(const WorldBundle&);
Outcome<MovementRecordCodecSelection> native_local_move_codec_selection(const PreparedCatalog&);
} // namespace hanmo::resident
