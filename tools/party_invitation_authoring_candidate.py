"""生成随行邀请Schema、能力与正常NativeRequest/GD接线候选；不改正在使用的正式stage。"""
from __future__ import annotations
import argparse
import copy
import difflib
import hashlib
import json
from pathlib import Path
import subprocess

BASE = 'b4ca92e6cf62105a4ca36d59ecf5159777bfe7c5'


def generate(repo: Path, output: Path) -> dict:
    """邀请块保在原场景实体中，让正常compose/namespace/Catalog路径逐层保留。"""
    if output.resolve() == repo.resolve() or (output / '.git').exists():
        raise ValueError('输出须为隔离候选目录')
    original, changed = {}, {}

    def read(path):
        """只读固定公共基线对象，未知接缝拒绝覆盖。"""
        if path not in changed:
            original[path] = subprocess.check_output(['git', 'show', BASE + ':' + path], cwd=repo).decode()
            changed[path] = original[path]
        return changed[path]

    def replace(path, old, new, count=1):
        """共享GD和作者分派按精确出现次数生成最小差量。"""
        value = read(path)
        if value.count(old) != count:
            raise ValueError(f'{path}: unexpected anchor count {value.count(old)}: {old[:60]}')
        changed[path] = value.replace(old, new)

    schema_path = 'schemas/content.schema.json';schema = json.loads(read(schema_path))
    identity = copy.deepcopy(schema['$defs']['entity_id']);identity['x-hanmo-global-definition'] = 'scenarios[*].party_invitations.invitations'
    props = {'id': identity, 'leader_actor_id': {'$ref': '#/$defs/reference_actors'}, 'invitee_actor_id': {'$ref': '#/$defs/reference_actors'}, 'dialogue_id': {'$ref': '#/$defs/reference_dialogues'}, 'node_id': {'type': 'string', 'pattern': '^[a-z][a-z0-9_.-]{0,79}$'}, 'role_id': {'$ref': '#/$defs/reference_roles'}}
    schema['$defs']['party_invitation'] = {'type': 'object', 'properties': props, 'required': list(props), 'additionalProperties': False}
    scenario = schema['$defs']['scenario']['properties']
    scenario['party_invitation_profile'] = {'enum': ['authored_node_v1']}
    scenario['party_invitations'] = {'type': 'object', 'properties': {'version': {'const': 1, 'type': 'integer'}, 'invitations': {'type': 'array', 'maxItems': 4096, 'items': {'$ref': '#/$defs/party_invitation'}}}, 'required': ['version', 'invitations'], 'additionalProperties': False}
    changed[schema_path] = json.dumps(schema, ensure_ascii=False, indent=2) + '\n'
    # 原生世界、人物、对白、队伍的现能力已覆盖此扩展，不改全局descriptor字节。
    replace('tools/verify.py', 'TESTS = [', "TESTS = ['test_party_participation_panel', ")
    links = 'tools/content/links.py';anchor = '    from .task_offer_response_links import check_task_offer_responses'
    replace(links, anchor, anchor + '\n    from .party_invitation_links import check_party_invitations')
    replace(links, ' + check_task_offer_responses(pack,index)', ' + check_task_offer_responses(pack,index) + check_party_invitations(pack,index)')
    backend = 'game/ui/native_resident_backend.gd';anchor = '\tif definition.get("task_offer_response_profile","")=="known_offers_v1": expected["module_versions"].append({"name":"task_offer_responses","version":"1"})'
    replace(backend, anchor, anchor + '\n\tif definition.get("party_invitation_profile","")=="authored_node_v1": expected["module_versions"].append({"name":"party_invitations","version":"1"})')
    requests = 'game/ui/resident_page_requests.gd';anchor = 'const InteractionTasksPanel=preload("res://ui/interaction_tasks_panel.gd")'
    replace(requests, anchor, anchor + '\nconst PartyParticipationPanel=preload("res://ui/party_participation_panel.gd")')
    # 同一原读屏障在获得当前位置后准备邀请/成员页，不能只给场景按钮预读。
    anchor = '\tif on_road or host.journey_view.opened:'
    replace(requests, anchor, '\tif not PartyParticipationPanel.prepare(host): return false\n' + anchor)
    screen = 'game/ui/screen_composer.gd';anchor = 'const InteractionTasksPanel=preload("res://ui/interaction_tasks_panel.gd")'
    replace(screen, anchor, anchor + '\nconst PartyParticipationPanel=preload("res://ui/party_participation_panel.gd")')
    anchor = '\tif view.get("phase")=="scene" and m._resident_mode and m.kernel.has_method("diplomacy_enabled")'
    replace(screen, anchor, '\tif m._resident_mode and PartyParticipationPanel.visible_here(m): PartyParticipationPanel.build(m,_sidebar_section(m,stage,"PartyParticipationArea","同行"))\n' + anchor)
    anchor = '\t\tm.WorldMapScreen.render(m,stage,m.kernel.journey_sidebar())'
    replace(screen, anchor, anchor + '\n\t\tif m._resident_mode and PartyParticipationPanel.visible_here(m): PartyParticipationPanel.build(m,_sidebar_section(m,stage,"PartyParticipationArea","同行"))')
    own = Path(__file__).resolve().parents[1];owned = ['tools/content/party_invitation_links.py', 'game/ui/party_participation_panel.gd']
    output.mkdir(parents=True, exist_ok=True);entries = []
    for path, value in changed.items():
        target = output / path;target.parent.mkdir(parents=True, exist_ok=True);target.write_text(value)
        entries.append({'path': path, 'base_git_oid': hashlib.sha1(b'blob ' + str(len(original[path].encode())).encode() + b'\0' + original[path].encode()).hexdigest(), 'sha256': hashlib.sha256(value.encode()).hexdigest()})
    for path in owned:
        target = output / path;target.parent.mkdir(parents=True, exist_ok=True);target.write_bytes((own / path).read_bytes())
    patch = ''.join(''.join(difflib.unified_diff(original[path].splitlines(True), value.splitlines(True), 'a/' + path, 'b/' + path)) for path, value in changed.items())
    (output / 'authoring.patch').write_text(patch);result = {'base_commit': BASE, 'activation': 'CANDIDATE_ONLY', 'shared_files': entries, 'owned_sources': owned, 'normal_loader_tested': False, 'patch_sha256': hashlib.sha256(patch.encode()).hexdigest()};(output / 'authoring-manifest.json').write_text(json.dumps(result, indent=2) + '\n');return result


def main() -> int:
    """仅输出可审阅候选，不在旧生产目录安装新内容或扩大能力摘要。"""
    parser = argparse.ArgumentParser(description=__doc__);parser.add_argument('--repo', type=Path, required=True);parser.add_argument('--output', type=Path, required=True);args = parser.parse_args();result = generate(args.repo.resolve(), args.output.resolve());print(json.dumps({'base': BASE, 'shared_paths': len(result['shared_files'])}));return 0


if __name__ == '__main__':
    raise SystemExit(main())
