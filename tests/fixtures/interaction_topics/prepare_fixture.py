"""从真实two_stories作者源派生多岗位夹具，使用正常prepare及受信Schema staging，不复制美术。"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys


def main():
    """准备旧入口、无对白首岗位和双合法入口；反例必须经正式prepare拒绝且不写产物。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--schema-stage', type=Path, required=True)
    args = parser.parse_args()
    root = args.project_root.resolve()
    sys.path.insert(0, str(root))
    from tools.content.io import prepare_pack
    from tools.content_stage import write_schema_index
    source = root / 'content/foundation/two_stories.json'
    pack = json.loads(source.read_text())
    extra = copy.deepcopy(next(role for role in pack['roles'] if role['id'] == 'role.merchant'))
    extra.update(id='role.a_optional', name_key='name.role.a_optional', services=['information'])
    pack['roles'].insert(0, extra)
    pack['texts']['name.role.a_optional'] = '商会问事'
    args.output.mkdir(parents=True, exist_ok=True)
    authored = args.output / 'two-roles.author.json'
    authored.write_text(json.dumps(pack, ensure_ascii=False, indent=2) + '\n')
    legacy = args.output / 'legacy.hmcatalog.json'
    multiple = args.output / 'two-roles.hmcatalog.json'
    reports = [prepare_pack(source, legacy), prepare_pack(authored, multiple)]
    explicit = copy.deepcopy(pack)
    explicit['roles'][0]['dialogue_id'] = 'dialog.optional'
    explicit['dialogues'].append({'id': 'dialog.optional', 'context_role': 'role.a_optional',
                                 'entry': 'hello', 'nodes': [
                                     {'id': 'hello', 'kind': 'say', 'speaker': '$provider',
                                      'text_key': 'topic.optional.hello', 'next': 'end'},
                                     {'id': 'end', 'kind': 'end'}]})
    explicit['texts']['topic.optional.hello'] = '商会问事，请讲。'
    explicit_source = args.output / 'explicit-roles.author.json'
    explicit_source.write_text(json.dumps(explicit, ensure_ascii=False, indent=2) + '\n')
    explicit_output = args.output / 'explicit-roles.hmcatalog.json'
    reports.append(prepare_pack(explicit_source, explicit_output))
    event_pack = copy.deepcopy(explicit)
    event_pack['events'].append({'id': 'event.optional', 'place_id': 'west.grain',
                                 'trigger': 'on_enter', 'conditions': [],
                                 'dialogue_id': 'dialog.optional', 'cooldown_ticks': 2,
                                 'once_per_actor': True, 'on_unavailable': 'cancel',
                                 'name_key': 'name.event.optional'})
    event_pack['texts']['name.event.optional'] = '商会来客'
    event_source = args.output / 'role-event.author.json'
    event_source.write_text(json.dumps(event_pack, ensure_ascii=False, indent=2) + '\n')
    event_output = args.output / 'role-event.hmcatalog.json'
    reports.append(prepare_pack(event_source, event_output))
    rejected = []
    for label, dialogue_id, expected_code in [
            ('wrong-context', 'dialog.shop', 'ROLE_DIALOGUE_CONTEXT'),
            ('missing-reference', 'dialog.missing', 'REFERENCE')]:
        invalid = copy.deepcopy(explicit)
        invalid['roles'][0]['dialogue_id'] = dialogue_id
        invalid_source = args.output / (label + '.author.json')
        invalid_source.write_text(json.dumps(invalid, ensure_ascii=False, indent=2) + '\n')
        invalid_output = args.output / (label + '.hmcatalog.json')
        try:
            prepare_pack(invalid_source, invalid_output)
        except ValueError as failure:
            assert str(failure).startswith('Invalid content: '), str(failure)
            report = json.loads(str(failure).removeprefix('Invalid content: '))
            assert expected_code in [issue['code'] for issue in report['issues']], report
            assert not invalid_output.exists(), 'Rejected fixture published a catalog'
            rejected.append({'case': label, 'expected_code': expected_code, 'report': report})
        else:
            raise AssertionError('Invalid role dialogue accepted: ' + label)
    args.schema_stage.mkdir(parents=True, exist_ok=True)
    schema = (root / 'schemas/content.schema.json').read_bytes()
    (args.schema_stage / 'content.schema.json').write_bytes(schema)
    write_schema_index(root, args.schema_stage)
    receipt = {'legacy': str(legacy.resolve()), 'multiple': str(multiple.resolve()),
               'explicit': str(explicit_output.resolve()), 'rejected': rejected,
               'role_event': str(event_output.resolve()),
               'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
               'schema_sha256': hashlib.sha256(schema).hexdigest(), 'prepared': reports}
    (args.output / 'prepare-receipt.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(receipt, ensure_ascii=False))


if __name__ == '__main__':
    main()
