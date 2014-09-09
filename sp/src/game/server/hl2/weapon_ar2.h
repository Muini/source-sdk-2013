//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:		Projectile shot from the AR2 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef	WEAPONAR2_H
#define	WEAPONAR2_H

#include "basegrenade_shared.h"
#include "basehlcombatweapon.h"
#include "in_buttons.h"

class CWeaponAR2 : public CHLMachineGun
{
public:
	DECLARE_CLASS( CWeaponAR2, CHLMachineGun );

	CWeaponAR2();

	DECLARE_SERVERCLASS();

	void	ItemPostFrame( void );
	void	Precache( void );
	
	void	PrimaryAttack( void );
	void	SecondaryAttack( void );
	void	DelayedAttack( void );

	const char *GetTracerType( void ) { return "AR2Tracer"; }

	void	AddViewKick( void );

	void	FireNPCPrimaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	FireNPCSecondaryAttack( CBaseCombatCharacter *pOperator, bool bUseWeaponAngles );
	void	Operator_ForceNPCFire( CBaseCombatCharacter  *pOperator, bool bSecondary );
	void	Operator_HandleAnimEvent( animevent_t *pEvent, CBaseCombatCharacter *pOperator );

	int		GetMinBurst( void ) { return 1; }
	int		GetMaxBurst( void ) { return 12; }
	float	GetFireRate( void ) { return 0.074f; }

	bool	CanHolster( void );
	bool	Reload( void );

	int		CapabilitiesGet( void ) { return bits_CAP_WEAPON_RANGE_ATTACK1; }

	Activity	GetPrimaryAttackActivity( void );
	
	void	DoImpactEffect( trace_t &tr, int nDamageType );

	virtual const Vector& GetBulletSpread( void )
	{
		static Vector cone=VECTOR_CONE_3DEGREES; //NPC & Default

		CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
		if ( pPlayer == NULL )
			return cone;

		if (pPlayer->m_nButtons & IN_DUCK) {  cone = VECTOR_CONE_0DEGREES;} else { cone = VECTOR_CONE_1DEGREES;} //Duck & Stand
		if (pPlayer->m_nButtons & IN_FORWARD) { cone = VECTOR_CONE_2DEGREES;} //Move
		if (pPlayer->m_nButtons & IN_BACK) { cone = VECTOR_CONE_2DEGREES;} //Move
		if (pPlayer->m_nButtons & IN_MOVERIGHT) { cone = VECTOR_CONE_2DEGREES;} //Move
		if (pPlayer->m_nButtons & IN_MOVELEFT) { cone = VECTOR_CONE_2DEGREES;} //Move
		if (pPlayer->m_nButtons & IN_RUN) { cone = VECTOR_CONE_3DEGREES;} //Run
		if (pPlayer->m_nButtons & IN_SPEED) { cone = VECTOR_CONE_3DEGREES;} //Run
		if (pPlayer->m_nButtons & IN_JUMP) { cone = VECTOR_CONE_3DEGREES;} //Jump
		//Mourrant ? 1.5 fois moins précis !
		/*if (pPlayer->GetHealth()<25)
			cone = cone*1.5;*/
		//Plus tu tires, moins tu sais viser
		cone = cone*(1+(m_nShotsFired/10));
		return cone;
	}

	float GetSpeedMalus() { return 0.9f; }

	const WeaponProficiencyInfo_t *GetProficiencyValues();

protected:

	float					m_flDelayedFire;
	bool					m_bShotDelayed;
	int						m_nVentPose;
	
	DECLARE_ACTTABLE();
	DECLARE_DATADESC();
};


#endif	//WEAPONAR2_H
