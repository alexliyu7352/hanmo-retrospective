"""Check actual authored life scenes, not a second gameplay implementation."""
import json
from pathlib import Path
from tools.content.validation import validate_pack
ROOT=Path(__file__).resolve().parents[1]

def test_lived_pack_has_distinct_people_and_places():
    """The lived city must be valid authored content with working non-quest inhabitants."""
    content=json.loads((ROOT/'content/foundation/lived_city.json').read_text())
    assert validate_pack(content).ok
    assert len(content['actors']) >= 10
    assert any(r['id']=='role.inn' and 'rest' in r['services'] for r in content['roles'])
    assert any(r['id']=='role.host' and 'work' in r['services'] for r in content['roles'])
    assert any(d['id']=='dialog.carter' and any(n.get('id')=='life' for n in d['nodes']) for d in content['dialogues'])

def test_scene_assets_are_real_and_independent_of_the_game_ui():
    """Each room needs a real resource entry, and the renderer cannot use a concept poster as its UI."""
    manifest_path=ROOT/'game/assets/art/manifest.json'
    assert manifest_path.is_file(), 'Scene asset manifest is missing'
    manifest=json.loads(manifest_path.read_text())
    assert len(manifest['backgrounds'])>=5
    assert len(manifest['portraits'])>=6
    for asset in manifest['backgrounds']+manifest['portraits']:
        assert (ROOT/'game'/asset['path']).is_file()
        assert asset['status']=='temporary-derived-original'


def test_provider_cannot_draw_service_money_from_unrelated_person():
    """Role authority must still hold for the new life service definitions."""
    p=json.loads((ROOT/'content/foundation/lived_city.json').read_text())
    next(r for r in p['roles'] if r['id']=='role.inn')['activities']['rest']['wallet_id']='wallet.office'
    errors=validate_pack(p)
    assert not errors.ok
    assert any(i.code=='SERVICE_AUTHORITY' for i in errors.issues)
