"""从正常治理Catalog与具名外交增量核schema/领域引用，尚不冒正式compose。"""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    for name in ('catalog', 'source_root', 'output'):
        parser.add_argument('--' + name.replace('_', '-'), type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    spec = importlib.util.spec_from_file_location('content.diplomacy_links', root / 'tools/content/diplomacy_links.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    from jsonschema import Draft202012Validator
    data = copy.deepcopy(json.loads(args.catalog.read_text())['data'])
    scenario = copy.deepcopy(data['scenarios']['civicecon::scenario.qingchuan_governance'])
    scenario['id'] = 'civicecon::scenario.qingchuan_diplomacy'
    candidate = root / 'content/foundation/diplomacy-candidate'
    scenario['dynamic_world']['diplomacy'] = json.loads((candidate / 'qingchuan-diplomacy.block.json').read_text())
    organizations = scenario['dynamic_world']['organization']
    for key, grant in json.loads((candidate / 'qingchuan-diplomacy.grants.json').read_text()).items():
        organizations['grants'][key] = grant
        organizations['offices'][grant['office_id']]['grant_ids'].append(key)
    data['texts'].update(json.loads((candidate / 'qingchuan-diplomacy.texts.json').read_text()))
    schema_bytes = subprocess.check_output(['git', 'show', '50e069efaa0660e324bc84b8eaf3ceab8f28f5bc:schemas/content.schema.json'], cwd=args.source_root)
    schema = json.loads(schema_bytes)
    selected = lambda source: Draft202012Validator({'$ref': '#/$defs/scenario', '$defs': source['$defs']})
    red = [error.message for error in selected(schema).iter_errors(scenario)]
    fragment = json.loads((root / 'schemas/fragments/diplomacy-v1.json').read_text())
    schema['$defs'].update(fragment['$defs'])
    schema['$defs']['dynamic_world']['properties']['diplomacy'] = fragment['property_schema']
    green = [error.message for error in selected(schema).iter_errors(scenario)]
    pack = {'required_capabilities': data['required_capabilities'], 'scenarios': [scenario]}
    issues = module.check_diplomacy(pack, data)
    cases = []

    def negative(label, mutate, expected):
        bad = copy.deepcopy(pack)
        mutate(bad['scenarios'][0]['dynamic_world'])
        found = module.check_diplomacy(bad, data)
        cases.append({'name': label, 'expected': expected, 'codes': [x[0] for x in found], 'passed': any(x[0] == expected for x in found)})

    policy = 'mission.qingchuan_sword_yard'
    terms = 'passage.qingchuan_sword_yard'
    negative('disabled explicit block', lambda d: d['diplomacy'].update(enabled=False), 'DIPLOMACY_PROFILE')
    negative('unknown version', lambda d: d['diplomacy'].update(version=2), 'DIPLOMACY_PROFILE')
    negative('boolean version', lambda d: d['diplomacy'].update(version=True), 'DIPLOMACY_PROFILE')
    negative('second task switch', lambda d: d.update(envoy_tasks={'enabled': True}), 'DIPLOMACY_DERIVED_TASK_PROFILE')
    negative('wrong recipient office', lambda d: d['diplomacy']['mission_policies'][policy].update(target_office_id='office.city_steward'), 'DIPLOMACY_POLICY_AUTHORITY')
    negative('unknown place', lambda d: d['diplomacy']['passage_terms'][terms].update(place_ids=['missing']), 'DIPLOMACY_TERMS_SOURCE')
    negative('repeated place', lambda d: d['diplomacy']['passage_terms'][terms].update(place_ids=['society::place.sword_gate'] * 2), 'DIPLOMACY_TERMS_SOURCE')
    negative('same parties', lambda d: d['diplomacy']['passage_terms'][terms].update(grantor_organization_id='society::org.city'), 'DIPLOMACY_PARTIES')
    negative('notice after expiry', lambda d: d['diplomacy']['passage_terms'][terms].update(expiry_notice_ticks=12), 'DIPLOMACY_TERMS_TIME')
    negative('no offer window', lambda d: d['diplomacy']['mission_policies'][policy].update(offer_valid_ticks=0), 'DIPLOMACY_POLICY_TIME')
    negative('unimplemented return', lambda d: d['diplomacy']['mission_policies'][policy]['task_terms'].update(return_report_required=True, report_work_ticks=1), 'DIPLOMACY_RETURN_UNSUPPORTED')
    negative('work crosses original expiry', lambda d: d['diplomacy']['mission_policies'][policy]['task_terms'].update(delivery_work_ticks=8), 'DIPLOMACY_TASK_TERMS')
    negative('missing issuer grant', lambda d: d['organization']['grants']['grant.diplomacy.city'].update(allowed_command_kinds=[]), 'DIPLOMACY_ISSUER_GRANT')
    negative('missing reply grant', lambda d: d['organization']['grants']['grant.diplomacy.sect'].update(allowed_command_kinds=['diplomatic_receive_passage']), 'DIPLOMACY_RECIPIENT_GRANT')
    negative('wrong actual receiving site', lambda d: d['organization']['grants']['grant.diplomacy.sect'].update(scopes=[{'kind': 'site', 'id': 'society::place.courtyard'}]), 'DIPLOMACY_RECIPIENT_GRANT')
    result = {'catalog_sha256': hashlib.sha256(args.catalog.read_bytes()).hexdigest(), 'base_schema_sha256': hashlib.sha256(schema_bytes).hexdigest(), 'old_schema_errors': red, 'new_schema_errors': green, 'diplomacy_link_errors': issues, 'negative_cases': cases, 'formal_compose': False, 'runtime_verified': False}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'old_schema_errors': red, 'new_schema_errors': green, 'diplomacy_link_errors': issues, 'negative_checks': len(cases), 'failed_cases': [x for x in cases if not x['passed']]}))
    return int(not red or bool(green) or bool(issues) or not all(x['passed'] for x in cases))


if __name__ == '__main__':
    raise SystemExit(main())
