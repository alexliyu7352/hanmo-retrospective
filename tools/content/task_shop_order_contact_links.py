"""显式店单接触尾只证明原真实开工；无段旧包不改变准入。"""
def check_task_shop_order_contacts(pack,index):
    errors=[]
    for number,scene in enumerate(pack.get('scenarios',[])):
        if 'task_shop_order_contact_profile' not in scene:continue
        selected=scene['task_shop_order_contact_profile']
        path=('scenarios',number,'task_shop_order_contact_profile')
        def error(code,text):errors.append((code,path,text))
        if selected!='original_shop_order_contacts_v1':error('TASK_SHOP_ORDER_CONTACT_PROFILE','未知店单接触来源版本');continue
        caps=set(pack.get('required_capabilities',[]))
        if not {'task_shop_order_contacts_v1','task_shop_orders_v1','family_life_v1'}<=caps:error('TASK_SHOP_ORDER_CONTACT_CAPABILITY','须显式真实店单与生命来源能力')
        dynamic=scene.get('dynamic_world',{})
        shop=dynamic.get('private_shops',{}) if isinstance(dynamic,dict) else {}
        orders=shop.get('orders',{}) if isinstance(shop,dict) else {}
        if scene.get('task_shop_order_profile')!='private_shop_orders_v1' or not isinstance(dynamic,dict) or dynamic.get('profile')!='local_society_v1' or not isinstance(orders,dict) or not orders.get('policies'):error('TASK_SHOP_ORDER_CONTACT_ORIGINAL','须原已声明私铺与非空真实店单政策')
        if not isinstance(scene.get('family_life'),dict) or scene['family_life'].get('profile')!='verified_military_deaths_v1':error('TASK_SHOP_ORDER_CONTACT_LIFE','须原真实生命历史，不以今日存活补旧接触')
    return errors
