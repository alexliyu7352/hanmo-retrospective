"""原 field_author_data decoder/field_generation 参数门；不填默认值或创建资源。"""

_POSITIVE = ('window_s', 'step_s', 'length_m', 'width_m', 'cell_m', 'deployment_length_m', 'deployment_width_m', 'deployment_inset_m', 'deployment_min_percent', 'exit_radius_m', 'anchor_shift_step_m', 'courier_m_per_minute', 'minimum_order_delay_s', 'pulse_s', 'round_equiv_s', 'max_segments', 'infantry_m_per_minute', 'archer_m_per_minute', 'cavalry_m_per_minute', 'charge_m_per_minute', 'rout_speed_bp', 'melee_contact_m', 'archer_max_range_m', 'ai_eval_s', 'ai_advance_hold_s', 'rejoin_distance_m', 'rejoin_stopped_s', 'max_formations_per_side')
_NONNEGATIVE = ('anchor_shift_limit_m', 'morale_loss_bp', 'commander_exposure_bp', 'archer_min_range_m', 'ai_retreat_fit_percent', 'ai_retreat_morale', 'ai_support_morale')
_TERRAINS = ('Plain', 'Field', 'Forest', 'Rough', 'Steep', 'Stream', 'Water', 'Road', 'Bridge', 'Ford', 'Village', 'Wall', 'Gate')
_BLOCKED = frozenset(('Gate', 'Steep', 'Wall', 'Water'))
_INT64_MAX = 9223372036854775807

def check_field_battle_parameters(pack, index):
    del index
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or 'field_battle' not in dynamic:
            continue
        path = ('scenarios', number, 'dynamic_world', 'field_battle')
        p = dynamic['field_battle']
        keys = set(_POSITIVE + _NONNEGATIVE) | {'version', 'terrain_speed_bp', 'terrain_cover'}
        if (not isinstance(p, dict) or set(p) != keys
                or p.get('version') != 'field_battle/1'):
            issues.append(('FIELD1_PARAMETER_SHAPE', path, '须原field_battle/1全部显式参数且无未知键'))
            continue
        if any(type(p[k]) is not int or p[k] < (1 if k in _POSITIVE else 0)
               or p[k] > _INT64_MAX for k in _POSITIVE + _NONNEGATIVE):
            issues.append(('FIELD1_PARAMETER_INTEGER', path, '整数正负域须与原native参数门一致；布尔不是整数'))
            continue
        if (p['window_s'] % p['step_s'] or p['pulse_s'] % p['step_s']
                or p['length_m'] % p['cell_m'] or p['width_m'] % p['cell_m']
                or p['length_m'] > _INT64_MAX // 10 or p['width_m'] > _INT64_MAX // 10
                or any(p[k] > 100 for k in ('deployment_min_percent', 'ai_retreat_fit_percent',
                                           'ai_retreat_morale', 'ai_support_morale'))
                or p['archer_min_range_m'] > p['archer_max_range_m']):
            issues.append(('FIELD1_PARAMETER_RELATION', path, '原步长整除、尺寸溢出、百分比及弓箭距离关系不符'))
        speed, cover = p['terrain_speed_bp'], p['terrain_cover']
        if (not isinstance(speed, dict) or not isinstance(cover, dict)
                or set(speed) != set(_TERRAINS) or set(cover) != set(_TERRAINS)):
            issues.append(('FIELD1_TERRAIN_SHAPE', path, '须原13地形两张完整参数表'))
            continue
        for name in _TERRAINS:
            values, c = speed[name], cover[name]
            if (type(c) is not int or not 0 <= c <= 100
                    or not isinstance(values, list) or len(values) != 3
                    or any(type(v) is not int or v > _INT64_MAX
                           or (v != 0 if name in _BLOCKED else v <= 0) for v in values)):
                issues.append(('FIELD1_TERRAIN_VALUE', path + (name,),
                               '原不可通行地形速度须0，其余三兵种速度须正；掩护0..100'))
    return issues
