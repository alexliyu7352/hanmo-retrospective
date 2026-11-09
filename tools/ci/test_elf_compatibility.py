"""只测发行 ABI 解析门；不冒称在 Ubuntu 20.04 上运行过。"""
import importlib.util
from pathlib import Path
import unittest

PATH = Path(__file__).with_name('check_elf_compatibility.py')

class AbiGateTest(unittest.TestCase):
    def test_target_versions_and_unknown_abi(self):
        self.assertTrue(PATH.exists(), 'ELF compatibility gate is missing')
        spec = importlib.util.spec_from_file_location('abi_gate', PATH)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        sample = '''Version definition section '.gnu.version_d':
          Name: GLIBC_9.99
        Version needs section '.gnu.version_r':
          Name: GLIBC_2.28
          Name: GLIBC_2.31
          Name: GLIBCXX_3.4.26
        '''
        self.assertEqual(module.glibc_violations(sample), [])
        self.assertEqual(module.glibc_violations(sample + '\n Name: GLIBC_2.38'), ['GLIBC_2.38'])
        self.assertEqual(module.glibc_violations(sample + '\n Name: GLIBC_ABI_DT_RELR'), ['GLIBC_ABI_DT_RELR'])
        self.assertEqual(module.glibc_violations(sample + '\n Name: GLIBC_PRIVATE'), ['GLIBC_PRIVATE'])
        self.assertEqual(module.glibc_violations(sample + '\n Name: GLIBC_2.9'), [])

if __name__ == '__main__':
    unittest.main()
