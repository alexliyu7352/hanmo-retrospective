"""用正式Installed目录中的人物/本领核邀学作者叶，尚不代表完整包或World授徒。"""
import argparse
from copy import deepcopy
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('teaching_requests', ROOT / 'tools/content/teaching_requests.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def main():
    """正常输入与相邻误绑定均只读检查，不改变正式作者文件。"""
    parser = argparse.ArgumentParser()
    parser.add_argument('--catalog', type=Path, required=True)
    args = parser.parse_args()
    raw = args.catalog.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == '8f02ed2028bce4cb2659e04d300b5440eb02bcf9a8bdb106923b27f7abe9c62e'
    index = json.loads(raw)['data']
    scenario = deepcopy(index['scenarios']['actorlife::scenario.qingchuan_procurement_life'])
    skill = 'society::skill.settle_breath'
    teacher, student = 'society::actor.sword_master', 'society::actor.apprentice'
    role, dialogue, request = 'teaching::role.learner', 'teaching::dialog.request', 'teaching::request.breath'
    scenario['teaching_requests'] = {'profile': MODULE.PROFILE, 'request_ids': [request]}
    scenario['enabled']['roles'].append(role)
    scenario['enabled']['dialogues'].append(dialogue)
    definition = {'id': request, 'teacher_actor_id': teacher, 'student_actor_id': student, 'skill_id': skill, 'dialogue_id': dialogue, 'node_id': 'accept'}
    pack = {'teaching_requests': [definition], 'roles': [{'id': role, 'initial_actor': student, 'place_id': 'society::place.courtyard'}], 'dialogues': [{'id': dialogue, 'context_role': role, 'nodes': [{'id': 'accept', 'kind': 'action', 'command': {'kind': 'teach_skill', 'skill_id': skill}}]}], 'scenarios': [scenario]}
    checks = 0

    def check(condition, message):
        nonlocal checks
        checks += 1
        assert condition, message

    def errors(value, source=index):
        return [row[0] for row in MODULE.check_teaching_requests(value, source)]

    check(not errors(pack), 'explicit request binds real teacher/student and existing ability')
    bad = deepcopy(pack); bad['teaching_requests'][0]['student_actor_id'] = teacher
    check('TEACHING_REQUEST_PARTIES' in errors(bad), 'self instruction cannot masquerade as invitation')
    bad = deepcopy(pack); bad['teaching_requests'][0]['accept_automatically'] = True
    check('TEACHING_REQUEST_FIELDS' in errors(bad), 'no automatic consent author switch')
    bad = deepcopy(pack); bad['scenarios'][0]['teaching_requests']['profile'] = 'unspecified'
    check('TEACHING_REQUEST_PROFILE' in errors(bad), 'unknown policy refused')
    bad = deepcopy(pack); bad['scenarios'][0]['teaching_requests']['request_ids'] *= 2
    check('TEACHING_REQUEST_IDS' in errors(bad), 'duplicate selection refused')
    bad = deepcopy(pack); bad['scenarios'][0]['enabled']['actors'].remove(student)
    check('TEACHING_REQUEST_ACTORS' in errors(bad), 'selected student must exist in active scenario')
    bad = deepcopy(pack); bad['scenarios'][0]['enabled']['roles'].remove(role)
    check('TEACHING_REQUEST_ROLE' in errors(bad), 'disabled conversation role cannot offer lesson')
    bad = deepcopy(pack); bad['scenarios'][0]['enabled']['dialogues'].remove(dialogue)
    check('TEACHING_REQUEST_DIALOGUE_DISABLED' in errors(bad), 'selected invitation dialogue must be enabled in the actual scenario')
    bad = deepcopy(pack); bad['dialogues'][0]['nodes'][0]['command']['skill_id'] = 'society::skill.measure_cut'
    check('TEACHING_REQUEST_ACTION' in errors(bad), 'action must teach the requested skill')
    bad = deepcopy(pack); bad['dialogues'][0]['nodes'][0]['kind'] = 'end'
    check('TEACHING_REQUEST_NODE' in errors(bad), 'closed dialogue is not pending acceptance')
    bad = deepcopy(pack); bad['teaching_requests'].append(dict(definition, id=request + '.duplicate')); bad['scenarios'][0]['teaching_requests']['request_ids'].append(request + '.duplicate')
    check('TEACHING_REQUEST_BINDING' in errors(bad), 'two IDs cannot claim one current node binding')
    passive = deepcopy(index); passive['teaching_requests'] = {'old.unselected': False}
    check(not errors(pack, passive), 'unused old passive source does not become active')
    legacy = deepcopy(pack); legacy['scenarios'][0].pop('teaching_requests')
    check(not errors(legacy), 'legacy scenario is not upgraded by passive definition')
    check(raw == args.catalog.read_bytes() and index == json.loads(raw)['data'], 'actual actor skills, locations, goods and roles unchanged')
    print(f'TEACHING_REQUEST_AUTHOR {checks} checks 0 failures; AUTHOR_LEAF_ONLY')


if __name__ == '__main__':
    main()
