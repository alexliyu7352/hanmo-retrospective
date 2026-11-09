"""新司法提交尾的严格作者门，原包无开关时原样通过。"""
import copy
import unittest
from content.political_shop_licensing_result_links import NAME, CAPABILITY, check_political_shop_licensing_receipts, with_political_shop_licensing_receipt_schema


def fixture():
    return {'required_capabilities': [CAPABILITY], 'scenarios': [{'dynamic_world': {
        'political_shop_licensing': {'version': 1, 'courts': {}},
        'private_shops': {'version': 1, 'enabled': True, 'premises': {}}, NAME: {'version': 1}}}]}


class LegalReceiptsAuthoring(unittest.TestCase):
    def test_explicit_and_old(self):
        self.assertEqual(check_political_shop_licensing_receipts(fixture(), {}), [])
        source = fixture(); source['scenarios'][0]['dynamic_world'].pop(NAME)
        self.assertEqual(check_political_shop_licensing_receipts(source, {}), [])
    def test_capability(self):
        source = fixture(); source['required_capabilities'] = []
        self.assertTrue(check_political_shop_licensing_receipts(source, {}))
    def test_unknown_or_boolean_version(self):
        for value in [True, 0, 2, '1', None]:
            source = fixture(); source['scenarios'][0]['dynamic_world'][NAME]['version'] = value
            self.assertTrue(check_political_shop_licensing_receipts(source, {}))
    def test_no_authored_runtime(self):
        source = fixture(); source['scenarios'][0]['dynamic_world'][NAME]['bindings'] = {}
        self.assertTrue(check_political_shop_licensing_receipts(source, {}))
    def test_dependencies(self):
        for key in ['political_shop_licensing', 'private_shops']:
            source = fixture(); del source['scenarios'][0]['dynamic_world'][key]
            self.assertTrue(check_political_shop_licensing_receipts(source, {}))
    def test_schema_pure_idempotent(self):
        source = {'$defs': {'dynamic_world': {'properties': {'old': {'const': 7}}}}}; before = copy.deepcopy(source)
        result = with_political_shop_licensing_receipt_schema(source)
        self.assertEqual(source, before); self.assertEqual(result['$defs']['dynamic_world']['properties']['old'], {'const': 7})
        self.assertEqual(with_political_shop_licensing_receipt_schema(result), result)
        result['$defs']['dynamic_world']['properties'][NAME]['required'] = []
        with self.assertRaises(ValueError): with_political_shop_licensing_receipt_schema(result)


if __name__ == '__main__':
    unittest.main()
