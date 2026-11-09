// 细地形真实拥有字段；仅供原 copy/retention 装配，不改数据或另计余额。
// 由 source_owned_fields 命名空间包含；类型须由原依赖登记先行可见。
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleParameters &value,
                  Visitor &&visitor) {
  visitor(value.version);
  visitor(value.window_s);
  visitor(value.step_s);
  visitor(value.length_m);
  visitor(value.width_m);
  visitor(value.cell_m);
  visitor(value.deployment_length_m);
  visitor(value.deployment_width_m);
  visitor(value.deployment_inset_m);
  visitor(value.deployment_min_percent);
  visitor(value.exit_radius_m);
  visitor(value.anchor_shift_step_m);
  visitor(value.anchor_shift_limit_m);
  visitor(value.courier_m_per_minute);
  visitor(value.minimum_order_delay_s);
  visitor(value.pulse_s);
  visitor(value.round_equiv_s);
  visitor(value.morale_loss_bp);
  visitor(value.commander_exposure_bp);
  visitor(value.max_segments);
  visitor(value.infantry_m_per_minute);
  visitor(value.archer_m_per_minute);
  visitor(value.cavalry_m_per_minute);
  visitor(value.charge_m_per_minute);
  visitor(value.rout_speed_bp);
  visitor(value.terrain_speed_bp);
  visitor(value.terrain_cover);
  visitor(value.melee_contact_m);
  visitor(value.archer_min_range_m);
  visitor(value.archer_max_range_m);
  visitor(value.ai_retreat_fit_percent);
  visitor(value.ai_retreat_morale);
  visitor(value.ai_support_morale);
  visitor(value.ai_eval_s);
  visitor(value.ai_advance_hold_s);
  visitor(value.rejoin_distance_m);
  visitor(value.rejoin_stopped_s);
  visitor(value.max_formations_per_side);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldScale &value,
                  Visitor &&visitor) {
  visitor(value.num);
  visitor(value.den);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldTerrainMetadata &value,
                  Visitor &&visitor) {
  visitor(value.map_id);
  visitor(value.height_sha256);
  visitor(value.rivers_sha256);
  visitor(value.land_sha256);
  visitor(value.km_per_unit);
  visitor(value.m_per_px);
  visitor(value.width_px);
  visitor(value.height_px);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldTerrainData &value,
                  Visitor &&visitor) {
  visitor(value.metadata);
  visitor(value.height);
  visitor(value.rivers);
  visitor(value.land);
}
template <class Visitor>
inline void visit(const ::hanmo::resident::FieldBattleCatalog &value,
                  Visitor &&visitor) {
  visitor(value.identity);
  visitor(value.parameters);
  visitor(value.maps);
  visitor(value.settlement_kinds);
  visitor(value.office_titles);
}
