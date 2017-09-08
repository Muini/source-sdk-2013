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

ConVar acsmod_combine_shield_health( "acsmod_combine_shield_health", "100");

extern ConVar nag;
 
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

	CapabilitiesAdd( bits_CAP_ANIMATEDFACE );
	CapabilitiesAdd( bits_CAP_MOVE_SHOOT );
	CapabilitiesAdd( bits_CAP_DOORS_GROUP );

	int chanceModifier = 0;
	if (g_pGameRules->IsSkillLevel(SKILL_HARD))
		chanceModifier -= 5;
	if (g_pGameRules->IsSkillLevel(SKILL_EASY))
		chanceModifier += 5;

	//Invisible
	if(!nag.GetBool())
	{
		if (random->RandomInt(0,20 + chanceModifier)==0)
			m_fIsInvisible = true;
	}

	if( IsInvisible() && m_iHealth > 0 )
		CreateEffects( false );
	
	if( IsElite() && m_iHealth > 0 )
		CreateEffects( true );

	if( IsInvisible() )
	{
		SetRenderMode(kRenderTransAdd);
		SetRenderColor(0,0,100,40);
		m_iShieldHealth = acsmod_combine_shield_health.GetFloat() / 3.0f;
	}
	if( IsElite() ){
		m_iShieldHealth = acsmod_combine_shield_health.GetFloat();
	}

	if ( GetActiveWeapon() )
	{
		CBaseCombatWeapon *pWeapon;

		pWeapon = GetActiveWeapon();

		if( FClassnameIs( pWeapon, "weapon_shotgun" ) ){
			chanceModifier -= 4;
		}else if( FClassnameIs( pWeapon, "weapon_sniper" ) ){
			chanceModifier += 50;
		}
	}

	//Random stuff
	if( !IsInvisible() )
	{
		//Armor
		if( random->RandomInt(0,12 + chanceModifier) <= 0 )
		{
			m_Helmet = CreateEntityByName( "item_armor_helmet" );
			m_Helmet->SetOwnerEntity( this );
			m_Helmet->Spawn();
			m_bHasHelmet = true;
		}
		if( random->RandomInt(0,8 + chanceModifier) <= 0 )
		{
			m_SmallShield = CreateEntityByName( "item_armor_smallshield" );
			m_SmallShield->SetOwnerEntity( this );
			m_SmallShield->Spawn();
			m_bHasSmallShield = true;
		} 
		else if( random->RandomInt(0,25 + chanceModifier) <= 0 )
		{
			m_Shield = CreateEntityByName( "item_armor_shield" );
			m_Shield->SetOwnerEntity( this );
			m_Shield->Spawn();
			m_bHasShield = true;
		}
		//Stuff
		if( random->RandomInt(0,4) == 0 )
		{
			CBaseEntity *pPistol = CreateEntityByName( "combinepistol" );
			pPistol->SetOwnerEntity( this );
			pPistol->Spawn();
			m_bHasPistol = true;
		}
		else if( random->RandomInt(0,60) == 0 )
		{
			if(!nag.GetBool()){
				CBaseEntity *p357 = CreateEntityByName( "combine357" );
				p357->SetOwnerEntity( this );
				p357->Spawn();
				m_bHas357 = true;
			}
		}

		if( random->RandomInt(0,10) == 0 )
		{
			CBaseEntity *pNade = CreateEntityByName( "combinegrenade" );
			pNade->SetOwnerEntity( this );
			pNade->Spawn();
			m_bHasGrenade = true;
		}

		if( random->RandomInt(0,20) == 0 )
		{
			CBaseEntity *pSMG = CreateEntityByName( "combinesmg" );
			pSMG->SetOwnerEntity( this );
			pSMG->Spawn();
			m_bHasSMG = true;
		}else if( random->RandomInt(0,25) == 0 )
		{
			CBaseEntity *pSMG = CreateEntityByName( "combineshotgun" );
			pSMG->SetOwnerEntity( this );
			pSMG->Spawn();
			m_bHasShotgun = true;
		}
		
		if( random->RandomInt(0,100) == 0 )
		{
			CBaseEntity *pSniper = CreateEntityByName( "combinesniper" );
			pSniper->SetOwnerEntity( this );
			pSniper->Spawn();
			m_bHasSniper = true;
		}
	}
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
	//if(random->RandomInt(0,30)!=0)
	//{
		if( !Q_stricmp( pModelName, "models/combine_super_soldier.mdl" ) )
		{
			m_fIsElite = true;
		}
		else
		{
			m_fIsElite = false;
		}
	//}else{
		//m_fIsElite = true;
		//SetModelName( MAKE_STRING( "models/combine_super_soldier.mdl" ) );
	//}

	if(!IsElite())
	{
		/*if(nag.GetBool())
		{
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
		}else{*/
			SetModelName( MAKE_STRING( "models/combine_soldier.mdl" ) );
		//}
	}else{
		/*static const char* modelnames[] = {
		"models/combine_super_soldier.mdl",
		};
		SetModelName ( MAKE_STRING( modelnames[ random->RandomInt( 0, ARRAYSIZE(modelnames) - 1 ) ]) );*/
		SetModelName( MAKE_STRING( "models/combine_super_soldier.mdl" ) );
	}


	PrecacheModel( STRING( GetModelName() ) );

	UTIL_PrecacheOther( "item_healthvial" );
	UTIL_PrecacheOther( "item_healthkit" );
	UTIL_PrecacheOther( "weapon_frag" );
	if(nag.GetBool())
	{
		PrecacheModel( "models/weapons/w_pistolet.mdl" );
		PrecacheModel( "models/weapons/w_smg1.mdl" );
		PrecacheModel( "models/weapons/w_357.mdl" );
		PrecacheModel( "models/weapons/w_musket.mdl" );

		UTIL_PrecacheOther( "item_ammo_pellet_s" );
		UTIL_PrecacheOther( "item_ammo_pellet_m" );
		UTIL_PrecacheOther( "item_ammo_pellet_l" );
		UTIL_PrecacheOther( "item_ammo_pellet_xl" );
	}else{
		PrecacheModel( "models/weapons/w_pistol.mdl" );
		PrecacheModel( "models/weapons/w_smg1.mdl" );
		PrecacheModel( "models/weapons/w_357.mdl" );
		PrecacheModel( "models/weapons/w_awp.mdl" );

		UTIL_PrecacheOther( "item_ammo_ar2_altfire" );
		UTIL_PrecacheOther( "item_ammo_smg1_grenade" );
		UTIL_PrecacheOther( "weapon_pistol" );
		UTIL_PrecacheOther( "weapon_smg1" );
		UTIL_PrecacheOther( "weapon_sniper" );
		UTIL_PrecacheOther( "weapon_357" );
	}
	PrecacheModel( "sprites/light_glow02.vmt" );
	PrecacheModel( "sprites/nadelaser.vmt" );

	PrecacheScriptSound( "NPC.BloodSpray" );
	PrecacheScriptSound( "NPC.Headshot" );
	PrecacheScriptSound( "NPC.ShieldHit" );
	PrecacheScriptSound( "NPC.ShieldDown" );
	PrecacheScriptSound( "NPC.ExplodeGore" );

	PrecacheParticleSystem( "combines_headshot_blood" );
	PrecacheParticleSystem( "blood_impact_red_dust" );
	PrecacheParticleSystem( "shield_impact" );
	PrecacheParticleSystem( "Humah_Explode_blood" );

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
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CombineS::OnRestore( void )
{
	if( IsElite() && m_iHealth > 0 )
		CreateEffects( true );

	if( IsInvisible() && m_iHealth > 0 )
		CreateEffects( false );

	BaseClass::OnRestore();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_CombineS::CreateEffects( bool elite )
{
	// Start up the eye glow
	m_pMainGlow = CSprite::SpriteCreate( "sprites/light_glow02.vmt", GetLocalOrigin(), false );

	int	nAttachment = LookupAttachment( "eyes" );

	if ( m_pMainGlow != NULL )
	{
		m_pMainGlow->FollowEntity( this );
		m_pMainGlow->SetAttachment( this, nAttachment );
		if(elite){
			m_pMainGlow->SetTransparency( kRenderGlow, 255, 255, 255, 150, kRenderFxNoDissipation );
			m_pMainGlow->SetScale( 0.15f );
		}else{
			m_pMainGlow->SetTransparency( kRenderGlow, 0, 0, 255, 100, kRenderFxNoDissipation );
			m_pMainGlow->SetScale( 0.1f );
		}
		m_pMainGlow->SetGlowProxySize( 4.0f );
	}

	// Start up the eye trail
	m_pGlowTrail	= CSpriteTrail::SpriteTrailCreate( "sprites/nadelaser.vmt", GetLocalOrigin(), false );

	if ( m_pGlowTrail != NULL )
	{
		m_pGlowTrail->FollowEntity( this );
		m_pGlowTrail->SetAttachment( this, nAttachment );
		if(elite){
			m_pGlowTrail->SetTransparency( kRenderTransAdd, 255, 255, 255, 250, kRenderFxNone );
			m_pGlowTrail->SetStartWidth( 10.0f );
			m_pGlowTrail->SetEndWidth( 1.0f );
			m_pGlowTrail->SetLifeTime( 1.0f );
		}else{
			m_pGlowTrail->SetTransparency( kRenderTransAdd, 0, 0, 255, 200, kRenderFxNone );
			m_pGlowTrail->SetStartWidth( 12.0f );
			m_pGlowTrail->SetEndWidth( 1.0f );
			m_pGlowTrail->SetLifeTime( 2.0f );
		}
	}
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
	//SHIELD
	if( ( IsElite() || IsInvisible() ) && m_iShieldHealth > 0 ){	
		if( info.GetDamageType() & (DMG_BULLET | DMG_BUCKSHOT | DMG_BLAST) ) 
		{
			UTIL_Smoke( info.GetDamagePosition(), random->RandomInt( 10, 15 ), 10 );
			DispatchParticleEffect( "shield_impact",  info.GetDamagePosition() + RandomVector( -2.0f, 2.0f ), RandomAngle( 0, 360 ) );
			if ( random->RandomInt( 0, 1 ) == 0 )
			{
				CBaseEntity *pTrail = CreateEntityByName( "sparktrail" );
				pTrail->SetOwnerEntity( this );
				pTrail->Spawn();
			}
			EmitSound( "NPC.ShieldHit" );

			m_iShieldHealth -= info.GetDamage();

			if( m_iShieldHealth <= 0)
				EmitSound( "NPC.ShieldDown" );

			return 0.0f;
		}
	}

	switch( iHitGroup )
	{
		case HITGROUP_HEAD:
			{
				if(!IsElite() && !IsInvisible())
				{
					/*if(random->RandomInt(0,6)==0)
						EmitSound( "NPC.BloodSpray" );*/
					
					EmitSound( "NPC.Headshot" );
					DispatchParticleEffect( "combines_headshot_blood",  info.GetDamagePosition() + RandomVector( -2.0f, 2.0f ), RandomAngle( 0, 360 ) );
					
					if( info.GetDamageType() & (DMG_BULLET | DMG_BUCKSHOT | DMG_BLAST) ) 
					{
						g_pEffects->Sparks( info.GetDamagePosition(), 1, 2 );
					}
					else if( info.GetDamageType() & (DMG_CLUB | DMG_SLASH) )
					{
						CGib::SpawnStickyGibs( this, info.GetDamagePosition(), random->RandomInt(0,1) );
					}
				}
				break;
			}
		case HITGROUP_STOMACH:
		case HITGROUP_CHEST:
			{
				if(!IsElite() && !IsInvisible())
				{
					//Kevlar
					DispatchParticleEffect( "blood_impact_red_dust",  info.GetDamagePosition() + RandomVector( -1.0f, 1.0f ), RandomAngle( 0, 360 ) );
					UTIL_Smoke( info.GetDamagePosition(), random->RandomInt( 10, 15 ), 10 );
				}
				break;
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

	//Shield Down
	/*if( IsElite() || IsInvisible() )
		EmitSound( "NPC.ShieldDown" );*/

	if ( pPlayer != NULL )
	{
		if(!nag.GetBool()){
			// Elites drop alt-fire ammo, so long as they weren't killed by dissolving.
			if( GetActiveWeapon() && FClassnameIs( GetActiveWeapon(), "weapon_ar2" ) )
			{
	#ifdef HL2_EPISODIC
				if ( HasSpawnFlags( SF_COMBINE_NO_AR2DROP ) == false )
	#endif
				{
					CBaseEntity *pItem = DropItem( "item_ammo_smg1_grenade", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );

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
		}
		CHalfLife2 *pHL2GameRules = static_cast<CHalfLife2 *>(g_pGameRules);

		// Attempt to drop health
		if ( pHL2GameRules->NPC_ShouldDropHealth( pPlayer ) )
		{
			if(random->RandomInt(0,3)==0)
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

		if( IsElite() || IsInvisible() ){
			DropItem( "item_battery", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			DropItem( "item_healthvial", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		}

		if(nag.GetBool())
		{
			if(m_bHasPistol)
			{
				DropItem( "weapon_pistolet", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
				if(random->RandomInt(0,10)==0)
					DropItem( "item_ammo_pellet_m", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			}
			if(random->RandomInt(0,15)==0)
				DropItem( "item_healthvial", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			if(m_bHasSMG)
			{
				DropItem( "weapon_rifle", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
				if(random->RandomInt(0,6)==0)
					DropItem( "item_ammo_pellet_m", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			}
			if(m_bHasShotgun)
			{
				DropItem( "weapon_blunderbuss", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
				if(random->RandomInt(0,6)==0)
					DropItem( "item_ammo_pellet_m", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			}
			if(m_bHasSniper)
			{
				DropItem( "weapon_musket", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
				if(random->RandomInt(0,20)==0)
					DropItem( "item_ammo_pellet_l", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			}
			if(random->RandomInt(0,30)==0)
				DropItem( "item_healthkit", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			if(random->RandomInt(0,40)==0)
				DropItem( "item_ammo_pellet_s", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			if(random->RandomInt(0,60)==0)
				DropItem( "item_battery", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			if(random->RandomInt(0,100)==0)
				DropItem( "weapon_frag", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		}else{
			if(m_bHasPistol)
			{
				DropItem( "weapon_pistol", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
				if(random->RandomInt(0,10)==0)
					DropItem( "item_ammo_pistol", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			}
			if(random->RandomInt(0,20)==0)
				DropItem( "item_healthvial", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			if(m_bHasSMG)
			{
				DropItem( "weapon_smg1", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
				if(random->RandomInt(0,20)==0)
					DropItem( "item_ammo_smg1", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			}
			if(m_bHasShotgun)
			{
				DropItem( "weapon_shotgun", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
				if(random->RandomInt(0,20)==0)
					DropItem( "item_ammo_buckshot", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			}
			if(random->RandomInt(0,40)==0)
				DropItem( "item_healthkit", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			if(random->RandomInt(0,60)==0)
				DropItem( "item_battery", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			//if(m_bHasGrenade)
				//DropItem( "weapon_frag", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			if(m_bHasSniper)
				DropItem( "weapon_sniper", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
			if(m_bHas357)
				DropItem( "weapon_357", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		}
	}

	if(m_bHasHelmet){
		DropItem( "item_healthvial", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		if(m_Helmet && m_Helmet->GetHealth() > 0){
			CGib::SpawnSpecificGibs( this, 1, 50, 200, "models/misc/faceshield.mdl", 120 );
		}
	}
	if(m_bHasSmallShield){
		if(m_SmallShield && m_SmallShield->GetHealth() > 0){
			CGib::SpawnSpecificGibs( this, 1, 50, 200, "models/misc/armshield.mdl", 140 );
		}
	}
	if(m_bHasShield){
		DropItem( "item_healthkit", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		DropItem( "item_battery", WorldSpaceCenter()+RandomVector(-4,4), RandomAngle(0,360) );
		if(m_Shield && m_Shield->GetHealth() > 0){
			CGib::SpawnSpecificGibs( this, 1, 50, 200, "models/misc/shield.mdl", 160 );
		}
	}

	//Explosion
	if( !IsElite() )
	{
		if( m_iHealth <= -90 )
		{
			if( info.GetDamageType() & ( DMG_BLAST | DMG_VEHICLE | DMG_FALL | DMG_CRUSH ) )
			{
				EmitSound( "NPC.ExplodeGore" );
			
				DispatchParticleEffect( "Humah_Explode_blood", GetAbsOrigin(), GetAbsAngles() );
			
				SetModel( "models/humans/charple03.mdl" );

				//CreateRagGib( "models/zombie/zombie1_legs.mdl", WorldSpaceCenter(), GetAbsAngles(), 500);
				CGib::SpawnSpecificGibs( this, 1, 100, 600, "models/gibs/hgibs_jaw.mdl", 5 );
				CGib::SpawnSpecificGibs( this, 1, 100, 600, "models/gibs/leg.mdl", 5 );
				CGib::SpawnSpecificGibs( this, 1, 100, 600, "models/gibs/hgibs_rib.mdl", 5 );
				CGib::SpawnSpecificGibs( this, 1, 100, 600, "models/gibs/hgibs_scapula.mdl", 5 );
				CGib::SpawnSpecificGibs( this, 1, 100, 600, "models/gibs/hgibs_spine.mdl", 5 );

				CGib::SpawnStickyGibs( this, GetAbsOrigin(), random->RandomInt(10,20) );
				
				//BLOOOOOOD !!!!
				trace_t tr;
				Vector randVector;
				//Create 128 random decals that are within +/- 256 units.
				for ( int i = 0 ; i < 64; i++ )
				{
					randVector.x = random->RandomFloat( -256.0f, 256.0f );
					randVector.y = random->RandomFloat( -256.0f, 256.0f );
					randVector.z = random->RandomFloat( -256.0f, 256.0f );

					AI_TraceLine( GetAbsOrigin()+Vector(0,0,1), GetAbsOrigin()-randVector, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );			 

					UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_RED );
				}

				for ( int i = 0 ; i < 4; i++ )
				{
					randVector.x = random->RandomFloat( -256.0f, 256.0f );
					randVector.y = random->RandomFloat( -256.0f, 256.0f );
					randVector.z = random->RandomFloat( -256.0f, 256.0f );

					AI_TraceLine( GetAbsOrigin()+Vector(0,0,1), GetAbsOrigin()-randVector, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );			 

					UTIL_DecalTrace( &tr, "Big_Gib_Blood" );
				}
			}
		}
	}

	if( info.GetDamageType() & ( DMG_SLASH | DMG_CRUSH | DMG_CLUB ) )
		CGib::SpawnStickyGibs( this, GetAbsOrigin(), random->RandomInt(0,2) );

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
	if ( info.GetAmmoType() == GetAmmoDef()->Index("AR2") )
		return true;

	// 357 rounds are heavy damage
	if ( info.GetAmmoType() == GetAmmoDef()->Index("357") )
		return true;

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

LINK_ENTITY_TO_CLASS( combinepistol, CCombinePistol );

void CCombinePistol::Spawn()
{
	CAI_BaseNPC *pOwner = ( GetOwnerEntity() ) ? GetOwnerEntity()->MyNPCPointer() : NULL;

	if ( pOwner )
	{
		int attachment = pOwner->LookupAttachment( "zipline" );
		if ( attachment )
		{
			SetAbsAngles( GetOwnerEntity()->GetAbsAngles() + QAngle(-110,0,0) );
			SetParent( GetOwnerEntity(), attachment );

			Vector vecPosition;
			vecPosition.Init( -17, 8, -9 );
			SetLocalOrigin( vecPosition );
		}
	}

	if(nag.GetBool())
		SetModel( "models/weapons/w_pistolet.mdl" );
	else
		SetModel( "models/weapons/w_pistol.mdl" );
	SetSolid( SOLID_VPHYSICS );
}

LINK_ENTITY_TO_CLASS( combinegrenade, CCombineGrenade );

void CCombineGrenade::Spawn()
{
	CAI_BaseNPC *pOwner = ( GetOwnerEntity() ) ? GetOwnerEntity()->MyNPCPointer() : NULL;

	if ( pOwner )
	{
		int attachment = pOwner->LookupAttachment( "zipline" );
		if ( attachment )
		{
			SetAbsAngles( GetOwnerEntity()->GetAbsAngles() );
			SetParent( GetOwnerEntity(), attachment );

			Vector vecPosition;
			vecPosition.Init( -20, 2.5, -6.5 );
			SetLocalOrigin( vecPosition );
		}
	}

	if(nag.GetBool())
		SetModel( "models/weapons/w_dynamite.mdl" );
	else
		SetModel( "models/weapons/w_grenade.mdl" );
	SetSolid( SOLID_BBOX );
	m_takedamage = DAMAGE_YES;
	SetHealth( 15 );
}

void CCombineGrenade::Event_Killed( const CTakeDamageInfo &info )
{
	AddSolidFlags( FSOLID_NOT_SOLID );
	m_takedamage = DAMAGE_NO;
	SetSolid( SOLID_NONE );

	EmitSound( "BaseGrenade.Explode" );

	UTIL_ScreenShake( GetAbsOrigin(), 40.0, 300.0, 0.5, 400, SHAKE_START );
	
	Vector vecAbsOrigin = GetAbsOrigin();
	CPASFilter filter( GetAbsOrigin() );

	te->Explosion( filter, -1.0, // don't apply cl_interp delay
		&vecAbsOrigin,
		g_sModelIndexFireball,
		300 * .05,  //Scale
		15,
		TE_EXPLFLAG_NONE,
		300, //Radius
		90, //Magnitude
		&Vector(0,0,1)
		);

	CSoundEnt::InsertSound ( SOUND_COMBAT, GetAbsOrigin(), 2048, 3.0 );

	RadiusDamage( CTakeDamageInfo( this, this, 90, DMG_BLAST ), GetAbsOrigin(), 300, CLASS_NONE, 0 );

	UTIL_Remove( this );
}

LINK_ENTITY_TO_CLASS( combinesmg, CCombineSMG1 );
void CCombineSMG1::Spawn()
{
	CAI_BaseNPC *pOwner = ( GetOwnerEntity() ) ? GetOwnerEntity()->MyNPCPointer() : NULL;

	if ( pOwner )
	{
		int attachment = pOwner->LookupAttachment( "zipline" );
		if ( attachment )
		{
			//SetAbsAngles( GetOwnerEntity()->GetAbsAngles() + QAngle(180,0,45) ); //Standart SMG rot
			SetAbsAngles( GetOwnerEntity()->GetAbsAngles() + QAngle(120,90,0) );
			SetParent( GetOwnerEntity(), attachment );

			Vector vecPosition;
			//vecPosition.Init( -5.5, -2.5, -2 ); //Standart SMG pos
			vecPosition.Init( -12, -.5, -2 );
			SetLocalOrigin( vecPosition );
		}
	}
	if(nag.GetBool())
		SetModel( "models/weapons/w_smg1.mdl" );
	else
		SetModel( "models/weapons/w_smg1.mdl" );
	SetSolid( SOLID_VPHYSICS );
}

LINK_ENTITY_TO_CLASS( combineshotgun, CCombineShotgun );
void CCombineShotgun::Spawn()
{
	CAI_BaseNPC *pOwner = ( GetOwnerEntity() ) ? GetOwnerEntity()->MyNPCPointer() : NULL;

	if ( pOwner )
	{
		int attachment = pOwner->LookupAttachment( "zipline" );
		if ( attachment )
		{
			//SetAbsAngles( GetOwnerEntity()->GetAbsAngles() + QAngle(180,0,45) );
			SetAbsAngles( GetOwnerEntity()->GetAbsAngles() + QAngle(-60,90,0) );
			SetParent( GetOwnerEntity(), attachment );

			Vector vecPosition;
			vecPosition.Init( -11, -1.5, -2 );
			SetLocalOrigin( vecPosition );
		}
	}
	if(nag.GetBool())
		SetModel( "models/weapons/w_blunderbuss.mdl" );
	else
		SetModel( "models/weapons/w_shotgun.mdl" );
	SetSolid( SOLID_VPHYSICS );
}

LINK_ENTITY_TO_CLASS( combinesniper, CCombineSniper );
void CCombineSniper::Spawn()
{
	CAI_BaseNPC *pOwner = ( GetOwnerEntity() ) ? GetOwnerEntity()->MyNPCPointer() : NULL;

	if ( pOwner )
	{
		int attachment = pOwner->LookupAttachment( "zipline" );
		if ( attachment )
		{
			SetAbsAngles( GetOwnerEntity()->GetAbsAngles() + QAngle(0,0,50) );
			SetParent( GetOwnerEntity(), attachment );

			Vector vecPosition;
			vecPosition.Init( -14, -1., -2 );
			SetLocalOrigin( vecPosition );
		}
	}
	if(nag.GetBool())
		SetModel( "models/weapons/w_musket.mdl" );
	else
		SetModel( "models/weapons/w_awp.mdl" );
	SetSolid( SOLID_VPHYSICS );
}

LINK_ENTITY_TO_CLASS( combine357, CCombine357 );
void CCombine357::Spawn()
{
	CAI_BaseNPC *pOwner = ( GetOwnerEntity() ) ? GetOwnerEntity()->MyNPCPointer() : NULL;

	if ( pOwner )
	{
		int attachment = pOwner->LookupAttachment( "zipline" );
		if ( attachment )
		{
			SetAbsAngles( GetOwnerEntity()->GetAbsAngles() + QAngle(-110,90,0) );
			SetParent( GetOwnerEntity(), attachment );

			Vector vecPosition;
			vecPosition.Init( -17, 11, -9 );
			SetLocalOrigin( vecPosition );
		}
	}

	SetModel( "models/weapons/w_357.mdl" );
	SetSolid( SOLID_VPHYSICS );
}