"""只解显式许可作者版本；原法院目录与所有引用保持唯一，不制造运行事实。"""


def political_shop_author_catalog(profile):
    """返回原借用 catalog 或 None；新 wrapper 不接受 enabled 或任何额外字段。"""
    if not isinstance(profile, dict) or type(profile.get('version')) is not int:
        return None
    if profile['version'] == 1:
        catalog = profile
    elif profile['version'] == 2 and set(profile) == {'version', 'catalog'}:
        catalog = profile['catalog']
    else:
        return None
    if (not isinstance(catalog, dict) or set(catalog) != {'version', 'courts'}
            or type(catalog.get('version')) is not int or catalog['version'] != 1
            or not isinstance(catalog.get('courts'), dict)
            or not 1 <= len(catalog['courts']) <= 4096):
        return None
    return catalog


def political_shop_author_pair(profile, shops):
    """只认法律1/Shop1或2、法律2/Shop3，不能以 >= 版本暗开后继。"""
    return (political_shop_author_catalog(profile) is not None
            and isinstance(shops, dict) and shops.get('enabled') is True
            and type(shops.get('version')) is int
            and ((profile['version'] == 1 and shops['version'] in (1, 2))
                 or (profile['version'] == 2 and shops['version'] == 3)))
