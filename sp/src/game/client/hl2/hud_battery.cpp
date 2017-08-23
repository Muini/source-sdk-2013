//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// battery.cpp
//
// implementation of CHudBattery class
//
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "hud_numericdisplay.h"
#include "iclientmode.h"
#include "vgui/ISurface.h"
 
using namespace vgui;
 
#include "vgui_controls/AnimationController.h"
#include "vgui/ILocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define INIT_BAT	-1

//-----------------------------------------------------------------------------
// Purpose: Displays suit power (armor) on hud
//-----------------------------------------------------------------------------
class CHudBattery : public CHudNumericDisplay, public CHudElement
{
	DECLARE_CLASS_SIMPLE( CHudBattery, CHudNumericDisplay );

public:
	CHudBattery( const char *pElementName );
	void Init( void );
	void Reset( void );
	void VidInit( void );
	void OnThink( void );
	void MsgFunc_Battery(bf_read &msg );
	bool ShouldDraw();
 
protected:
	virtual void Paint();

private:
	CPanelAnimationVar(Color, m_BatColor, "HullColor", "255 255 255 250");
	CPanelAnimationVar(int, m_iBatDisabledAlpha, "HullDisabledAlpha", "10");
	CPanelAnimationVarAliasType(float, m_flBarInsetX, "BarInsetX", "2", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarInsetY, "BarInsetY", "2", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarWidth, "BarWidth", "100", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarHeight, "BarHeight", "1", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarChunkWidth, "BarChunkWidth", "1", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarChunkGap, "BarChunkGap", "0", "proportional_float");
	int		m_iBat;	
	int		m_iNewBat;
};

DECLARE_HUDELEMENT( CHudBattery );
DECLARE_HUD_MESSAGE( CHudBattery, Battery );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudBattery::CHudBattery( const char *pElementName ) : BaseClass(NULL, "HudSuit"), CHudElement( pElementName )
{
	vgui:: Panel * pParent = g_pClientMode-> GetViewport ();
	SetParent (pParent);
	SetHiddenBits( HIDEHUD_HEALTH | HIDEHUD_NEEDSUIT | HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBattery::Init( void )
{
	HOOK_HUD_MESSAGE( CHudBattery, Battery);
	Reset();
	m_iBat		= INIT_BAT;
	m_iNewBat   = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBattery::Reset( void )
{
	SetLabelText(g_pVGuiLocalize->Find("#Valve_Hud_SUIT"));
	SetDisplayValue(m_iBat);
	m_iBat		= INIT_BAT;
	m_iNewBat   = 0;
	SetBgColor (Color (0,0,0,100));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBattery::VidInit( void )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudBattery::ShouldDraw( void )
{
	bool bNeedsDraw = ( m_iBat != m_iNewBat ) || ( GetAlpha() > 0 );

	return ( bNeedsDraw && CHudElement::ShouldDraw() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBattery::OnThink( void )
{
	if ( m_iBat == m_iNewBat )
		return;

	if ( !m_iNewBat )
	{
	 	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitPowerZero");
	}
	else if ( m_iNewBat < m_iBat )
	{
		// battery power has decreased, so play the damaged animation
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitDamageTaken");

		// play an extra animation if we're super low
		if ( m_iNewBat < 20 )
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitArmorLow");
		}
	}
	else
	{
		// battery power has increased (if we had no previous armor, or if we just loaded the game, don't use alert state)
		if ( m_iBat == INIT_BAT || m_iBat == 0 || m_iNewBat >= 20)
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitPowerIncreasedAbove20");
		}
		else
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitPowerIncreasedBelow20");
		}
	}

	m_iBat = m_iNewBat;

	SetDisplayValue(m_iBat);
	Paint();
}
void CHudBattery::Paint()
{
	// Get bar chunks
 
	int chunkCount = m_flBarWidth / (m_flBarChunkWidth + m_flBarChunkGap);
	int enabledChunks = (int)((float)chunkCount * (m_iBat / 100.0f) + 0.5f );
 
	// Draw the suit power bar
	surface()->DrawSetColor (m_BatColor);
 
	int xpos = m_flBarInsetX, ypos = m_flBarInsetY;
 
	for (int i = 0; i < enabledChunks; i++)
	{
		surface()->DrawFilledRect(xpos, ypos, xpos + m_flBarChunkWidth, ypos + m_flBarHeight);
		xpos += (m_flBarChunkWidth + m_flBarChunkGap);
	}
 
	// Draw the exhausted portion of the bar.
	surface()->DrawSetColor(Color(m_BatColor [0], m_BatColor [1], m_BatColor [2], m_iBatDisabledAlpha));
 
	for (int i = enabledChunks; i < chunkCount; i++)
	{
		surface()->DrawFilledRect(xpos, ypos, xpos + m_flBarChunkWidth, ypos + m_flBarHeight);
		xpos += (m_flBarChunkWidth + m_flBarChunkGap);
	}
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudBattery::MsgFunc_Battery( bf_read &msg )
{
	m_iNewBat = msg.ReadShort();
}
