//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The various ammo types for HL2	
//
//=============================================================================//

#include "cbase.h"
#include "props.h"
#include "items.h"
#include "npc_combines.h"
#include "soundent.h"
#include "particle_parse.h"
#include "particles/particles.h"
#include "gib.h"
#include "IEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	acsmod_armor_health( "acsmod_armor_health","30");

/* ============================== */
/* Armor Base Class */
/* ============================== */
class CItem_Armor : public CItem
{
public:
	DECLARE_CLASS( CItem_Armor, CItem );

	void Spawn( void )
	{ 
		Precache( );
		SetSolid( SOLID_VPHYSICS );
		m_takedamage = DAMAGE_YES;
	}
	void Precache( void )
	{
		//PrecacheScriptSound( "Bounce.Metal" );
		PrecacheScriptSound( "Breakable.MatMetal" );
	}
	void Event_Killed( const CTakeDamageInfo &info )
	{
		AddSolidFlags( FSOLID_NOT_SOLID );
		m_takedamage = DAMAGE_NO;
		SetSolid( SOLID_NONE );

		EmitSound( "Breakable.MatMetal" );

		//g_pEffects->Sparks( info.GetDamagePosition(), 1, 1 );
		
		BaseClass::Event_Killed( info );
	}
	int OnTakeDamage( const CTakeDamageInfo &info )
	{
		if ( info.GetDamageType() & (DMG_CLUB | DMG_CRUSH | DMG_SLASH | DMG_AIRBOAT) )
		{
			CTakeDamageInfo dmgInfo = info;
			dmgInfo.ScaleDamage( 10.0 );
			return BaseClass::OnTakeDamage( dmgInfo );
		}

		//EmitSound( "Bounce.Metal" );
		g_pEffects->Ricochet( info.GetDamagePosition(), info.GetDamagePosition()+ RandomVector( -4.0f, 4.0f ) );
		g_pEffects->Sparks( info.GetDamagePosition(), 1, 1 );
		
		return BaseClass::OnTakeDamage( info );
	}
	
};

/* ============================== */
/* Armor Helmet */
/* ============================== */
class CItem_ArmorHelmet : public CItem_Armor
{
public:
	DECLARE_CLASS( CItem_ArmorHelmet, CItem );

	void Spawn( void )
	{ 
		Precache( );

		CItem_Armor::Spawn();

		SetModel( "models/misc/faceshield.mdl" );

		CAI_BaseNPC *pOwner = ( GetOwnerEntity() ) ? GetOwnerEntity()->MyNPCPointer() : NULL;

		if ( pOwner )
		{
			int attachment = pOwner->LookupAttachment( "eyes" );
			if ( attachment )
			{
				SetAbsAngles( GetOwnerEntity()->GetAbsAngles() );
				SetParent( GetOwnerEntity(), attachment );

				Vector vecPosition;
				vecPosition.Init( -1, 0, 0 );
				SetLocalOrigin( vecPosition );
			}
		}

		SetHealth( acsmod_armor_health.GetFloat() * 0.5 );
	}
	void Precache( void )
	{
		PrecacheModel ("models/misc/faceshield.mdl");
	}
	void Event_Killed( const CTakeDamageInfo &info )
	{
		CGib::SpawnSpecificGibs( this, 1, 50, 200, "models/misc/faceshield.mdl", 120 );
		
		CItem_Armor::Event_Killed( info );
	}
	int OnTakeDamage( const CTakeDamageInfo &info )
	{	
		return CItem_Armor::OnTakeDamage( info );
	}
	
};
LINK_ENTITY_TO_CLASS(item_armor_helmet, CItem_ArmorHelmet);

/* ============================== */
/* Armor Shield */
/* ============================== */
class CItem_ArmorShield : public CItem_Armor
{
public:
	DECLARE_CLASS( CItem_ArmorShield, CItem );

	void Spawn( void )
	{ 
		Precache( );

		CItem_Armor::Spawn();

		CAI_BaseNPC *pOwner = ( GetOwnerEntity() ) ? GetOwnerEntity()->MyNPCPointer() : NULL;

		if ( pOwner )
		{
			int attachment = pOwner->LookupAttachment( "anim_attachment_RH" );
			if ( attachment )
			{
				SetAbsAngles( GetOwnerEntity()->GetAbsAngles() + QAngle(0,-90,90) );
				SetParent( GetOwnerEntity(), attachment );

				Vector vecPosition;
				vecPosition.Init( 5, 15, -6 );
				SetLocalOrigin( vecPosition );
			}
		}

		SetModel( "models/misc/shield.mdl" );

		SetHealth( acsmod_armor_health.GetFloat() * 10 );
	}
	void Precache( void )
	{
		PrecacheModel ("models/misc/shield.mdl");
	}
	void Event_Killed( const CTakeDamageInfo &info )
	{
		CGib::SpawnSpecificGibs( this, 1, 50, 200, "models/misc/shield.mdl", 160 );
		
		CItem_Armor::Event_Killed( info );
	}
	int OnTakeDamage( const CTakeDamageInfo &info )
	{
		return CItem_Armor::OnTakeDamage( info );
	}
	
};
LINK_ENTITY_TO_CLASS(item_armor_shield, CItem_ArmorShield);

/* ============================== */
/* Armor Small Shield */
/* ============================== */
class CItem_ArmorSmallShield : public CItem_Armor
{
public:
	DECLARE_CLASS( CItem_ArmorSmallShield, CItem );

	void Spawn( void )
	{ 
		Precache( );

		CItem_Armor::Spawn();

		CAI_BaseNPC *pOwner = ( GetOwnerEntity() ) ? GetOwnerEntity()->MyNPCPointer() : NULL;

		if ( pOwner )
		{
			int attachment = pOwner->LookupAttachment( "anim_attachment_RH" );
			if ( attachment )
			{
				if( FClassnameIs( pOwner, "npc_metropolice" ) )
					SetAbsAngles( GetOwnerEntity()->GetAbsAngles() + QAngle(90,-90,90) );
				else if ( FClassnameIs( pOwner, "npc_combine_s" ) )
					SetAbsAngles( GetOwnerEntity()->GetAbsAngles() + QAngle(65,-90,90) );
				else
					SetAbsAngles( GetOwnerEntity()->GetAbsAngles() + QAngle(45,-90,90) );

				SetParent( GetOwnerEntity(), attachment );

				Vector vecPosition;
				vecPosition.Init( 0, 0, 0 );
				SetLocalOrigin( vecPosition );
			}
		}

		SetModel( "models/misc/armshield.mdl" );

		SetHealth( acsmod_armor_health.GetFloat() );
	}
	void Precache( void )
	{
		PrecacheModel ("models/misc/armshield.mdl");
	}
	void Event_Killed( const CTakeDamageInfo &info )
	{
		CGib::SpawnSpecificGibs( this, 1, 50, 200, "models/misc/armshield.mdl", 140 );
		
		CItem_Armor::Event_Killed( info );
	}
	int OnTakeDamage( const CTakeDamageInfo &info )
	{
		return CItem_Armor::OnTakeDamage( info );
	}
	
};
LINK_ENTITY_TO_CLASS(item_armor_smallshield, CItem_ArmorSmallShield);