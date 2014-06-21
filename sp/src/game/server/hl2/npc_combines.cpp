//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This is the soldier version of the combine, analogous to the HL1 grunt.
//
//=============================================================================//

#include "cbase.h"
#include "ai_hull.h"
#include "ai_motor.h"
#include "npc_combines.h"
#include "bitstring.h"
#include "engine/IEngineSound.h"
#include "soundent.h"
#include "ndebugoverlay.h"
#include "npcevent.h"
#include "hl2/hl2_player.h"
#include "game.h"
#include "ammodef.h"
#include "explode.h"
#include "ai_memory.h"
#include "Sprite.h"
#include "soundenvelope.h"
#include "weapon_physcannon.h"
#include "hl2_gamerules.h"
#include "gameweaponmanager.h"
#include "vehicle_base.h"
#include "particle_parse.h"
#include "particles/particles.h"
#include "gib.h"
#include "IEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_combine_s_health( "sk_combine_s_health","0");
ConVar	sk_combine_s_kick( "sk_combine_s_kick","0");

ConVar sk_combine_guard_health( "sk_combine_guard_health", "0");
ConVar sk_combine_guard_kick( "sk_combine_guard_kick", "0");
 
// Whether or not the combine guard should spawn health on death
ConVar combine_guard_spawn_health( "combine_guard_spawn_health", "1" );

extern ConVar sk_plr_dmg_buckshot;	
extern ConVar sk_plr_num_shotgun_pellets;

//Whether or not the combine should spawn health on death
ConVar	combine_spawn_health( "combine_spawn_health", "1" );

LINK_ENTITY_TO_CLASS( npc_combine_s, CNPC_CombineS );


#define AE_SOLDIER_BLOCK_PHYSICS		20 // trying to block an incoming physics object

extern Activity ACT_WALK_EASY;
extern Activity ACT_WALK_MARCH;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CombineS::Spawn( void )
{
	if (random->RandomInt(0,20)==0)
		m_fIsInvisible = true;

	Precache();

	SetModel( STRING( GetModelName() ) );

	if( IsElite() )
	{
		// Stronger, tougher.
		SetHealth( sk_combine_guard_health.GetFloat() );
		SetMaxHealth( sk_combine_guard_health.GetFloat() );
		SetKickDamage( sk_combine_guard_kick.GetFloat() );
	}
	else
	{
		SetHealth( sk_combine_s_health.GetFloat() );
		SetMaxHealth( sk_combine_s_health.GetFloat() );
		SetKickDamage( sk_combine_s_kick.GetFloat() );
	}

	if( IsInvisible() )
	{
		SetRenderMode(kRenderTransAdd);
		SetRenderColor(200,200,255,100);
	}

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );

	BaseClass::Spawn();

#if HL2_EPISODIC
	if (m_iUseMarch && !HasSpawnFlags(SF_NPC_START_EFFICIENT))
	{
		Msg( "Soldier %s is set to use march anim, but is not an efficient AI. The blended march anim can only be used for dead-ahead walks!\n", GetDebugName() );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CNPC_CombineS::Precache()
{
	const char *pModelName = STRING( GetModelName() );

	//A chance to be an elite guy ?
	if(random->RandomInt(0,30)!=0)
	{
		if( !Q_stricmp( pModelName, "models/combine_super_soldier.mdl" ) )
		{
			m_fIsElite = true;
		}
		else
		{
			m_fIsElite = false;
		}
	}else{
		m_fIsElite = true;
		SetModelName( MAKE_STRING( "models/combine_super_soldier.mdl" ) );
	}

	if(!IsElite())
	{
	//if( !GetModelName() )
	//{
		//SetModelName( MAKE_STRING( "models/combine_soldier.mdl" ) );
		static const char* modelnames[] = {
		"models/seal_01.mdl",
		"models/seal_02.mdl",
		"models/seal_03.mdl",
		"models/seal_04.mdl",
		"models/seal_05.mdl",
		"models/seal_06.mdl",
		"models/seal_01s.mdl",
		"models/seal_02s.mdl",
		"models/seal_03s.mdl",
		"models/seal_04s.mdl",
		"models/seal_05s.mdl",
		"models/seal_06s.mdl",
		};
		SetModelName ( MAKE_STRING( modelnames[ random->RandomInt( 0, ARRAYSIZE(modelnames) - 1 ) ]) );
	//}
	}else{
		static const char* modelnames[] = {
		"models/combine_super_soldier.mdl",
		};
		SetModelName ( MAKE_STRING( modelnames[ random->RandomInt( 0, ARRAYSIZE(modelnames) - 1 ) ]) );
	}


	PrecacheModel( STRING( GetModelName() ) );

	UTIL_PrecacheOther( "item_healthvial" );
	UTIL_PrecacheOther( "weapon_frag" );
	UTIL_PrecacheOther( "item_ammo_ar2_altfire" );
	UTIL_PrecacheOther( "weapon_pistol" );
	UTIL_PrecacheOther( "item_ammo_pistol" );

	PrecacheParticleSystem( "blood_impact_red_dust" );

	BaseClass::Precache();
}


void CNPC_CombineS::DeathSound( const CTakeDamageInfo &info )
{
	// NOTE: The response system deals with this at the moment
	if ( GetFlags() & FL_DISSOLVING )
		return;

	GetSentences()->Speak( "COMBINE_DIE", SENTENCE_PRIORITY_INVALID, SENTENCE_CRITERIA_ALWAYS ); 
}


//-----------------------------------------------------------------------------
// Purpose: Soldiers use CAN_RANGE_ATTACK2 to indicate whether they can throw
//			a grenade. Because they check only every half-second or so, this
//			condition must persist until it is updated again by the code
//			that determines whether a grenade can be thrown, so prevent the 
//			base class from clearing it out. (sjb)
//-----------------------------------------------------------------------------
void CNPC_CombineS::ClearAttackConditions( )
{
	bool fCanRangeAttack2 = HasCondition( COND_CAN_RANGE_ATTACK2 );

	// Call the base class.
	BaseClass::ClearAttackConditions();

	if( fCanRangeAttack2 )
	{
		// We don't allow the base class to clear this condition because we
		// don't sense for it every frame.
		SetCondition( COND_CAN_RANGE_ATTACK2 );
	}
}

void CNPC_CombineS::PrescheduleThink( void )
{
	/*//FIXME: This doesn't need to be in here, it's all debug info
	if( HasCondition( COND_HEAR_PHYSICS_DANGER ) )
	{
		// Don't react unless we see the item!!
		CSound *pSound = NULL;

		pSound = GetLoudestSoundOfType( SOUND_PHYSICS_DANGER );

		if( pSound )
		{
			if( FInViewCone( pSound->GetSoundReactOrigin() ) )
			{
				DevMsg( "OH CRAP!\n" );
				NDebugOverlay::Line( EyePosition(), pSound->GetSoundReactOrigin(), 0, 0, 255, false, 2.0f );
			}
		}
	}
	*/

	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
// Purpose: Allows for modification of the interrupt mask for the current schedule.
//			In the most cases the base implementation should be called first.
//-----------------------------------------------------------------------------
void CNPC_CombineS::BuildScheduleTestBits( void )
{
	//Interrupt any schedule with physics danger (as long as I'm not moving or already trying to block)
	if ( m_flGroundSpeed == 0.0 && !IsCurSchedule( SCHED_FLINCH_PHYSICS ) )
	{
		SetCustomInterruptCondition( COND_HEAR_PHYSICS_DANGER );
	}

	BaseClass::BuildScheduleTestBits();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output :
//-----------------------------------------------------------------------------
int CNPC_CombineS::SelectSchedule ( void )
{
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
float CNPC_CombineS::GetHitgroupDamageMultiplier( int iHitGroup, const CTakeDamageInfo &info )
{
	switch( iHitGroup )
	{
	case HITGROUP_HEAD:
		{
			// Headshot Effects
			if( info.GetDamageType() == DMG_CLUB || info.GetDamageType() == DMG_SLASH )
			{
				if(random->RandomInt(0,6)==0)
					EmitSound( "NPC.BloodSpray" );
			} 
			else if( info.GetDamageType() == DMG_BULLET || info.GetDamageType() == DMG_BUCKSHOT ) 
			{
				DispatchParticleEffect( "combines_headshot_blood",  info.GetDamagePosition() + RandomVector( -2.0f, 2.0f ), RandomAngle( 0, 360 ) );
				if(IsElite())
				{
					g_pEffects->Sparks( info.GetDamagePosition(), 1, 2 );
					UTIL_Smoke( info.GetDamagePosition(), random->RandomInt( 10, 15 ), 10 );
				}else{
					if(random->RandomInt(0,6)==0)
						EmitSound( "NPC.BloodSpray" );
				}
			}
			break;
		}
	case HITGROUP_STOMACH:
	case HITGROUP_CHEST:
		{
			if(!IsElite())
			{
				//Kevlar
				DispatchParticleEffect( "blood_impact_red_dust",  info.GetDamagePosition() + RandomVector( -1.0f, 1.0f ), RandomAngle( 0, 360 ) );
				UTIL_Smoke( info.GetDamagePosition(), random->RandomInt( 10, 15 ), 10 );
			}
			break;
		}
	}

	if( info.GetDamageType() == DMG_CLUB || info.GetDamageType() == DMG_SLASH )
	{
		CGib::SpawnStickyGibs( this, info.GetDamagePosition(), random->RandomInt(0,2) );
	}
	else if( info.GetDamageType() == DMG_BULLET ||
		info.GetDamageType() == DMG_BUCKSHOT ||
		info.GetDamageType() == DMG_BLAST ) 
	{
		if(IsInvisible())
		{
			//g_pEffects->Sparks( info.GetDamagePosition(), 1, 2 );
			UTIL_Smoke( info.GetDamagePosition(), random->RandomInt( 10, 15 ), 10 );
			DispatchParticleEffect( "shield_impact",  info.GetDamagePosition() + RandomVector( -2.0f, 2.0f ), RandomAngle( 0, 360 ) );
			EmitSound( "NPC.ShieldHit" );
			//g_pEffects->Ricochet( info.GetDamagePosition(), info.GetDamagePosition()+ RandomVector( -4.0f, 4.0f ) );
			return 0.8f;
		}
		else if(IsElite())
		{
			UTIL_Smoke( info.GetDamagePosition(), random->RandomInt( 10, 15 ), 10 );
			DispatchParticleEffect( "shield_impact",  info.GetDamagePosition() + RandomVector( -2.0f, 2.0f ), RandomAngle( 0, 360 ) );
			EmitSound( "NPC.ShieldHit" );
			//g_pEffects->Ricochet( info.GetDamagePosition(), info.GetDamagePosition() + RandomVector( -4.0f, 4.0f ) );
			return 0.4f;
		}
	}

	return BaseClass::GetHitgroupDamageMultiplier( iHitGroup, info );
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_CombineS::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	case AE_SOLDIER_BLOCK_PHYSICS:
		DevMsg( "BLOCKING!\n" );
		m_fIsBlocking = true;
		break;

	default:
		BaseClass::HandleAnimEvent( pEvent );
		break;
	}
}

void CNPC_CombineS::OnChangeActivity( Activity eNewActivity )
{
	// Any new sequence stops us blocking.
	m_fIsBlocking = false;

	BaseClass::OnChangeActivity( eNewActivity );

#if HL2_EPISODIC
	// Give each trooper a varied look for his march. Done here because if you do it earlier (eg Spawn, StartTask), the
	// pose param gets overwritten.
	if (m_iUseMarch)
	{
		SetPoseParameter("casual", RandomFloat());
	}
#endif
}

void CNPC_CombineS::OnListened()
{
	BaseClass::OnListened();

	if ( HasCondition( COND_HEAR_DANGER ) && HasCondition( COND_HEAR_PHYSICS_DANGER ) )
	{
		if ( HasInterruptCondition( COND_HEAR_DANGER ) )
		{
			ClearCondition( COND_HEAR_PHYSICS_DANGER );
		}
	}

	// debugging to find missed schedules
#if 0
	if ( HasCondition( COND_HEAR_DANGER ) && !HasInterruptCondition( COND_HEAR_DANGER ) )
	{
		DevMsg("Ignore danger in %s\n", GetCurSchedule()->GetName() );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CNPC_CombineS::Event_Killed( const CTakeDamageInfo &info )
{
	// Don't bother if we've been told not to, or the player has a megaphyscannon
	if ( combine_spawn_health.GetBool() == false || PlayerHasMegaPhysCannon() )
	{
		BaseClass::Event_Killed( info );
		return;
	}

	CBasePlayer *pPlayer = ToBasePlayer( info.GetAttacker() );

	if ( !pPlayer )
	{
		CPropVehicleDriveable *pVehicle = dynamic_cast<CPropVehicleDriveable *>( info.GetAttacker() ) ;
		if ( pVehicle && pVehicle->GetDriver() && pVehicle->GetDriver()->IsPlayer() )
		{
			pPlayer = assert_cast<CBasePlayer *>( pVehicle->GetDriver() );
		}
	}

	if ( pPlayer != NULL )
	{
		/*
		// Elites drop alt-fire ammo, so long as they weren't killed by dissolving.
		if( IsElite() )
		{
#ifdef HL2_EPISODIC
			if ( HasSpawnFlags( SF_COMBINE_NO_AR2DROP ) == false )
#endif
			{
				CBaseEntity *pItem = DropItem( "item_ammo_ar2_altfire", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );

				if ( pItem )
				{
					IPhysicsObject *pObj = pItem->VPhysicsGetObject();

					if ( pObj )
					{
						Vector			vel		= RandomVector( -64.0f, 64.0f );
						AngularImpulse	angImp	= RandomAngularImpulse( -300.0f, 300.0f );

						vel[2] = 0.0f;
						pObj->AddVelocity( &vel, &angImp );
					}

					if( info.GetDamageType() & DMG_DISSOLVE )
					{
						CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating*>(pItem);

						if( pAnimating )
						{
							pAnimating->Dissolve( NULL, gpGlobals->curtime, false, ENTITY_DISSOLVE_NORMAL );
						}
					}
					else
					{
						WeaponManager_AddManaged( pItem );
					}
				}
			}
		}
		*/
		CHalfLife2 *pHL2GameRules = static_cast<CHalfLife2 *>(g_pGameRules);

		// Attempt to drop health
		if ( pHL2GameRules->NPC_ShouldDropHealth( pPlayer ) )
		{
			if(random->RandomInt(0,2)==0)
			{
				DropItem( "item_healthvial", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
				pHL2GameRules->NPC_DroppedHealth();
			}
		}
		
		if ( HasSpawnFlags( SF_COMBINE_NO_GRENADEDROP ) == false )
		{
			// Attempt to drop a grenade
			if ( pHL2GameRules->NPC_ShouldDropGrenade( pPlayer ) )
			{
				DropItem( "weapon_frag", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
				pHL2GameRules->NPC_DroppedGrenade();
			}
		}
		if(random->RandomInt(0,5)==0)
			DropItem( "item_ammo_pistol", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		if(random->RandomInt(0,8)==0)
			DropItem( "weapon_pistol", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		if(random->RandomInt(0,10)==0)
			DropItem( "item_healthvial", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		if(random->RandomInt(0,20)==0)
			DropItem( "item_ammo_smg1", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		if(random->RandomInt(0,30)==0)
			DropItem( "item_healthkit", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		if(random->RandomInt(0,60)==0)
			DropItem( "item_battery", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		if(random->RandomInt(0,100)==0)
			DropItem( "weapon_frag", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		if(random->RandomInt(0,1000)==0)
			DropItem( "weapon_357", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
	}
	//Explosion
	if( !IsElite() )
	{
		if( m_iHealth <= -90 )
		{
			if( info.GetDamageType() & ( DMG_BLAST | DMG_VEHICLE | DMG_FALL | DMG_CRUSH ) )
			{
				EmitSound( "NPC.ExplodeGore" );
			
				DispatchParticleEffect( "Humah_Explode_blood", WorldSpaceCenter(), GetAbsAngles() );
			
				SetModel( "models/humans/charple03.mdl" );

				//CreateRagGib( "models/zombie/zombie1_legs.mdl", WorldSpaceCenter(), GetAbsAngles(), 500);
				CGib::SpawnSpecificGibs( this, 1, 100, 600, "models/gibs/hgibs_jaw.mdl", 5 );
				CGib::SpawnSpecificGibs( this, 1, 100, 600, "models/gibs/leg.mdl", 5 );
				CGib::SpawnSpecificGibs( this, 1, 100, 600, "models/gibs/hgibs_rib.mdl", 5 );
				CGib::SpawnSpecificGibs( this, 1, 100, 600, "models/gibs/hgibs_scapula.mdl", 5 );
				CGib::SpawnSpecificGibs( this, 1, 100, 600, "models/gibs/hgibs_spine.mdl", 5 );

				CGib::SpawnStickyGibs( this, WorldSpaceCenter(), random->RandomInt(10,20) );
				
				//BLOOOOOOD !!!!
				trace_t tr;
				Vector randVector;
				//Create 128 random decals that are within +/- 256 units.
				for ( int i = 0 ; i < 64; i++ )
				{
					randVector.x = random->RandomFloat( -256.0f, 256.0f );
					randVector.y = random->RandomFloat( -256.0f, 256.0f );
					randVector.z = random->RandomFloat( -256.0f, 256.0f );

					AI_TraceLine( WorldSpaceCenter()+Vector(0,0,1), WorldSpaceCenter()-randVector, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );			 

					UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_RED );
				}
			}
		}
	}
	
	//Shield Down
	if( IsElite() || IsInvisible() )
		EmitSound( "NPC.ShieldDown" );

	if( info.GetDamageType() & ( DMG_SLASH | DMG_CRUSH | DMG_CLUB ) )
		CGib::SpawnStickyGibs( this, WorldSpaceCenter(), random->RandomInt(0,3) );

	BaseClass::Event_Killed( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CombineS::IsLightDamage( const CTakeDamageInfo &info )
{
	return BaseClass::IsLightDamage( info );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &info - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CNPC_CombineS::IsHeavyDamage( const CTakeDamageInfo &info )
{
	// Combine considers AR2 fire to be heavy damage
	//if ( info.GetAmmoType() == GetAmmoDef()->Index("AR2") )
	//	return true;

	// 357 rounds are heavy damage
	//if ( info.GetAmmoType() == GetAmmoDef()->Index("357") )
	//	return true;

	// Shotgun blasts where at least half the pellets hit me are heavy damage
	if ( info.GetDamageType() & DMG_BUCKSHOT )
	{
		int iHalfMax = sk_plr_dmg_buckshot.GetFloat() * sk_plr_num_shotgun_pellets.GetInt() * 0.5;
		if ( info.GetDamage() >= iHalfMax )
			return true;
	}

	// Rollermine shocks
	if( (info.GetDamageType() & DMG_SHOCK) && hl2_episodic.GetBool() )
	{
		return true;
	}

	return BaseClass::IsHeavyDamage( info );
}

#if HL2_EPISODIC
//-----------------------------------------------------------------------------
// Purpose: Translate base class activities into combot activites
//-----------------------------------------------------------------------------
Activity CNPC_CombineS::NPC_TranslateActivity( Activity eNewActivity )
{
	// If the special ep2_outland_05 "use march" flag is set, use the more casual marching anim.
	if ( m_iUseMarch && eNewActivity == ACT_WALK )
	{
		eNewActivity = ACT_WALK_MARCH;
	}

	return BaseClass::NPC_TranslateActivity( eNewActivity );
}


//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CNPC_CombineS )

	DEFINE_KEYFIELD( m_iUseMarch, FIELD_INTEGER, "usemarch" ),

END_DATADESC()
#endif