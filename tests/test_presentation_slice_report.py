"""表现纵切片回执必须区分结构测试、实际输入、视觉和跨进程恢复。"""
from copy import deepcopy
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def module():
    """按脚本路径加载独立入口，避免测试包名称影响执行。"""
    path = ROOT / 'tools/verify_presentation_slice.py'
    assert path.exists(), '缺少严格纵切片验证入口'
    spec = importlib.util.spec_from_file_location('slice_report', path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def test_slice_report_rejects_missing_actual_input_or_resume(tmp_path):
    """若删去输入或重启证据，任何自报PASS均必须被拒绝。"""
    verify = module()
    report = verify.empty_report(ROOT)
    report['status'] = 'PASS'
    for name in verify.GATES:
        report[name]['status'] = 'PASS'
    errors = verify.validate_report(report, tmp_path)
    assert any('actual_input' in error for error in errors)
    assert any('save_resume' in error for error in errors)
    assert any('visual' in error for error in errors)


def test_structural_assets_cannot_claim_visual_pass(tmp_path):
    """几何child部件即便声明child比例也不等于真实儿童素材。"""
    verify = module()
    report = verify.empty_report(ROOT)
    report['visual'].update(status='PASS', asset_scope='structural_test_only', children_verified=True)
    assert any('visual' in error for error in verify.validate_report(report, tmp_path))


def test_empty_report_honestly_records_blockers(tmp_path):
    """默认报告保留未跑/阻碍，不把无截图无输入当成绿灯。"""
    verify = module()
    report = verify.empty_report(ROOT)
    assert report['status'] == 'BLOCKED'
    assert report['scope'] == verify.LINUX_SCOPE
    assert report['actual_input']['status'] == 'BLOCKED'
    assert report['visual']['status'] == 'BLOCKED'
    assert report['web']['status'] == 'DEFERRED'
    assert verify.validate_report(report, tmp_path) == []


def test_artifact_hash_and_path_are_verified(tmp_path):
    """篡改字节、缺失文件或逃逸路径不能保留旧证据身份。"""
    verify = module()
    proof = tmp_path / 'proof.json'
    proof.write_text('{}')
    reference = verify.artifact(proof, tmp_path)
    assert verify.checked_artifact(reference, tmp_path) == proof
    proof.write_text('{"tampered":true}')
    for bad in (reference, {'path': '../outside', 'sha256': '0' * 64}):
        try:
            verify.checked_artifact(bad, tmp_path)
        except ValueError:
            pass
        else:
            raise AssertionError('坏证据未被拒绝')


def test_author_variants_change_only_declared_appearance_and_layout():
    """同源两配置不得借表现样板改任务、属性、资金、地理或战斗规则。"""
    verify = module()
    first = json.loads((ROOT / 'content/foundation/alpha_life.json').read_text())
    second = deepcopy(first)
    second['actors'][0]['appearance_definition'] = {'kind': 'fixed'}
    second['places'][0]['scene_template_ids'] = {'city': 'other'}
    assert verify.gameplay_content(first) == verify.gameplay_content(second)
    attribute = next(iter(second['actors'][0]['attributes']))
    second['actors'][0]['attributes'][attribute] += 1
    assert verify.gameplay_content(first) != verify.gameplay_content(second)


def test_source_digest_cannot_be_relabelled(tmp_path):
    """提交号相同也不能掩盖另一组源码字节。"""
    verify = module()
    report = verify.empty_report(ROOT)
    report['source']['sha256'] = '0' * 64
    assert any('source' in error for error in verify.validate_report(report, tmp_path))


def test_native_input_rejects_signal_only_transcript(tmp_path):
    """完整步骤名称仍不能把按钮emit调用伪装成原生输入。"""
    verify = module()
    report = verify.empty_report(ROOT)
    report['structure']['manifest'] = {'sha256': 'a' * 64}
    evidence = {'backend': 'native_os', 'source_sha256': report['source']['sha256'],
        'manifest_sha256': 'a' * 64,
        'steps': [{'id': name, 'event': 'signal'} for name in report['fixture']['required_input_steps']]}
    path = tmp_path / 'inputs.json'; path.write_text(json.dumps(evidence))
    report['actual_input'] = {'status': 'PASS', 'evidence': verify.artifact(path, tmp_path)}
    assert any('signals/callbacks' in error for error in verify.validate_report(report, tmp_path))


def test_resume_rejects_recreation_in_same_process(tmp_path):
    """重新创建核心或主场景不能替代退出进程后的独立恢复。"""
    verify = module()
    report = verify.empty_report(ROOT)
    report['engine']['actual'] = verify.ENGINE
    state = {'actors': {}, 'rng': {}, 'tick': 2, 'content_digest': 'catalog-digest'}
    for name, value in {
        'catalog': {'catalog_sha256': 'catalog-digest'},
        'checkpoint': {'writer_pid': 42, 'state': state, 'profiles': {}},
        'resume': {'writer_pid': 42, 'reader_pid': 42, 'state': state, 'checks': 1, 'failures': []}
    }.items():
        (tmp_path / (name + '.json')).write_text(json.dumps(value))
    ref = lambda name: verify.artifact(tmp_path / (name + '.json'), tmp_path)
    report['structure']['variants'] = [{'id': name, 'catalog': ref('catalog')} for name in ('a', 'b')]
    report['save_resume'] = {'status': 'PASS', 'evidence': [
        {'id': name, 'proof': ref('resume'), 'checkpoint': ref('checkpoint')} for name in ('a', 'b')]}
    assert any('cross-process' in error for error in verify.validate_report(report, tmp_path))


def test_real_prepare_saves_reopens_two_author_configurations(tmp_path):
    """真实作者管线生成两份可复核目录；源码清单保持不变，运行时无需补配置。"""
    path = ROOT / 'tests/fixtures/presentation/prepare_vertical_slice.py'
    spec = importlib.util.spec_from_file_location('slice_prepare', path)
    helper = importlib.util.module_from_spec(spec); spec.loader.exec_module(helper)
    before = (ROOT / 'game/data/presentation.json').read_bytes()
    project = tmp_path / 'project'
    prepared = helper.prepare(project)
    assert (ROOT / 'game/data/presentation.json').read_bytes() == before
    assert [item['id'] for item in prepared['variants']] == ['a', 'b']
    authors = [json.loads(Path(item['author_path']).read_text()) for item in prepared['variants']]
    verify = module()
    assert verify.gameplay_content(authors[0]) == verify.gameplay_content(authors[1])
    assert authors[0] != authors[1]
    assert (project / 'game/data/generated/content.schema.json').read_bytes() == (ROOT / 'schemas/content.schema.json').read_bytes()
    for item in prepared['variants']:
        assert item['saved'] == 'AUTHOR_SAVED' and item['reopened_equal'] is True
        catalog = json.loads(Path(item['catalog_path']).read_text())
        assert catalog['source_sha256'] == item['author_sha256']
        assert catalog['catalog']['world_maps']['map.qingchuan']['scene_template_id'] == f'slice.{item["id"]}.world'


def test_duplicate_variant_cannot_stand_in_for_second_configuration(tmp_path):
    """同一配置重复两遍不能构成双配置规则/恢复证据。"""
    verify = module()
    report = verify.empty_report(ROOT)
    report['engine']['actual'] = verify.ENGINE
    for gate in ('rules', 'save_resume'):
        report[gate] = {'status': 'PASS', 'evidence': [{'id': 'a'}, {'id': 'a'}]}
    errors = verify.validate_report(report, tmp_path)
    assert sum('both distinct variants' in error for error in errors) == 2


def test_cross_configuration_comparison_keeps_every_gameplay_field():
    """外貌排除只限显式档案与目录摘要，不能抹去资金、随机流或行动计数。"""
    verify = module()
    state = {'content_digest': 'a', 'actors': {'actor.player': {'appearance': {'face': 'a'}, 'hp': 100}},
             'rng': {'world': 5}, 'next_instance_id': 10, 'wallets': {'player': 30}}
    other = deepcopy(state)
    other['content_digest'] = 'b'; other['actors']['actor.player']['appearance']['face'] = 'b'
    assert verify.gameplay_world(state) == verify.gameplay_world(other)
    other['next_instance_id'] += 1
    assert verify.gameplay_world(state) != verify.gameplay_world(other)


def synthetic_complete_report(tmp_path, image_format='JPEG'):
    """仅为验证器造形状完整的单测数据，不是原生输入、视觉或游戏通过证据。"""
    from PIL import Image
    verify = module()
    report = verify.empty_report(ROOT)
    report.update(scope='linux_native', status='PASS')
    report['engine']['actual'] = verify.ENGINE

    def put(name, value):
        """写入单测私有目录并绑定字节，避免修改任何历史游戏证据。"""
        path = tmp_path / (name + '.json')
        path.write_text(json.dumps(value))
        return verify.artifact(path, tmp_path)

    image_path = tmp_path / ('synthetic.' + image_format.lower())
    Image.new('RGB', (32, 24), 'white').save(image_path, image_format)
    screenshot = verify.artifact(image_path, tmp_path)
    manifest = put('manifest', {'components': {'body': {}}, 'character_profiles': {'person': {}},
        'scene_templates': {'room': {}}, 'backgrounds': {'room': {'path': 'res://art/room.png'}},
        'portraits': {'person': {'path': 'res://art/person.png'}}})
    state = {'actors': {'actor.player': {'appearance': {'face': 'unit-test'}}},
             'rng': {'world': 123}, 'tick': 2, 'content_digest': 'catalog-digest'}
    state_ref = put('state', state)
    profiles = {'actor.player': state['actors']['actor.player']['appearance']}
    author = put('author', {'actors': [], 'places': [], 'world_maps': []})
    catalog = put('catalog', {'source_sha256': author['sha256'], 'catalog_sha256': 'catalog-digest'})
    preparation = put('preparation', {'variants': [
        {'id': name, 'saved': 'AUTHOR_SAVED', 'reopened_equal': True, 'prepared': 'AUTHORING_ONLY'}
        for name in ('a', 'b')]})
    report['structure'] = {'status': 'PASS', 'evidence': preparation, 'manifest': manifest,
        'same_gameplay_content': True, 'variants': [
            {'id': name, 'author': author, 'catalog': catalog, 'catalog_sha256': catalog['sha256']}
            for name in ('a', 'b')]}
    rules = put('rules', {'checks': 1, 'failures': [], 'commands': [{'kind': 'wait'}], 'scenes': ['unit-test'],
        'profiles': profiles, 'rendered_state': state, 'headless_state': state})
    report['rules'] = {'status': 'PASS', 'evidence': [{'id': name, 'proof': rules} for name in ('a', 'b')]}
    checkpoint = put('checkpoint', {'writer_pid': 42, 'state': state, 'profiles': profiles})
    resume = put('resume', {'writer_pid': 42, 'reader_pid': 43, 'state': state, 'checks': 1, 'failures': []})
    report['save_resume'] = {'status': 'PASS', 'evidence': [
        {'id': name, 'proof': resume, 'checkpoint': checkpoint} for name in ('a', 'b')]}
    common = {'source_sha256': report['source']['sha256'], 'manifest_sha256': manifest['sha256']}
    native = put('native', dict(common, backend='native_os', steps=[
        {'id': name, 'event': 'mouse', 'variant': 'a', 'screenshot': screenshot,
         'state': state_ref, 'headless_state': state_ref} for name in report['fixture']['required_input_steps']]))
    report['actual_input'] = {'status': 'PASS', 'evidence': native}
    visual = put('visual', dict(common, reviewer='unit-test-only', cases={
        name: {'result': 'PASS', 'notes': 'synthetic validator test, not a visual review'}
        for name in report['fixture']['required_visual_cases']}, screenshots=[
            {'image': screenshot, 'window': row} for row in report['fixture']['window_matrix']]))
    report['visual'] = {'status': 'PASS', 'asset_scope': 'production_art', 'evidence': visual}
    return verify, report, image_path


def test_linux_scope_can_validate_five_gates_with_web_deferred(tmp_path):
    """明确Linux范围的五门齐证可以通过；延期Web不再制造逻辑死路。"""
    verify, report, _ = synthetic_complete_report(tmp_path)
    assert verify.validate_report(report, tmp_path) == []
    report['web'] = {'status': 'NOT_RUN', 'reason': '用户按Linux-first计划将Web后置'}
    assert verify.validate_report(report, tmp_path) == []


def test_png_and_jpeg_require_complete_decodable_bytes(tmp_path):
    """接受真实编码的PNG/JPEG，拒绝仅有文件头、截断图和伪装格式。"""
    from PIL import Image
    verify = module()
    for format_name in ('PNG', 'JPEG'):
        path = tmp_path / ('image-' + format_name)
        Image.new('RGB', (32, 24), 'white').save(path, format_name)
        assert verify.checked_image(verify.artifact(path, tmp_path), tmp_path) == path
        path.write_bytes(path.read_bytes()[:32])
        try:
            verify.checked_image(verify.artifact(path, tmp_path), tmp_path)
        except ValueError:
            pass
        else:
            raise AssertionError('截断图像被接受')
    disguised = tmp_path / 'disguised.jpg'
    Image.new('RGB', (32, 24), 'white').save(disguised, 'GIF')
    try:
        verify.checked_image(verify.artifact(disguised, tmp_path), tmp_path)
    except ValueError:
        pass
    else:
        raise AssertionError('不支持的格式被扩展名伪装通过')


def test_linux_scope_keeps_input_visual_resume_and_identity_required(tmp_path):
    """平台拆分不能放宽缺输入/儿童视觉/重启证据或允许截图被替换。"""
    verify, report, image_path = synthetic_complete_report(tmp_path, 'PNG')
    for gate in ('actual_input', 'visual', 'save_resume'):
        missing = deepcopy(report); missing[gate] = {'status': 'BLOCKED'}
        assert verify.validate_report(missing, tmp_path)
    fake_web = deepcopy(report); fake_web['web']['status'] = 'PASS'
    assert any('web' in error.lower() for error in verify.validate_report(fake_web, tmp_path))
    unexplained_web = deepcopy(report); unexplained_web['web'].pop('reason')
    assert any('web' in error for error in verify.validate_report(unexplained_web, tmp_path))
    no_scope = deepcopy(report); no_scope.pop('scope')
    assert verify.validate_report(no_scope, tmp_path)
    unknown_scope = deepcopy(report); unknown_scope['scope'] = 'skip-native-tests'
    assert any('scope' in error for error in verify.validate_report(unknown_scope, tmp_path))
    changed_identity = deepcopy(report); changed_identity['source']['sha256'] = '0' * 64
    assert verify.validate_report(changed_identity, tmp_path)
    image_path.write_bytes(image_path.read_bytes() + b'changed')
    assert verify.validate_report(report, tmp_path)


def test_legacy_blocked_report_stays_unchanged(tmp_path):
    """历史无scope报告只按原status读取，不因Linux范围修订自动晋升或回填。"""
    verify = module()
    report = verify.empty_report(ROOT)
    report.pop('scope', None)
    before = deepcopy(report)
    assert verify.validate_report(report, tmp_path) == []
    assert report == before and report['status'] == 'BLOCKED'
