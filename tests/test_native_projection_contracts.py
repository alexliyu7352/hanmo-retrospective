"""限定离线投影的构建来源与GD归属登记；不把静态合同检查当作原生查询验收。"""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]


def test_native_production_files_are_part_of_build_identity():
    """所有实际本库源和公共头必须进入显式摘要及重配置依赖，防止新增头漏登记。"""
    root = ROOT / 'native/world'
    text = (root / 'CMakeLists.txt').read_text()
    match = re.search(r'foreach\(path (.*?)\)', text, re.S)
    assert match is not None
    covered = set(match.group(1).split())
    expected = {'CMakeLists.txt', 'src/build_identity.hpp.in', 'tools/sim_main.cpp'}
    for directory in ('src', 'include'):
        expected.update(str(path.relative_to(root)) for path in (root / directory).rglob('*')
                        if path.is_file() and path.suffix in {'.cpp', '.hpp'})
    assert expected <= covered, sorted(expected - covered)
    assert 'CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${path}"' in text
    assert 'file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${path}" sha)' in text


def test_legacy_codec_uses_existing_schema_and_ownership_sources():
    """codec只声明两个旧依赖；开发缓存是唯一调用接缝，Kernel/Main不直接读取codec。"""
    data = json.loads((ROOT / 'game/data/module_contracts.json').read_text())
    source = 'game/core/backends/legacy_projection_codec.gd'
    assert data['files'][source] == 'builtin'
    assert {edge['target'] for edge in data['source_edges'] if edge['source'] == source} == {
        'game/core/state_schema.gd', 'game/core/runtime/world_validation.gd'
    }
    for path in ('game/core/world_kernel.gd', 'game/core/backends/legacy_world_backend.gd', 'game/ui/main.gd'):
        assert 'legacy_projection_codec.gd' not in (ROOT / path).read_text()


def test_bridge_export_map_is_hashed_and_relinked():
    """桥的直接对象与静态库共同只导出C入口，导出表改变须刷新身份且重链。"""
    source = ROOT / 'native/godot'
    cmake = (source / 'CMakeLists.txt').read_text()
    covered = re.search(r'foreach\(path CMakeLists\.txt (.*?)\)', cmake, re.S)
    assert covered is not None and 'exports.map' in covered.group(1).split()
    assert 'LINKER:--exclude-libs,ALL' in cmake
    assert 'LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map' in cmake
    assert 'LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/exports.map"' in cmake
    script = (source / 'exports.map').read_text()
    assert re.search(r'global:\s*hanmo_world_library_init\s*;', script)
    assert re.search(r'local:\s*\*\s*;', script)


def test_actor_bridge_gate_is_explicitly_isolated():
    """新Godot门必须由固定库与冻结工件准备，不能混入默认游戏／裸跑清单。"""
    from tools import verify
    name = 'test_native_actor_commitments_bridge'
    assert name not in verify.TESTS
    assert 'native_actor_commitments_bridge/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert '默认OFF' in verify.EXCLUDED_TESTS[name]
    assert '完整M1' in verify.EXCLUDED_TESTS[name]


def test_live_growth_read_remains_an_explicit_development_adapter():
    """只登记按需适配叶与隔离fixture，默认游戏不启原生读取或构建第二个世界。"""
    from tools import verify
    data = json.loads((ROOT / 'game/data/module_contracts.json').read_text())
    cache = 'game/core/backends/native_growth_read_cache.gd'
    backend = 'game/core/backends/legacy_world_backend.gd'
    codec = 'game/core/backends/legacy_projection_codec.gd'
    assert data['files'][cache] == 'builtin'
    edges = {(edge['source'], edge['target']) for edge in data['source_edges']}
    assert (backend, cache) in edges and (cache, codec) in edges
    assert 'native_growth_action_enabled' not in (ROOT / 'game/project.godot').read_text()
    name = 'test_live_native_growth_action'
    assert name not in verify.TESTS
    assert 'live_native_growth_action/prepare_fixture.py' in verify.EXCLUDED_TESTS[name]
    assert '默认OFF' in verify.EXCLUDED_TESTS[name]
