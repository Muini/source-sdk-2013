//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef WEAPON_EPEE_H
#define WEAPON_EPEE_H

#include "basebludgeonweapon.h"

#if defined( _WIN32 )
#pragma once
#endif

#ifdef HL2MP
#error weapon_epee.h must not be included in hl2mp. The windows compiler will use the wrong class elsewhere if it is.
#endif

#define	EPEE_RANGE	75.0f
#define	EPEE_REFIRE	0.6f

//-----------------------------------------------------------------------------
// CWeaponEpee
//-----------------------------------------------------------------------------

class CWeaponEpee : public CBaseHLBludgeonWeapon
{
public:
	DECLARE_CLASS( CWeaponEpee, CBaseHLBludgeonWeapon );

	DECLARE_SERVERCLASS();
	DECLARE_ACTTABLE();

	CWeaponEpee();

	void	DelayedAttack( void );
	void	ItemPostFrame( void );

	float		GetRange( void )		{	return	EPEE_RANGE;	}
	float		GetFireRate( void )		{	return	EPEE_REFIRE;	}

	void		AddViewKick( float force );
	float		GetDamageForActivity( Activity hitActivity );

	virtual int WeaponMeleeAttack1Condition( float flDot, float flDist );

	// Animation event
	virtual void Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

private:
	// Animation event handlers
	void HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	bool  m_bDelayedAttack;
	float m_flDelayedAttackTime;
};

#endif // WEAPON_EPEE_H
