//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Epee - an old favorite
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon.h"
#include "player.h"
#include "gamerules.h"
#include "ammodef.h"
#include "mathlib/mathlib.h"
#include "in_buttons.h"
#include "soundent.h"
#include "basebludgeonweapon.h"
#include "vstdlib/random.h"
#include "npcevent.h"
#include "ai_basenpc.h"
#include "weapon_epee.h"
#include "basecombatcharacter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar    sk_plr_dmg_epee		( "sk_plr_dmg_epee","0");
ConVar    sk_npc_dmg_epee		( "sk_npc_dmg_epee","0");

//-----------------------------------------------------------------------------
// CWeaponEpee
//-----------------------------------------------------------------------------

IMPLEMENT_SERVERCLASS_ST(CWeaponEpee, DT_WeaponEpee)
END_SEND_TABLE()

#ifndef HL2MP
LINK_ENTITY_TO_CLASS( weapon_epee, CWeaponEpee );
PRECACHE_WEAPON_REGISTER( weapon_epee );
#endif

acttable_t CWeaponEpee::m_acttable[] = 
{
	{ ACT_MELEE_ATTACK1,	ACT_MELEE_ATTACK_SWING, true },
	{ ACT_IDLE,				ACT_IDLE_ANGRY_MELEE,	false },
	{ ACT_IDLE_ANGRY,		ACT_IDLE_ANGRY_MELEE,	false },
};

IMPLEMENT_ACTTABLE(CWeaponEpee);

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CWeaponEpee::CWeaponEpee( void )
{
		m_bDelayedAttack = false;
		m_flDelayedAttackTime = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Get the damage amount for the animation we're doing
// Input  : hitActivity - currently played activity
// Output : Damage amount
//-----------------------------------------------------------------------------
float CWeaponEpee::GetDamageForActivity( Activity hitActivity )
{
	if ( ( GetOwner() != NULL ) && ( GetOwner()->IsPlayer() ) )
			return sk_plr_dmg_epee.GetFloat();

	return sk_npc_dmg_epee.GetFloat();
}
//-----------------------------------------------------------------------------
// Purpose: Delayed Attack !
//-----------------------------------------------------------------------------
void CWeaponEpee::ItemPostFrame( void )
{
		CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
		
		if ( pOwner == NULL )
			return;

		if ( (pOwner->m_nButtons & IN_ATTACK) && ( (m_flNextPrimaryAttack - 0.2f) <= gpGlobals->curtime ) && !m_bDelayedAttack)
		{
			BaseClass::PrimaryAttack();
			AddViewKick( -1.0f );
		}
		else if ( (pOwner->m_nButtons & IN_ATTACK2)  && (m_flNextPrimaryAttack <= gpGlobals->curtime) && !m_bDelayedAttack)
		{
			//Animation Comments
			SendWeaponAnim( ACT_VM_HAULBACK );
			AddViewKick( -2.0f );
			m_flDelayedAttackTime = gpGlobals->curtime + SequenceDuration();
			//m_flDelayedAttackTime = gpGlobals->curtime + 0.5f;
			m_bDelayedAttack = true;
		}
		DelayedAttack();

		BaseClass::ItemPostFrame();
}
void CWeaponEpee::DelayedAttack( void )
{
         if (m_bDelayedAttack && gpGlobals->curtime > m_flDelayedAttackTime)
         {
			//Replace this Comment with Weapon Attack Function
			BaseClass::SecondaryAttack();
			AddViewKick( 5.0f );
			m_bDelayedAttack = false;
         }
}
//-----------------------------------------------------------------------------
// Purpose: Add in a view kick for this weapon
//-----------------------------------------------------------------------------
void CWeaponEpee::AddViewKick( float force )
{
	CBasePlayer *pPlayer  = ToBasePlayer( GetOwner() );
	
	if ( pPlayer == NULL )
		return;

	QAngle punchAng;

	punchAng.x = random->RandomFloat( 0.1f, 0.5f )*force;
	punchAng.y = random->RandomFloat( 1.0f, 3.0f )*force;
	punchAng.z = 0.0f;
	
	pPlayer->ViewPunch( punchAng ); 
}


//-----------------------------------------------------------------------------
// Attempt to lead the target (needed because citizens can't hit manhacks with the epee!)
//-----------------------------------------------------------------------------
ConVar sk_epee_lead_time( "sk_epee_lead_time", "0.8" );

int CWeaponEpee::WeaponMeleeAttack1Condition( float flDot, float flDist )
{
	// Attempt to lead the target (needed because citizens can't hit manhacks with the epee!)
	CAI_BaseNPC *pNPC	= GetOwner()->MyNPCPointer();
	CBaseEntity *pEnemy = pNPC->GetEnemy();
	if (!pEnemy)
		return COND_NONE;

	Vector vecVelocity;
	vecVelocity = pEnemy->GetSmoothedVelocity( );

	// Project where the enemy will be in a little while
	float dt = sk_epee_lead_time.GetFloat();
	dt += random->RandomFloat( -0.3f, 0.2f );
	if ( dt < 0.0f )
		dt = 0.0f;

	Vector vecExtrapolatedPos;
	VectorMA( pEnemy->WorldSpaceCenter(), dt, vecVelocity, vecExtrapolatedPos );

	Vector vecDelta;
	VectorSubtract( vecExtrapolatedPos, pNPC->WorldSpaceCenter(), vecDelta );

	if ( fabs( vecDelta.z ) > EPEE_RANGE )
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	Vector vecForward = pNPC->BodyDirection2D( );
	vecDelta.z = 0.0f;
	float flExtrapolatedDist = Vector2DNormalize( vecDelta.AsVector2D() );
	if ((flDist > EPEE_RANGE) && (flExtrapolatedDist > EPEE_RANGE))
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	float flExtrapolatedDot = DotProduct2D( vecDelta.AsVector2D(), vecForward.AsVector2D() );
	if ((flDot < 0.7) && (flExtrapolatedDot < 0.7))
	{
		return COND_NOT_FACING_ATTACK;
	}

	return COND_CAN_MELEE_ATTACK1;
}


//-----------------------------------------------------------------------------
// Animation event handlers
//-----------------------------------------------------------------------------
void CWeaponEpee::HandleAnimEventMeleeHit( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	// Trace up or down based on where the enemy is...
	// But only if we're basically facing that direction
	Vector vecDirection;
	AngleVectors( GetAbsAngles(), &vecDirection );

	CBaseEntity *pEnemy = pOperator->MyNPCPointer() ? pOperator->MyNPCPointer()->GetEnemy() : NULL;
	if ( pEnemy )
	{
		Vector vecDelta;
		VectorSubtract( pEnemy->WorldSpaceCenter(), pOperator->Weapon_ShootPosition(), vecDelta );
		VectorNormalize( vecDelta );
		
		Vector2D vecDelta2D = vecDelta.AsVector2D();
		Vector2DNormalize( vecDelta2D );
		if ( DotProduct2D( vecDelta2D, vecDirection.AsVector2D() ) > 0.8f )
		{
			vecDirection = vecDelta;
		}
	}

	for( int i=0;i<3;i++ )
	{
		Vector vecEnd;
		VectorMA( pOperator->Weapon_ShootPosition(), EPEE_RANGE, vecDirection, vecEnd );
		CBaseEntity *pHurt = pOperator->CheckTraceHullAttack( pOperator->Weapon_ShootPosition(), vecEnd, 
			Vector((-16-i*3),(-16-i*3),(-16-i*3)), Vector((36+i*3),(36+i*3),(36+i*3)), sk_npc_dmg_epee.GetFloat(), DMG_CLUB, 0.75 );
		
		// did I hit someone?
		if ( pHurt )
		{
			// play sound
			WeaponSound( MELEE_HIT );
			// Fake a trace impact, so the effects work out like a player's crowbaw
			trace_t traceHit;
			UTIL_TraceLine( pOperator->Weapon_ShootPosition(), pHurt->GetAbsOrigin(), MASK_SHOT_HULL, pOperator, COLLISION_GROUP_NONE, &traceHit );
			ImpactEffect( traceHit );
		}
		else
		{
			WeaponSound( MELEE_MISS );
		}
	}

	AddViewKick( 1.0f );
}


//-----------------------------------------------------------------------------
// Animation event
//-----------------------------------------------------------------------------
void CWeaponEpee::Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator )
{
	switch( pEvent->event )
	{
	case EVENT_WEAPON_MELEE_HIT:
		HandleAnimEventMeleeHit( pEvent, pOperator );
		break;

	default:
		BaseClass::Operator_HandleAnimEvent( pEvent, pOperator );
		break;
	}
}
