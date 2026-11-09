"""Check the exact new author scene and closed response contract; no World is created."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
from jsonschema import Draft202012Validator


def main():
    parser = argparse.ArgumentParser()
    for key in ('schema', 'catalog', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    pack = json.loads((root / 'content/packs/authored/diplomatic_responses_1_0/content.json').read_text())
    original = json.loads(args.catalog.read_text())['data']
    schema = json.loads(args.schema.read_text())
    spec = importlib.util.spec_from_file_location('response_links', root / 'tools/content/diplomatic_response_links.py')
    links = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(links)
    scene_schema = {'$ref': '#/$defs/scenario', '$defs': schema['$defs']}
    errors = [error.message for scene in pack['scenarios'] for error in Draft202012Validator(scene_schema).iter_errors(scene)]
    issues = links.check_diplomatic_responses(pack, original)
    cases = []

    def negative(name, mutate, expected):
        bad = copy.deepcopy(pack)
        mutate(bad['scenarios'][0]['dynamic_world'])
        found = links.check_diplomatic_responses(bad, original)
        cases.append({'name': name, 'expected': expected, 'codes': [x[0] for x in found], 'passed': any(x[0] == expected for x in found)})

    key = 'qingchuan_diplomatic_recipient'
    response = lambda d: d['director']['policies'][key]['diplomatic_response']
    negative('missing profile', lambda d: d['director'].pop('diplomatic_response_version'), 'DIPLOMATIC_RESPONSE_BINDING')
    negative('unknown profile', lambda d: d['director'].update(diplomatic_response_version=2), 'DIPLOMATIC_RESPONSE_PROFILE')
    negative('boolean profile', lambda d: d['director'].update(diplomatic_response_version=True), 'DIPLOMATIC_RESPONSE_PROFILE')
    negative('missing diplomacy', lambda d: d.pop('diplomacy'), 'DIPLOMATIC_RESPONSE_PROFILE')
    negative('disabled diplomacy', lambda d: d['diplomacy'].update(enabled=False), 'DIPLOMATIC_RESPONSE_PROFILE')
    negative('missing registry', lambda d: d['director'].pop('registry_version'), 'DIPLOMATIC_RESPONSE_PROFILE')
    negative('missing exact policy', lambda d: d['director']['policies'][key].pop('diplomatic_response'), 'DIPLOMATIC_RESPONSE_BINDING')
    negative('missing template', lambda d: d['director']['policies'][key].update(templates=['maintain_commitment']), 'DIPLOMATIC_RESPONSE_BINDING')
    negative('wrong office', lambda d: response(d).update(office_id='office.city_steward'), 'DIPLOMATIC_RESPONSE_AUTHORITY')
    negative('wrong organization', lambda d: d['director']['policies'][key].update(organization_id='society::org.city'), 'DIPLOMATIC_RESPONSE_AUTHORITY')
    negative('unknown choice', lambda d: response(d)['directives'][0].update(choice='sign_automatically'), 'DIPLOMATIC_RESPONSE_DIRECTIVE')
    negative('wrong terms', lambda d: response(d)['directives'][0].update(terms_id='unknown'), 'DIPLOMATIC_RESPONSE_DIRECTIVE')
    negative('duplicate directive', lambda d: response(d)['directives'].append(copy.deepcopy(response(d)['directives'][0])), 'DIPLOMATIC_RESPONSE_DIRECTIVE')
    negative('invalid reference shape', lambda d: response(d)['directives'][0].update(mission_policy_id=[]), 'DIPLOMATIC_RESPONSE_DIRECTIVE')
    old = {'scenarios': [original['scenarios']['civicecon::scenario.qingchuan_diplomacy']]}
    old_issues = links.check_diplomatic_responses(old, original)
    for name, directives in [('no directive defers', []), ('explicit defer', [{'mission_policy_id': 'mission.qingchuan_sword_yard', 'terms_id': 'passage.qingchuan_sword_yard', 'choice': 'defer'}])]:
        held = copy.deepcopy(pack)
        held['scenarios'][0]['dynamic_world']['director']['policies'][key]['diplomatic_response']['directives'] = directives
        found = links.check_diplomatic_responses(held, original)
        cases.append({'name': name, 'passed': not found, 'codes': [x[0] for x in found]})
    result = {'schema_errors': errors, 'response_link_errors': issues, 'old_profile_errors': old_issues, 'cases': cases, 'formal_compose': False, 'world_created': False}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'schema_errors': errors, 'link_errors': issues, 'old_profile_errors': old_issues, 'cases': len(cases), 'failed': [x for x in cases if not x['passed']]}, ensure_ascii=False))
    return int(bool(errors or issues or old_issues) or not all(x['passed'] for x in cases))


if __name__ == '__main__':
    raise SystemExit(main())
