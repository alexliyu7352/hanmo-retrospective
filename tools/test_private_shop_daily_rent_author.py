"""日租正常声明与坏输入门；不构造运行付款或以作者声明代本人选择。"""
from copy import deepcopy
import unittest
from tools.content.private_shop_daily_rent_links import (
    CAPABILITY, DECLARATION, check_private_shop_daily_rent,
    with_private_shop_daily_rent_schema,
)


def sample():
    return {'required_capabilities': [CAPABILITY, 'private_shops_v1', 'native_dynamic_world'],
            'scenarios': [{'dynamic_world': {'profile': 'local_society_v1',
                'private_shops': {'enabled': True, 'version': 1},
                'private_shop_daily_rent': deepcopy(DECLARATION)}}]}


class DailyRentAuthor(unittest.TestCase):
    def test_exact(self):
        pack = sample(); before = deepcopy(pack)
        self.assertEqual(check_private_shop_daily_rent(pack, {}), [])
        self.assertEqual(pack, before)

    def test_old_profile_unchanged(self):
        self.assertEqual(check_private_shop_daily_rent({'scenarios': [{}]}, {}), [])

    def test_every_capability(self):
        for cap in sample()['required_capabilities']:
            pack = sample(); pack['required_capabilities'].remove(cap)
            self.assertTrue(check_private_shop_daily_rent(pack, {}))

    def test_original_pair(self):
        for shops in (None, {}, {'enabled': False, 'version': 1},
                      {'enabled': True, 'version': True}, {'enabled': True, 'version': 2}):
            pack = sample(); pack['scenarios'][0]['dynamic_world']['private_shops'] = shops
            self.assertTrue(check_private_shop_daily_rent(pack, {}))

    def test_no_runtime_fields_or_second_switch(self):
        for field in ('enabled', 'payments', 'assessments', 'last_step_tick'):
            pack = sample(); pack['scenarios'][0]['dynamic_world']['private_shop_daily_rent'][field] = True
            self.assertTrue(check_private_shop_daily_rent(pack, {}))
        pack = sample(); pack['scenarios'][0]['initial'] = {'private_shop_daily_rent': {}}
        self.assertTrue(check_private_shop_daily_rent(pack, {}))

    def test_exact_types_and_policies(self):
        for value in (None, [], {}, dict(DECLARATION, version=True),
                      dict(DECLARATION, profile='calendar_day'),
                      dict(DECLARATION, arrears_policy='infinite_debt')):
            pack = sample(); pack['scenarios'][0]['dynamic_world']['private_shop_daily_rent'] = value
            self.assertTrue(check_private_shop_daily_rent(pack, {}))

    def test_schema_is_strict_and_idempotent(self):
        schema = {'$defs': {'dynamic_world': {'properties': {}}},
                  'properties': {'required_capabilities': {'items': {'enum': []}}}}
        before = deepcopy(schema); result = with_private_shop_daily_rent_schema(schema)
        self.assertEqual(schema, before)
        self.assertEqual(with_private_shop_daily_rent_schema(result), result)
        self.assertEqual(result['$defs']['private_shop_daily_rent_catalog']['properties']['version'],
                         {'type': 'integer', 'enum': [1]})
        self.assertEqual(result['properties']['required_capabilities']['items']['enum'], [CAPABILITY])
        result['$defs']['private_shop_daily_rent_catalog']['additionalProperties'] = True
        with self.assertRaises(ValueError):
            with_private_shop_daily_rent_schema(result)

    def test_schema_requires_real_capability_table(self):
        with self.assertRaises(ValueError):
            with_private_shop_daily_rent_schema({'$defs': {'dynamic_world': {'properties': {}}}})


if __name__ == '__main__':
    unittest.main()
