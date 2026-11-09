"""实际World预约路由源门；行为由原Task纯读和正式银行另核。"""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class ShopReservationRegistration(unittest.TestCase):
    def test_actual_contract_router(self):
        source=(ROOT/'native/world/src/resident/kernel/dynamic_access.cpp').read_text()
        block=source.split('bool BundleServices::reservation_exists(',1)[1].split('OrganizationAccess BundleServices::organization_access()',1)[0]
        line=next(line for line in block.splitlines() if 'task_contract_reservation_exists' in line)
        self.assertIn('if(ref.kind==ReservationRef::Kind::contract)',line)
        self.assertIn('task_contract_reservation_exists(world_.tasks,ref.id)',line)
        self.assertIn('world_.governance_tasks_enabled()&&governance_reservation_exists(world_.tasks,world_.governance_tasks,ref.id)',line)
        self.assertIn('world_.task_shop_orders_enabled()&&world_.tasks.shop_orders&&task_shop_order_reservation_exists(world_.tasks,*world_.tasks.shop_orders,ref.id)',line)
        self.assertLess(block.index('if(!world_.dynamic_enabled())return false;'),block.index(line))
        self.assertNotIn('task_shop_order_contract:',block)
        self.assertIn('return ref.kind==ReservationRef::Kind::action&&world_.common.action(ref.id)',block)
if __name__=='__main__':unittest.main()
