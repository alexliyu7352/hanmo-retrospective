"""Real filesystem and subprocess tests of content preparation, not runtime simulation."""
import copy
import hashlib
import importlib
import json
from pathlib import Path
import subprocess
import sys
import pytest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
SOURCE=ROOT/'content/foundation/two_stories.json'


def io_api():
    """Load the production IO module after asserting it has actually been implemented."""
    assert (ROOT/'tools/content/io.py').exists(), 'Content preparation is not implemented'
    return importlib.import_module('tools.content.io')


def write_case(tmp_path, raw):
    """Write exact invalid bytes rather than mocking a JSON loader."""
    p=tmp_path/'source.json';p.write_bytes(raw);return p


@pytest.mark.parametrize('raw',[
 b'{"id":"a","id":"b"}',b'{"a":NaN}',b'{"a":Infinity}',b'{"a":1e999}',
 b'{"a":"\xff"}',b'{not json}',b'[]',b'null',b'{"a":"\\ud800"}',
 b'['*40+b'0'+b']'*40,
])
def test_bad_json_does_not_enter_content_validation(tmp_path,raw):
    """Duplicate keys, invalid bytes and non-JSON values must be diagnosed explicitly."""
    with pytest.raises(ValueError):io_api().read_pack(write_case(tmp_path,raw))


def test_size_limit_before_loading(tmp_path):
    """The loader must bound file bytes rather than allocate an arbitrarily large document."""
    p=tmp_path/'oversized.json';p.write_bytes(b' '* (2*1024*1024+1))
    with pytest.raises(ValueError):io_api().read_pack(p)


def test_read_missing_file_has_actionable_error(tmp_path):
    """Missing content is reported as a content read failure, not an empty world."""
    with pytest.raises(ValueError):io_api().read_pack(tmp_path/'missing.json')


def test_prepare_is_byte_reproducible(tmp_path):
    """Identical source must yield byte-identical indexes at different output paths."""
    a=tmp_path/'a.hmcatalog.json';b=tmp_path/'b.hmcatalog.json'
    io_api().prepare_pack(SOURCE,a);io_api().prepare_pack(SOURCE,b)
    assert a.read_bytes()==b.read_bytes()
    data=json.loads(a.read_text())
    assert data['runtime_verified'] is False
    assert data['status']=='AUTHORING_ONLY'
    assert data['format']=='HM-CONTENT-INDEX'
    assert set(data['catalog']['offers'])=={'offer.grain','offer.letter'}
    assert data['source_sha256']==hashlib.sha256(SOURCE.read_bytes()).hexdigest()


def test_invalid_prepare_preserves_old_artifact(tmp_path):
    """A bad edit must not destroy the previously valid content build."""
    output=tmp_path/'latest.hmcatalog.json';output.write_text('previous valid artifact')
    p=json.loads(SOURCE.read_text());p['offers'][0]['funding_wallet']='missing'
    source=tmp_path/'bad.json';source.write_text(json.dumps(p))
    with pytest.raises(ValueError):io_api().prepare_pack(source,output)
    assert output.read_text()=='previous valid artifact'
    assert not list(tmp_path.glob('*.tmp'))


def test_cannot_overwrite_source():
    """Content compilation must never overwrite the authored source itself."""
    before=SOURCE.read_bytes()
    with pytest.raises(ValueError):io_api().prepare_pack(SOURCE,SOURCE)
    assert SOURCE.read_bytes()==before


def test_cannot_overwrite_other_authored_file():
    """Protection covers the source tree, not only the exact input filename."""
    out=ROOT/'content/foundation/procurement.json';before=out.read_bytes()
    with pytest.raises(ValueError):io_api().prepare_pack(SOURCE,out)
    assert out.read_bytes()==before


def test_cannot_write_game_source():
    """A CLI output argument cannot replace a gameplay script."""
    out=ROOT/'game/core/world_kernel.gd';before=out.read_bytes()
    with pytest.raises(ValueError):io_api().prepare_pack(SOURCE,out)
    assert out.read_bytes()==before


def test_dialogue_array_order_survives_preparation(tmp_path):
    """Canonical dictionary indexing must not reorder a dialogue's authored node list."""
    out=tmp_path/'p.hmcatalog.json';io_api().prepare_pack(SOURCE,out)
    source=json.loads(SOURCE.read_text());actual=json.loads(out.read_text())
    assert actual['catalog']['dialogues']['dialog.office']['nodes']==source['dialogues'][0]['nodes']


def test_add_second_content_does_not_edit_runtime(tmp_path):
    """This proves content preparation only, not that Godot can already run the second story."""
    files=list((ROOT/'game').rglob('*.gd'))
    before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    io_api().prepare_pack(ROOT/'content/foundation/procurement.json',tmp_path/'one.hmcatalog.json')
    io_api().prepare_pack(SOURCE,tmp_path/'two.hmcatalog.json')
    after={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    assert before==after
    a=json.loads((tmp_path/'one.hmcatalog.json').read_text())
    b=json.loads((tmp_path/'two.hmcatalog.json').read_text())
    assert len(b['catalog']['actors'])>len(a['catalog']['actors'])
    assert len(b['catalog']['offers'])==2


def run_cli(*args):
    """Run the actual command-line entrypoint using the current interpreter."""
    assert (ROOT/'tools/content_pipeline.py').exists(), 'Content CLI is not implemented'
    return subprocess.run([sys.executable,str(ROOT/'tools/content_pipeline.py'),*map(str,args)],capture_output=True,text=True)


def test_cli_lint_reports_static_scope():
    """A lint pass must remain distinct from game/editor verification."""
    r=run_cli('lint',SOURCE)
    assert r.returncode==0,r.stderr
    assert json.loads(r.stdout)['runtime_verified'] is False


def test_cli_prepare_creates_real_output(tmp_path):
    """The CLI must write the actual checked index, not merely print success."""
    out=tmp_path/'content.hmcatalog.json'
    r=run_cli('prepare',SOURCE,'--output',out)
    assert r.returncode==0,r.stderr
    assert out.exists()
    assert json.loads(r.stdout)['status']=='AUTHORING_ONLY'


def test_cli_invalid_pack_returns_nonzero(tmp_path):
    """Malformed content cannot leave a zero exit code in a build pipeline."""
    r=run_cli('lint',write_case(tmp_path,b'{"hello":"world"}'))
    assert r.returncode!=0
    assert json.loads(r.stdout)['status']=='INVALID_CONTENT'


def test_current_runtime_manifest_matches_but_is_not_itself_playtest():
    """Native runtime evidence is recorded separately; static capability matching cannot certify gameplay."""
    r=run_cli('check-target',SOURCE,'--runtime',ROOT/'content/foundation/runtime_capabilities.json')
    assert r.returncode==0,r.stdout
    report=json.loads(r.stdout)
    assert report['status']=='DECLARED_CAPABILITIES_MATCH'
    assert report['runtime_verified'] is False


def test_explicit_legacy_runtime_still_refuses_new_content(tmp_path):
    """Retain the old E0 compatibility refusal with a labeled historic manifest fixture."""
    fixture={'format':'HM-RUNTIME-CAPABILITIES','content_abi':1,'runtime_verified':False,'engine_build':None,'capabilities':[]}
    file=tmp_path/'e0-historic.json'; file.write_text(json.dumps(fixture))
    r=run_cli('check-target',SOURCE,'--runtime',file)
    assert r.returncode==3
    assert 'tasks.message' in json.loads(r.stdout)['missing_capabilities']


def test_matching_declared_capabilities_is_not_playtest(tmp_path):
    """A synthetic capability manifest can test matching but cannot certify execution."""
    fixture={'format':'HM-RUNTIME-CAPABILITIES','content_abi':1,'runtime_verified':True,'engine_build':'TEST-ONLY','capabilities':json.loads(SOURCE.read_text())['required_capabilities']}
    f=tmp_path/'caps.json';f.write_text(json.dumps(fixture))
    r=run_cli('check-target',SOURCE,'--runtime',f)
    assert r.returncode==0,r.stdout
    assert json.loads(r.stdout)['status']=='DECLARED_CAPABILITIES_MATCH'
    assert json.loads(r.stdout)['runtime_verified'] is False


def test_wrong_content_abi_rejected(tmp_path):
    """Even listed capabilities cannot override incompatible content ABI."""
    fixture={'format':'HM-RUNTIME-CAPABILITIES','content_abi':2,'runtime_verified':True,'engine_build':'TEST-ONLY','capabilities':json.loads(SOURCE.read_text())['required_capabilities']}
    f=tmp_path/'caps.json';f.write_text(json.dumps(fixture))
    r=run_cli('check-target',SOURCE,'--runtime',f)
    assert r.returncode==3


def test_missing_runtime_manifest_is_actionable(tmp_path):
    """Missing runtime evidence returns a failure, not a fabricated compatibility claim."""
    r=run_cli('check-target',SOURCE,'--runtime',tmp_path/'missing.json')
    assert r.returncode!=0
    assert json.loads(r.stdout)['status']=='IO_ERROR'


def test_source_digest_uses_the_validated_snapshot(tmp_path, monkeypatch):
    """An edit after validation must not label old content with a newer file's digest."""
    api = io_api()
    original = SOURCE.read_bytes()
    source = tmp_path / 'editable.json'
    source.write_bytes(original)
    output = tmp_path / 'checked.hmcatalog.json'
    real_validate = api.validate_pack

    def edit_after_validation(pack, filename):
        """Inject a real editor save between validation and prepared-file creation."""
        report = real_validate(pack, filename)
        edited = copy.deepcopy(pack)
        edited['version'] = '0.3.99'
        source.write_text(json.dumps(edited, ensure_ascii=False), encoding='utf8')
        return report

    monkeypatch.setattr(api, 'validate_pack', edit_after_validation)
    api.prepare_pack(source, output)
    artifact = json.loads(output.read_text(encoding='utf8'))
    assert artifact['source_sha256'] == hashlib.sha256(original).hexdigest()
    assert artifact['content_version'] == json.loads(original)['version']
    assert source.read_bytes() != original
