"""原群体成员释放/损失物理段：只显式版本配对，不造成员、伤势、死亡或运输数据。"""


def _selected(value, versions):
    return (isinstance(value, dict) and set(value) == {'version'}
            and type(value.get('version')) is int and value['version'] in versions)


def check_movement_group_member_handoffs(pack, index):
    del index  # 两个版本块没有ID；不猜namespace或加入任何来源引用。
    issues = []
    for number, scene in enumerate(pack.get('scenarios', [])):
        dynamic = scene.get('dynamic_world')
        if not isinstance(dynamic, dict):
            continue
        # 原native_transport_catalog版本/配对；这里不读取或改写运输内容。
        transport_versions = {'travel': (4, 5), 'group_transport': (4, 5, 6),
                              'transport_manifest_bindings': (2,), 'carrier_location': (1,),
                              'private_transport_funding': (1,)}
        if any(key in dynamic for key in transport_versions):
            transport_path = ('scenarios', number, 'dynamic_world')
            for key, allowed in transport_versions.items():
                if key in dynamic and not _selected(dynamic[key], allowed):
                    issues.append(('ORIGINAL_TRANSPORT_SELECTION_SHAPE', transport_path + (key,),
                                   '原运输块仅含显式受支持整数version，无未知参数'))
            clock = scene.get('world_time')
            if not isinstance(clock, dict) or clock.get('profile') != 'rational_world_complete_v2':
                issues.append(('ORIGINAL_TRANSPORT_COMPLETE_REQUIRED', transport_path,
                               '原运输物理选择须显式Complete，不从字段存在生成钟'))
            if (('group_transport' in dynamic and 'transport_manifest_bindings' not in dynamic)
                    or (('group_transport' in dynamic or 'transport_manifest_bindings' in dynamic)
                        and ('travel' not in dynamic or 'carrier_location' not in dynamic))
                    or ('carrier_location' in dynamic and 'travel' not in dynamic)):
                issues.append(('ORIGINAL_TRANSPORT_SOURCE_PAIR', transport_path,
                               '原group须manifest2；group/manifest须travel及carrier1；carrier须travel'))
            travel5 = _selected(dynamic.get('travel'), (5,))
            funding1 = _selected(dynamic.get('private_transport_funding'), (1,))
            if travel5 != funding1:
                issues.append(('ORIGINAL_TRANSPORT_FUNDING_PAIR', transport_path,
                               '原travel5与private_transport_funding1必须双向明确配对'))
        release = 'movement_group_member_releases' in dynamic
        loss = 'movement_group_member_losses' in dynamic
        if not release and not loss:
            continue
        path = ('scenarios', number, 'dynamic_world')
        for key in ('movement_group_member_releases', 'movement_group_member_losses'):
            if key in dynamic and not _selected(dynamic[key], (1,)):
                issues.append(('MOVEMENT_MEMBER_SOURCE1_SHAPE', path + (key,),
                               '原物理段只准显式整数version:1，不接受bool/未知字段或隐式推断'))
        if release:
            clock, field = scene.get('world_time'), dynamic.get('field_battle')
            if (not _selected(dynamic.get('group_transport'), (4, 5, 6))
                    or not isinstance(clock, dict)
                    or clock.get('profile') != 'rational_world_complete_v2'
                    or not isinstance(field, dict) or field.get('version') != 'field_battle/1'):
                issues.append(('MOVEMENT_MEMBER_RELEASE1_ORIGINAL_PAIR', path + ('movement_group_member_releases',),
                               '释放1须明选原group_transport4/5/6、Field1和Complete，不能自动启用'))
        if loss:
            if (not _selected(dynamic.get('movement_group_member_releases'), (1,))
                    or scene.get('family_life') != {'profile': 'complete_life_causes_v4',
                                                   'player_death': 'await_explicit_continuation'}
                    or not _selected(scene.get('character_injury_sources'), (1,))):
                issues.append(('MOVEMENT_MEMBER_LOSS1_ORIGINAL_PAIR', path + ('movement_group_member_losses',),
                               '损失1须原释放1、family_life完整死因4实际作者profile和character_injury_sources1'))
    return issues
