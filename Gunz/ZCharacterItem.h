#ifndef _ZCHARACTERITEM_H
#define _ZCHARACTERITEM_H

#include "MMatchItem.h"
#include "ZItem.h"
#include "ZFile.h"
#include <list>
#include <algorithm>
using namespace std;

struct BulletInfo;

/// ĳ���Ͱ� ����ϰ� �ִ� �����۵�
class ZCharacterItem
{
private:
protected:
	ZItem					m_Items[MMCIP_END];
	MMatchCharItemParts		m_nSelectedWeapon;		// ������ Item�� ���� m_Items�� �ش��ϴ� �ε���
	bool Confirm(MMatchCharItemParts parts, MMatchItemDesc* pDesc);
	bool IsWeaponItem(MMatchCharItemParts parts);
public:
	ZCharacterItem();
	virtual ~ZCharacterItem();
	void SelectWeapon(MMatchCharItemParts parts);
	bool EquipItem(MMatchCharItemParts parts, int nItemDescID);

	bool Reload();

	ZItem* GetItem(MMatchCharItemParts parts)
	{
		if ((parts < MMCIP_HEAD) || (parts >= MMCIP_END))
		{
			_ASSERT(0);
			return NULL;
		}
		return &m_Items[(int)parts]; 
	}
	MMatchItemDesc *GetDesc(MMatchCharItemParts slot)
	{
		ZItem *pItem = GetItem(slot);
		if (!pItem)
			return nullptr;

		return pItem->GetDesc();
	}
	ZItem* GetSelectedWeapon(); 
	MMatchCharItemParts GetSelectedWeaponParts() { return (MMatchCharItemParts)m_nSelectedWeapon; }

	MMatchCharItemParts GetSelectedWeaponType() {
		return m_nSelectedWeapon;
	}

	void Save(BulletInfo(&Bullets)[MMCIP_END]);
	void Load(const BulletInfo(&Bullets)[MMCIP_END]);
};

#endif