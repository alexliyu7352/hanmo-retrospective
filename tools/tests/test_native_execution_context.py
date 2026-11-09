"""CMake真实结构解析的纯合成正负例；不运行GCC/配置/真实context捕获。"""
import importlib.util
from pathlib import Path
import unittest


class ExecutionContextTests(unittest.TestCase):
    """缺失隐式/不存在前根、错driver与重复语言均不能造完整search证据。"""
    def setUp(self):
        """只导入纯读取模块；不调用derive里的既有技术verifier。"""
        p=Path(__file__).resolve().parents[1]/'native/execution_context.py'
        spec=importlib.util.spec_from_file_location('test_context',p);self.m=importlib.util.module_from_spec(spec);spec.loader.exec_module(self.m)
        self.reply={'kind':'toolchains','version':{'major':1},'toolchains':[{'language':'CXX','compiler':{'id':'GNU','path':'/cc','implicit':{'includeDirectories':['/include','/usr/include']}}}]}

    def test_actual_toolchain_shape(self):
        """保真实root顺序，不以旧MD路径代替FileAPI隐式根。"""
        self.assertEqual(self.m.implicit_roots(self.reply,{'/cc':{}}),['/include','/usr/include'])

    def test_missing_and_foreign_toolchain_rejected(self):
        """FileAPI缺真实selected driver或根不能为空成功。"""
        with self.assertRaises(RuntimeError):self.m.implicit_roots(self.reply,{'/other':{}})
        self.reply['toolchains'][0]['compiler']['implicit']['includeDirectories']=[]
        with self.assertRaises(RuntimeError):self.m.implicit_roots(self.reply,{'/cc':{}})

    def test_absent_search_root_retained(self):
        """GCC忽略的缺目录仍是未来可新增shadow的原搜索边界。"""
        self.assertEqual(self.m.absent_search_roots('  ignoring nonexistent directory "/gcc/include-fixed"\n'),['/gcc/include-fixed'])
        with self.assertRaises(RuntimeError):self.m.absent_search_roots('no verbose source')
        with self.assertRaises(RuntimeError):self.m.absent_search_roots('ignoring nonexistent directory "relative"')


if __name__=='__main__':unittest.main()
