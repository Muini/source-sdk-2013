#include "hud.h"
#include "cbase.h"
#include "hud_weaponswap.h"
#include "iclientmode.h"
#include "hud_macros.h"
#include "vgui_controls/controls.h"
#include "vgui/ISurface.h"

#include "c_baseplayer.h"
#include "vgui_controls/AnimationController.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

DECLARE_HUDELEMENT( CHudWeaponSwap );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudWeaponSwap::CHudWeaponSwap( const char *pElementName ) : BaseClass(NULL, "HudWeaponSwap"), CHudElement( pElementName )
{
    Panel *pParent = g_pClientMode->GetViewport();
    SetParent( pParent );

    SetVisible( false );
    SetAlpha( 255 );
    SetEnabled( true );

    // Create Texture for Looking around
    m_nIcon = surface()->CreateNewTextureID();
    surface()->DrawSetTextureFile( m_nIcon, "VGUI/hud/icon_weapon_swap" , true, true);

    SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponSwap::Init()
{
    g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SwapMessageHide" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponSwap::Reset()
{
    g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SwapMessageHide" );
}

//-----------------------------------------------------------------------------
// Purpose: Updates the label color each frame
//-----------------------------------------------------------------------------
void CHudWeaponSwap::OnThink()
{
    if ( ShouldDraw() )
    {
        SetVisible( true );
        g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SwapMessageShow" ); 
    }
    else
    {
        SetVisible( false );
        g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "SwapMessageHide" ); 
    }
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the panel should draw
//-----------------------------------------------------------------------------
bool CHudWeaponSwap::ShouldDraw()
{
    C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
    if ( !pPlayer )
        return false;
	
	C_BaseCombatWeapon *pBumpedWeapon = dynamic_cast<CBaseCombatWeapon *>(pPlayer->GetBumpedWeapon().Get());
	
    if ( !pBumpedWeapon )
    {
        return false;
    }
    else
    {
        return true;
    }
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns the weapon in the specified slot
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHudWeaponSwap::GetWeaponInSlot( int iSlot, int iSlotPos )
{
    C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
    if ( !player )
        return NULL;

    for ( int i = 0; i < MAX_WEAPONS; i++ )
    {
        C_BaseCombatWeapon *pWeapon = player->GetWeapon(i);

        if ( pWeapon == NULL )
            continue;

        if ( pWeapon->GetSlot() == iSlot && pWeapon->GetPosition() == iSlotPos )
            return pWeapon;
    }

    return NULL;
}

//-------------------------------------------------------------------------
// Purpose: draws the selection area
//-------------------------------------------------------------------------
void CHudWeaponSwap::Paint()
{
    // find and display our current selection
    C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
    if ( !pPlayer )
        return;
	
	C_BaseCombatWeapon *pBumpedWeapon = dynamic_cast<CBaseCombatWeapon *>(pPlayer->GetBumpedWeapon().Get());

    if ( !pBumpedWeapon )
        return;

    C_BaseCombatWeapon *pOwnedWeapon = GetWeaponInSlot( pBumpedWeapon->GetSlot(), pBumpedWeapon->GetPosition() );
    if ( !pOwnedWeapon )
        return;

    Color col = GetFgColor();

    pOwnedWeapon->GetSpriteInactive()->DrawSelf( 8, 8, col );

    SetPaintBorderEnabled(false);
    surface()->DrawSetTexture( m_nIcon );
    surface()->DrawTexturedRect( 100, 8, 50, 50 );

    pBumpedWeapon->GetSpriteInactive()->DrawSelf( 120, 8, col );
	
}