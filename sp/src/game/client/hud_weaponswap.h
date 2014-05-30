#include "hudelement.h"
#include <vgui_controls/Panel.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Displays hints across the center of the screen
//-----------------------------------------------------------------------------
class CHudWeaponSwap : public CHudElement, public Panel
{
    DECLARE_CLASS_SIMPLE( CHudWeaponSwap, Panel );
public:
    CHudWeaponSwap( const char *pElementName );

    void Init();
    void Reset();

    C_BaseCombatWeapon* GetWeaponInSlot( int iSlot, int iSlotPos );

    virtual bool ShouldDraw();

protected:
    virtual void OnThink();
    virtual void Paint();

protected:
    int m_nIcon;
};