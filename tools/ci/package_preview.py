#!/usr/bin/env python3
"""Compose and package a playable preview through the existing strict resident pipeline."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'cache', 'native', 'output', 'font', 'font-license'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--preset', choices=('qingchuan', 'envoy'), default='qingchuan')
    args = parser.parse_args()
    root, cache, output = args.root.resolve(), args.cache.resolve(), args.output.resolve()
    if output.exists():
        raise RuntimeError('Preview output must be new')
    output.mkdir(parents=True)
    sys.path.insert(0, str(root))
    from tools.restore_toolchain import restore_toolchain
    from tools.content.pack_compose import compose_pack_set
    from tools.native_resident_profile import prepare_profile, validate_profile, export_profile, _entries, NATIVE_ROOTS
    source = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    native = args.native.resolve()
    receipt = json.loads((native / 'native-build-receipt.json').read_text())
    native_source = receipt['source_commit']
    if _entries(root, native_source, NATIVE_ROOTS) != _entries(root, source, NATIVE_ROOTS):
        raise RuntimeError('Preview native inputs differ from the actual native build')
    restored = restore_toolchain(cache / 'toolchain', 'local', cache / 'archives')
    os.environ['XDG_DATA_HOME'] = restored['xdg_data_home']
    os.environ['XDG_CACHE_HOME'] = str(cache / 'export-runtime/cache')
    os.environ['XDG_CONFIG_HOME'] = str(cache / 'export-runtime/config')
    engine = Path(restored['editor'])
    selected = [('local_society_1_11', 'hanmo.local_society', '1.11.0')]
    scenario = 'society::scenario.qingchuan_spring_reserve'
    if args.preset == 'envoy':
        selected += [('political_economy_1_2', 'hanmo.political_economy', '1.2.0'),
                     ('diplomatic_responses_1_0', 'hanmo.diplomatic_responses', '1.0.0'),
                     ('envoy_actor_cards_1_0', 'hanmo.envoy_actor_cards', '1.0.0')]
        scenario = 'envoycards::scenario.qingchuan_envoy_card'
    selection = {'format': 'HM-PACK-SELECTION', 'format_version': 1,
                 'packs': [{'id': key, 'version': version} for _, key, version in selected]}
    packs = [root / 'content/packs/authored' / directory for directory, _, _ in selected]
    composed = output / 'content-set'
    result = compose_pack_set(packs, selection, composed)
    (output / 'compose.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    from tools.content_stage import composed_set_record
    content = composed_set_record(composed, root)
    library = native / 'libhanmo_world_bridge.so'
    config = {'format': 'HM-NATIVE-RESIDENT-RELEASE/1', 'export_mode': 'debug',
              'engine': {'version': restored['build'], 'sha256': hashlib.sha256(engine.read_bytes()).hexdigest()},
              'native': {'source_commit': native_source, 'library_sha256': hashlib.sha256(library.read_bytes()).hexdigest()},
              'content': {'pack_set_digest': content['pack_set_digest'], 'scenario_id': scenario},
              'storage': {'profile': 'preview-' + source[:12],
                          'custom_user_dir_name': 'hanmo-preview-' + source[:12]}}
    config_path = output / 'profile.json'
    config_path.write_text(json.dumps(config, indent=2) + '\n')
    stage = output / 'stage'
    prepare_profile(root, stage, library, [composed],
                    native_build_receipt=native / 'native-build-receipt.json',
                    profile_config=config_path, font=args.font.resolve(),
                    font_license=args.font_license.resolve(), font_face_index=2)
    validate_profile(root, stage / 'game')
    packaged = output / ('hanmo-preview-linux-' + source[:12])
    export_profile(root, stage / 'game', packaged, str(engine))
    manifest = json.loads((stage / 'native-resident-stage.json').read_text())
    binding = {'source_commit': source, 'scenario_id': scenario,
               'catalog_path': manifest['content']['catalog_path'],
               'pack_set_digest': content['pack_set_digest'],
               'storage_profile': config['storage']['profile']}
    (output / 'smoke-input.json').write_text(json.dumps(binding, indent=2) + '\n')
    # Target-container smoke must pass before the existing package_profile creates a ZIP.
    (output / 'preview-path.json').write_text(json.dumps({'folder': str(packaged), 'source_commit': source}, indent=2) + '\n')
    print(packaged)


if __name__ == '__main__':
    main()
