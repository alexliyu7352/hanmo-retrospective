"""Real author-document behavior; no gameplay implementation or mock editor."""
import copy
import importlib
import importlib.util
import json
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SAMPLE = ROOT / 'content/foundation/two_stories.json'


def api():
    """Require the planned production module with an explicit missing-feature assertion."""
    assert importlib.util.find_spec('tools.content.document') is not None, 'AuthorDocument is not implemented'
    return importlib.import_module('tools.content.document')


def document(tmp_path):
    """Open an independent real author file for each test."""
    p = tmp_path / 'story.json'
    p.write_bytes(SAMPLE.read_bytes())
    return api().AuthorDocument.open(p)


def test_open_has_clean_state_and_revision_zero(tmp_path):
    """Opening must not edit author content or pretend to run it."""
    d = document(tmp_path)
    assert d.status()['dirty'] is False
    assert d.status()['revision'] == 0
    assert d.status()['runtime_verified'] is False


def test_snapshot_is_detached(tmp_path):
    """Views must not be able to silently mutate the single author truth."""
    d = document(tmp_path); detached = d.snapshot()
    detached['actors'][0]['attributes']['martial'] = 99
    assert d.record('actors', 'actor.player')['attributes']['martial'] == 60
    assert not d.status()['dirty']


def test_record_is_detached(tmp_path):
    """Reading a record cannot create a hidden edit outside undo history."""
    d = document(tmp_path); record = d.record('actors', 'actor.player')
    record['initial_place'] = 'nowhere'
    assert d.record('actors', 'actor.player')['initial_place'] == 'west.street'


def test_edit_undo_redo_dirty(tmp_path):
    """One field edit is one undo item and undo to the saved state clears dirty."""
    d = document(tmp_path)
    d.set_value('actors', 'actor.player', '/attributes/martial', 72, expected_revision=0)
    assert d.record('actors', 'actor.player')['attributes']['martial'] == 72
    assert d.status()['dirty'] and d.status()['revision'] == 1
    d.undo()
    assert d.record('actors', 'actor.player')['attributes']['martial'] == 60
    assert not d.status()['dirty'] and d.status()['revision'] == 2
    d.redo()
    assert d.record('actors', 'actor.player')['attributes']['martial'] == 72
    assert d.status()['revision'] == 3


def test_noop_does_not_consume_history(tmp_path):
    """Reapplying the same value is not a new author intent."""
    d = document(tmp_path); d.set_value('actors', 'actor.player', '/attributes/martial', 60)
    assert d.status()['revision'] == 0 and not d.status()['can_undo']


def test_old_view_revision_rejected(tmp_path):
    """A stale panel must not overwrite another panel's accepted edit."""
    d = document(tmp_path); d.set_text('name.actor.player', '新名字')
    before = d.snapshot()
    with pytest.raises(api().DocumentError) as exc:
        d.set_value('actors', 'actor.player', '/attributes/martial', 75, expected_revision=0)
    assert exc.value.code == 'STALE_REVISION'
    assert d.snapshot() == before


def test_new_edit_after_undo_invalidates_redo(tmp_path):
    """Branching author history must not replay an obsolete edit."""
    d = document(tmp_path); d.set_text('name.actor.player', '甲'); d.undo()
    d.set_text('name.actor.player', '乙')
    assert not d.status()['can_redo']


@pytest.mark.parametrize('pointer,value', [('/id', 'actor.renamed'), ('', {}),
    ('/attributes/missing', 1), ('/attributes/~9wrong', 1), ('/absent/path', 1)])
def test_invalid_edit_is_atomic(tmp_path, pointer, value):
    """Bad pointers and identity edits must leave data and history unchanged."""
    d = document(tmp_path); before = d.snapshot()
    with pytest.raises(api().DocumentError):
        d.set_value('actors', 'actor.player', pointer, value)
    assert d.snapshot() == before and d.status()['revision'] == 0


def test_non_json_value_rejected(tmp_path):
    """A caller cannot insert Python objects into a serializable author document."""
    d = document(tmp_path)
    with pytest.raises(api().DocumentError):
        d.set_value('actors', 'actor.player', '/appearance_note', {1, 2})
    assert not d.status()['dirty']


def test_invalid_semantic_draft_retained_with_diagnostics(tmp_path):
    """Authors may temporarily create a broken link without losing their edit."""
    d = document(tmp_path)
    d.set_value('actors', 'actor.player', '/initial_place', 'missing.place')
    report = d.validate()
    assert report['status'] == 'INVALID_CONTENT'
    assert any('initial_place' in i['path'] for i in report['issues'])
    assert d.status()['dirty']


def test_add_record_and_undo(tmp_path):
    """Adding an NPC uses data, not a core code branch, and can be undone."""
    d = document(tmp_path); r = d.record('actors', 'actor.player'); r['id'] = 'actor.new'
    d.add_record('actors', r)
    r['attributes']['martial'] = 100
    assert d.record('actors', 'actor.new')['attributes']['martial'] == 60
    d.undo()
    assert not any(x['id'] == 'actor.new' for x in d.snapshot()['actors'])


def test_add_id_unique_across_tables(tmp_path):
    """Entity identity cannot collide with a different record kind."""
    d = document(tmp_path); r = d.record('actors', 'actor.player'); r['id'] = 'west.street'
    with pytest.raises(api().DocumentError) as exc: d.add_record('actors', r)
    assert exc.value.code == 'DUPLICATE_ID'


def test_references_find_nested_conditions(tmp_path):
    """Reference discovery includes deeply nested dialogue conditions."""
    d = document(tmp_path); refs = d.references_to('offers', 'offer.grain')
    assert any('/conditions/0/terms/' in r['path'] for r in refs)
    assert any(r['source_id'] == 'dialog.office' for r in refs)


def test_referenced_delete_refused_with_paths(tmp_path):
    """Deletion cannot silently invalidate jobs, private recipients or dialogue."""
    d = document(tmp_path); before = d.snapshot()
    with pytest.raises(api().DocumentError) as exc: d.delete_record('actors', 'actor.player')
    assert exc.value.code == 'REFERENCED_RECORD'
    assert exc.value.details['references']
    assert d.snapshot() == before


def test_reference_check_survives_incomplete_sibling_fields(tmp_path):
    """An invalid draft must not hide its existing target references."""
    d = document(tmp_path)
    d.set_value('dialogues', 'dialog.office', '/nodes/3/options/2/conditions/0/terms/0/state', 'unfinished')
    refs = d.references_to('offers', 'offer.grain')
    assert any('/conditions/0/terms/0/offer_id' in r['path'] for r in refs)


def test_delete_unused_copy_then_undo(tmp_path):
    """Unreferenced authored records are deletable, with a reversible result."""
    d = document(tmp_path); r = d.clone_record('actors', 'actor.player')
    d.delete_record('actors', r['id'])
    assert not any(x['id'] == r['id'] for x in d.snapshot()['actors'])
    d.undo(); assert d.record('actors', r['id'])['id'] == r['id']


def test_clone_npc_keeps_external_identity_links(tmp_path):
    """Copying an NPC does not clone its town, office, shop or other characters."""
    d = document(tmp_path); r = d.clone_record('actors', 'actor.chen', 'actor.other')
    original = d.record('actors', 'actor.chen')
    assert r['id'] == 'actor.other'
    assert r['initial_place'] == original['initial_place']
    assert r['dialogue_id'] == original['dialogue_id']
    assert r['name_key'] != original['name_key']


def test_clone_dialogue_remaps_internal_edges_and_texts(tmp_path):
    """A cloned conversation has isolated editable text and correctly redirected local edges."""
    d = document(tmp_path); old = d.record('dialogues', 'dialog.office')
    clone = d.clone_record('dialogues', 'dialog.office', 'dialog.office.copy')
    old_nodes = {x['id'] for x in old['nodes']}; new_nodes = {x['id'] for x in clone['nodes']}
    assert not old_nodes & new_nodes
    assert clone['entry'] in new_nodes
    for n in clone['nodes']:
        for key in ['next','if_true','if_false','on_success','on_failure']:
            if key in n: assert n[key] in new_nodes
        for opt in n.get('options', []): assert opt['next'] in new_nodes
    old_key = next(n['text_key'] for n in old['nodes'] if n['kind']=='say')
    new_key = next(n['text_key'] for n in clone['nodes'] if n['kind']=='say')
    assert old_key != new_key
    d.set_text(new_key, '副本专用对白')
    assert d.snapshot()['texts'][old_key] != '副本专用对白'
    assert clone['context_role'] == old['context_role']
    commands = [n['command'] for n in clone['nodes'] if n['kind'] == 'action']
    assert any(c.get('offer_id') == 'offer.grain' for c in commands)


def test_clone_is_one_undo_for_record_and_copied_texts(tmp_path):
    """Undoing a clone must remove the new record and its newly created text keys together."""
    d = document(tmp_path); before=d.snapshot()
    d.clone_record('dialogues', 'dialog.office'); d.undo()
    assert d.snapshot() == before


def test_clone_auto_id_avoids_collision(tmp_path):
    """Repeated duplicates must not reuse the first generated stable ID."""
    d = document(tmp_path)
    a=d.clone_record('actors','actor.player'); b=d.clone_record('actors','actor.player')
    assert a['id'] != b['id']


def test_rename_display_text_keeps_id_and_relationships(tmp_path):
    """A display-name change cannot change the identity referenced by relationships."""
    d=document(tmp_path); before=d.snapshot()['relations']
    d.set_text('name.actor.chen','新的称呼')
    assert d.record('actors','actor.chen')['id']=='actor.chen'
    assert d.snapshot()['relations']==before


def test_history_is_bounded(tmp_path):
    """Unlimited typing cannot retain unbounded full-pack history."""
    d=document(tmp_path)
    for i in range(70): d.set_text('name.actor.player',str(i))
    assert d.status()['undo_count'] <= 64
    assert d.status()['history_bytes'] <= 16*1024*1024


def test_empty_history_returns_no_change(tmp_path):
    """Undo/redo without available history is an explicit no-op."""
    d=document(tmp_path)
    assert d.undo()['changed'] is False and d.redo()['changed'] is False
    assert d.status()['revision']==0


def test_malformed_file_not_opened(tmp_path):
    """Duplicate JSON properties cannot silently replace authored definitions."""
    p=tmp_path/'bad.json';p.write_text('{"format":"HM-CONTENT","format":"oops"}')
    with pytest.raises(api().DocumentError):api().AuthorDocument.open(p)


@pytest.mark.parametrize('table', ['missing', '', None])
def test_unknown_table_returns_structured_error(tmp_path, table):
    """An invalid panel target must not leak a KeyError instead of an editor diagnostic."""
    d=document(tmp_path)
    with pytest.raises(api().DocumentError) as exc:
        d.set_value(table,'actor.player','/appearance_note','x')
    assert exc.value.code=='UNKNOWN_TABLE'


def test_nonstring_mapping_key_rejected_cleanly(tmp_path):
    """Python callers cannot insert a non-JSON mapping key even in unfinished draft fields."""
    d=document(tmp_path)
    with pytest.raises(api().DocumentError):
        d.set_value('actors','actor.player','/appearance_note',{1:'bad'})
    assert not d.status()['dirty']


def test_long_dialogue_ids_remap_without_collision(tmp_path):
    """Truncating an 80-character node ID must not collide with another newly allocated ID."""
    d=document(tmp_path); a='n'*79+'a'; b='n'*79+'b'
    original=d.record('dialogues','dialog.office')
    original['id']='dialog.long';original['entry']=a
    original['nodes']=[{'id':a,'kind':'say','speaker':'$provider','text_key':'office.hello','next':b},
                       {'id':b,'kind':'end'}]
    d.add_record('dialogues',original)
    clone=d.clone_record('dialogues','dialog.long')
    assert len({n['id'] for n in clone['nodes']})==2
    assert clone['nodes'][0]['next']==clone['nodes'][1]['id']


def test_metadata_edit_version_and_declarations(tmp_path):
    """Adding a supported content type requires editable version and capability declarations."""
    d=document(tmp_path)
    assert hasattr(d,'set_metadata'), 'Author metadata edits are not implemented'
    d.set_metadata('version','0.3.2')
    assert d.snapshot()['version']=='0.3.2'
    d.undo(); assert d.snapshot()['version']=='0.3.1'
    with pytest.raises(api().DocumentError): d.set_metadata('schema_version',99)


def test_unknown_discriminator_keeps_conservative_reference(tmp_path):
    """An unfinished condition type cannot hide its remaining typed reference fields from deletion checks."""
    d=document(tmp_path)
    d.set_value('dialogues','dialog.office','/nodes/0/condition/op','unfinished')
    refs=d.references_to('offers','offer.grain')
    assert any(r['path']=='/dialogues/0/nodes/0/condition/offer_id' for r in refs)


def test_invalid_option_container_clone_refused_cleanly(tmp_path):
    """An incomplete choice editor field must return a repairable diagnostic, never a partial clone."""
    d=document(tmp_path);d.set_value('dialogues','dialog.office','/nodes/3/options','unfinished')
    before=d.snapshot()
    with pytest.raises(api().DocumentError) as exc:d.clone_record('dialogues','dialog.office')
    assert exc.value.code=='INVALID_DIALOGUE' and d.snapshot()==before


def test_free_text_matching_id_is_not_a_reference(tmp_path):
    """Only schema-defined identity fields count as references, not a name mentioned in prose."""
    d=document(tmp_path);r=d.clone_record('actors','actor.player')
    d.set_text('note.free_text',r['id'])
    assert not d.references_to('actors',r['id'])
    d.delete_record('actors',r['id'])
