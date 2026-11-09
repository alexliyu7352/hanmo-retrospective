"""验证 Godot 构建使用的真实内容准备管线；不把 Python 测试当作游戏运行。"""
from __future__ import annotations
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import pytest

ROOT=Path(__file__).resolve().parents[1]


def build_tool():
    """载入真实构建入口；入口缺失或导入失败时直接报告错误。"""
    spec=importlib.util.spec_from_file_location('hanmo_runtime_build',ROOT/'tools/build.py')
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def skeleton(tmp_path):
    """复制实际定义与管线到隔离工程；排除本地 SDK，不改作者原件。"""
    from tests.test_content_discovery import prepare_root
    prepare_root(tmp_path)
    return tmp_path


def test_real_build_exposes_author_staging():
    """导入器必须提供内容准备入口，避免引擎打开尚未生成的资源。"""
    assert callable(getattr(build_tool(),'stage_content',None))


def test_stage_uses_actual_author_pipeline(tmp_path):
    """两份作者包经真实管线生成后身份正确，原始内容字节保持不变。"""
    build=build_tool()
    assert callable(getattr(build,'stage_content',None))
    root=skeleton(tmp_path)
    source=root/'content/foundation/two_stories.json'
    before=source.read_bytes()
    build.stage_content(root)
    staged=root/'game/data/generated'
    assert json.loads((staged/'two_stories.hmcatalog.json').read_text())['catalog']['actors']['actor.shen']['id']=='actor.shen'
    assert json.loads((staged/'procurement.hmcatalog.json').read_text())['content_id']=='foundation.procurement'
    assert (staged/'content.schema.json').read_bytes()==(root/'schemas/content.schema.json').read_bytes()
    assert source.read_bytes()==before
    declaration = json.loads((staged/'runtime_capabilities.json').read_text())
    assert declaration['runtime_verified'] is False
    assert len(declaration['capabilities']) == 18
    assert declaration['rule_capabilities_sha256'] == json.loads((staged/'two_stories.hmcatalog.json').read_text())['rule_capabilities_sha256']


def test_bad_author_content_prevents_staging(tmp_path):
    """真实缺失场所必须由同一校验器拒绝，不能生成成功目录。"""
    build=build_tool()
    assert callable(getattr(build,'stage_content',None))
    root=skeleton(tmp_path)
    path=root/'content/foundation/two_stories.json'
    source=json.loads(path.read_text()); source['actors'][0]['initial_place']='nonexistent'
    path.write_text(json.dumps(source))
    with pytest.raises(RuntimeError): build.stage_content(root)
    assert not (root/'game/data/generated/two_stories.hmcatalog.json').exists()


def test_generated_content_not_a_second_distributable_author_source(tmp_path):
    """运行时生成目录不是作者真值，不得进入源码归档。"""
    generated=tmp_path/'game/data/generated'; generated.mkdir(parents=True)
    (generated/'two_stories.hmcatalog.json').write_text('{}')
    (tmp_path/'README.md').write_text('real source')
    assert [p.name for p in build_tool().source_files(tmp_path)]==['README.md']
