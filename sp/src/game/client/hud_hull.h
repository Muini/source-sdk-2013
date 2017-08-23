#if !defined HUD_HULL_H
#define HUD_HULL_H 
 
#ifdef _WIN32
#pragma once
#endif
 
#include "hudelement.h"
#include "hud_numericdisplay.h"
 
//-----------------------------------------------------------------------------
// Purpose: Shows the hull bar
//-----------------------------------------------------------------------------
 
class CHudHull : public CHudElement, public vgui::Panel
{
 
	DECLARE_CLASS_SIMPLE(CHudHull, vgui::Panel);
 
public:
	CHudHull(const char * pElementName);
 
	virtual void Init (void);
	virtual void Reset (void);
	virtual void OnThink (void);
 
protected:
	virtual void Paint();
 
private:
	CPanelAnimationVar(Color, m_HullColor, "HullColor", "255 255 255 250");
	CPanelAnimationVar(int, m_iHullDisabledAlpha, "HullDisabledAlpha", "10");
	CPanelAnimationVarAliasType(float, m_flBarInsetX, "BarInsetX", "2", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarInsetY, "BarInsetY", "2", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarWidth, "BarWidth", "100", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarHeight, "BarHeight", "3", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarChunkWidth, "BarChunkWidth", "1", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarChunkGap, "BarChunkGap", "0", "proportional_float");
	float m_flHull;
	int m_nHullLow;
 
};
 
#endif // HUD_SUITPOWER_H