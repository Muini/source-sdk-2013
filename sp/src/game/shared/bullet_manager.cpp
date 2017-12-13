// (More) Reallistic simulated bullets
//
// This code is originally based from article on Valve Developer Community
// The original code you can find by this link:
// http://developer.valvesoftware.com/wiki/Simulated_Bullets
 
// NOTENOTE: Tested only on localhost. There maybe a latency errors and others
// NOTENOTE: The simulation might be strange.
 
#include "cbase.h"
#include "util_shared.h"
#include "bullet_manager.h"
#include "effect_dispatch_data.h"
#include "tier0/vprof.h"
#include "decals.h"
#include "baseentity_shared.h"
#include "IEffects.h"

#ifdef CLIENT_DLL
#else
	#include "fire.h"

#endif
 
CBulletManager *g_pBulletManager;
CUtlLinkedList<CSimulatedBullet*> g_Bullets;
 
#ifdef CLIENT_DLL//-------------------------------------------------
#include "engine/ivdebugoverlay.h"
#include "c_te_effect_dispatch.h"
ConVar g_debug_client_bullets( "g_debug_client_bullets", "0", FCVAR_CHEAT );
extern void FX_PlayerTracer( Vector& start, Vector& end);
#else//-------------------------------------------------------------
#include "te_effect_dispatch.h"
#include "soundent.h"
#include "player_pickup.h"
#include "ilagcompensationmanager.h"
ConVar g_debug_bullets( "g_debug_bullets", "0", FCVAR_CHEAT, "Debug of bullet simulation\nThe white line shows the bullet trail.\nThe red line shows not passed penetration test.\nThe green line shows passed penetration test. Turn developer mode for more information." );
#endif//------------------------------------------------------------
 
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
 
#define MAX_RICO_DOT_ANGLE 0.2f  //Maximum dot allowed for any ricochet
#define MIN_RICO_SPEED_PERC 0.5f //Minimum speed percent allowed for any ricochet
 
//extern ConVar acsmod_bullet_penetration_ratio;
 
/*static void BulletSpeedModifierCallback(ConVar *var, const char *pOldString, float flOldValue)
{
	if(var->GetFloat()==0.0f) //To avoid math exception
		var->Revert();
}*/
ConVar sv_bullet_speed_modifier( "sv_bullet_speed_modifier", "700.000000", (FCVAR_ARCHIVE | FCVAR_REPLICATED),
								"Density/(This Value) * (Distance Penetrated) = (Change in Speed)"/*,
								BulletSpeedModifierCallback*/ );
 
/*static void UnrealRicochetCallback(ConVar *var, const char *pOldString)
{
	if(gpGlobals->maxClients > 1)
	{
		var->Revert();
		Msg("Cannot use unreal ricochet in multiplayer game\n");
	}
 
	if(var->GetBool()) //To avoid math exception
		Warning("\nWarning! Enabling unreal ricochet may cause the game crash.\n\n");
}*/
ConVar sv_bullet_unrealricochet( "sv_bullet_unrealricochet", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Unreal ricochet"/*,UnrealRicochetCallback*/);
 
extern ConVar acsmod_hitmarker;
 
LINK_ENTITY_TO_CLASS( bullet_manager, CBulletManager );
 
 
 
//==================================================
// Purpose:	Constructor
//==================================================
CSimulatedBullet::CSimulatedBullet()
{
	m_vecOrigin.Init();
	m_vecDirShooting.Init();
	m_flInitialBulletSpeed = m_flBulletSpeed = 0;
	m_flEntryDensity = 0.0f;
	bStuckInWall = false;
	bHasInteracted = false;
 
	m_iDamageType = 2;
}
 
//==================================================
// Purpose:	Constructor
//==================================================
CSimulatedBullet::CSimulatedBullet( FireBulletsInfo_t info,Vector newdir,CBaseEntity *pInfictor, CBaseEntity *pAdditionalIgnoreEnt,
						bool bTraceHull
#ifndef CLIENT_DLL
						, CBaseEntity *pCaller
#endif
)
{
	// Input validation
	Assert( pInfictor );
#ifndef CLIENT_DLL
	Assert( pCaller );
#endif
 
	bulletinfo = info;			// Setup Fire bullets information here
 
	p_eInfictor = pInfictor;	// Setup inflictor
 
	// Create a list of entities with which this bullet does not collide.
	m_pIgnoreList = new CTraceFilterSimpleList(COLLISION_GROUP_NONE);
	Assert( m_pIgnoreList );
 
	bStuckInWall = false;
	bHasInteracted = info.m_bAlreadyInterract;
 
	// Don't collide with the entity firing the bullet.
	m_pIgnoreList->AddEntityToIgnore(p_eInfictor);
 
	// Don't collide with some optionally-specified entity.
	if(pAdditionalIgnoreEnt!=NULL)
		m_pIgnoreList->AddEntityToIgnore(pAdditionalIgnoreEnt);
 
	m_iDamageType = GetAmmoDef()->DamageType( bulletinfo.m_iAmmoType );
 
 
	//m_szTracerName = (char*)p_eInfictor->GetTracerType(); 
 
	// Basic information about the bullet
	m_flInitialBulletSpeed = m_flBulletSpeed = GetAmmoDef()->GetAmmoOfIndex(bulletinfo.m_iAmmoType)->bulletSpeed;
	m_vecDirShooting = newdir;

	m_vecOrigin = bulletinfo.m_vecSrc;
	
	if ( p_eInfictor->IsPlayer() )
	{
		// adjust tracer position for player
		Vector forward, right;
		CBasePlayer *pPlayer = ToBasePlayer( p_eInfictor );
		pPlayer->EyeVectors( &forward, &right, NULL );
		m_vecOrigin = bulletinfo.m_vecSrc + Vector ( 0 , 0 , -4 ) + right * 2 + forward * 16;
	}
	else
	{
		m_vecOrigin = bulletinfo.m_vecSrc;

		CBaseCombatCharacter *pBCC = p_eInfictor->MyCombatCharacterPointer();
		if ( pBCC != NULL )
		{
			CBaseCombatWeapon *pWeapon = pBCC->GetActiveWeapon();

			if ( pWeapon != NULL )
			{
				Vector vecMuzzle;
				QAngle vecMuzzleAngles;

				if ( pWeapon->GetAttachment( 1, vecMuzzle, vecMuzzleAngles ) )
				{
					m_vecOrigin = vecMuzzle;
				}
			}
		}
	}

	m_bTraceHull = bTraceHull;
#ifndef CLIENT_DLL
	m_hCaller = pCaller;
#endif
	m_flEntryDensity = 0.0f;
	m_vecTraceRay = m_vecOrigin + m_vecDirShooting * m_flBulletSpeed;
	m_flRayLength = m_flInitialBulletSpeed;
}
 
 
 
//==================================================
// Purpose:	Deconstructor
//==================================================
CSimulatedBullet::~CSimulatedBullet()
{
	delete m_pIgnoreList;
}
//==================================================
// Purpose:	Simulates a bullet through a ray of its bullet speed
//==================================================
bool CSimulatedBullet::SimulateBullet(void)
{
	VPROF( "C_SimulatedBullet::SimulateBullet" );
 
	if(!IsFinite(m_flBulletSpeed))
		return false;		 //prevent a weird crash
 
	trace_t trace;
 
	if(m_flBulletSpeed <= 0) //Avoid errors;
		return false;
 
	if(!p_eInfictor)
	{
		p_eInfictor = bulletinfo.m_pAttacker;
 
		if(!p_eInfictor)
			return false;
	}
 
	m_flRayLength = m_flBulletSpeed;
 
	m_flBulletSpeed += 0.8f * m_vecDirShooting.z; //TODO: Bullet mass
	m_vecTraceRay = m_vecOrigin + m_vecDirShooting * m_flBulletSpeed; 
 
	m_vecDirShooting.z -= .4 / m_flBulletSpeed;
 
#ifdef GAME_DLL
	if(bulletinfo.m_flLatency!= 0 )
	{
		m_vecTraceRay *= bulletinfo.m_flLatency * 100;
	}
#endif
 
	if(!IsInWorld())
	{
		return false;
	}
 
	if(bStuckInWall)
		return false;
 
	if(!m_bWasInWater)
	{
		WaterHit(m_vecOrigin,m_vecTraceRay);
	}
 
	m_bWasInWater = (UTIL_PointContents(m_vecOrigin)& MASK_WATER )?true:false;
 
	if(m_bWasInWater)
	{
		m_flBulletSpeed -= m_flBulletSpeed * 0.6;
#ifdef GAME_DLL
		//This is a server stuff
		UTIL_BubbleTrail(m_vecOrigin, m_vecTraceRay, 5 );
#endif
	}
 
#ifndef CLIENT_DLL
	if(m_bWasInWater)
	{
		CEffectData tracerData;
		tracerData.m_vStart = m_vecOrigin;
		tracerData.m_vOrigin = m_vecTraceRay;
 
		tracerData.m_fFlags = TRACER_TYPE_WATERBULLET;
 
		DispatchEffect( "TracerSound", tracerData );
	}
#endif
 
 
	bool bulletSpeedCheck;
	bulletSpeedCheck = false;
 
 
	if(m_bTraceHull)
		UTIL_TraceHull( m_vecOrigin, m_vecTraceRay, Vector(-3, -3, -3), Vector(3, 3, 3), MASK_SHOT, m_pIgnoreList, &trace );
	else
		UTIL_TraceLine( m_vecOrigin, m_vecTraceRay, MASK_SHOT, m_pIgnoreList, &trace );
 
	if(!m_bWasInWater)
	{
		//UTIL_Tracer( m_vecOrigin, trace.endpos, p_eInfictor->entindex(), TRACER_DONT_USE_ATTACHMENT, 0, true,p_eInfictor->GetTracerType());
		MakeTracer(trace, bulletinfo.m_iAmmoType);
	}
 
#ifdef CLIENT_DLL
	if(g_debug_client_bullets.GetBool())
	{
		debugoverlay->AddLineOverlay( trace.startpos, trace.endpos, 255, 0, 0, true, 10.0f );
	}
#else
	if(g_debug_bullets.GetBool())
	{
		NDebugOverlay::Line( trace.startpos, trace.endpos, 255, 255, 255, true, 10.0f );
	}
 
	// Now hit all triggers along the ray that respond to shots...
	// Clip the ray to the first collided solid returned from traceline
	CTakeDamageInfo triggerInfo( p_eInfictor, bulletinfo.m_pAttacker, bulletinfo.m_flDamage, GetDamageType() );
	CalculateBulletDamageForce( &triggerInfo, bulletinfo.m_iAmmoType, m_vecDirShooting, trace.endpos );
	triggerInfo.ScaleDamageForce( bulletinfo.m_flDamageForceScale );
	triggerInfo.SetAmmoType( bulletinfo.m_iAmmoType );
	BulletManager()->SendTraceAttackToTriggers( triggerInfo, trace.startpos, trace.endpos, m_vecDirShooting );
#endif
 
	if (trace.fraction == 1.0f)
	{
		m_vecOrigin += m_vecDirShooting * m_flBulletSpeed; //Do the WAY
 
		CEffectData data;
		data.m_vStart = trace.startpos;
		data.m_vOrigin = trace.endpos;
		data.m_nDamageType = GetDamageType();
 
		DispatchEffect( "RagdollImpact", data );
 
		return true;
	}
	else
	{
		EntityImpact(trace);
 
		if (trace.m_pEnt == p_eInfictor) //HACK: Remove bullet if we hit self (for frag grenades)
			return false;
 
		if(!(trace.surface.flags&SURF_SKY))
		{
			/*if(trace.allsolid)//in solid
			{
				if(!AllSolid(trace))
					return false;
 
				m_vecOrigin += m_vecDirShooting * m_flBulletSpeed; //Do the WAY
 
				bulletSpeedCheck = true;
			}
			else */if(trace.fraction!=1.0f)//hit solid
			{
				if(!EndSolid(trace))
					return false;
 
				bulletSpeedCheck = true;
			}
			else if(trace.startsolid)//exit solid
			{
				if(!StartSolid(trace))
					return false;
 
				m_vecOrigin += m_vecDirShooting * m_flBulletSpeed; //Do the WAY
 
				bulletSpeedCheck = true;
			}
			else
			{
				//don't do a bullet speed check for not touching anything
			}
		}
		else
		{
			return false; //Through sky? No.
		}
	}
 
	/*if(sv_bullet_unrealricochet.GetBool()) //Fun bullet ricochet fix
	{
		delete m_pIgnoreList; //Prevent causing of memory leak
		m_pIgnoreList = new CTraceFilterSimpleList(COLLISION_GROUP_NONE);
	}*/
 
	if(bulletSpeedCheck)
	{
		if(m_flBulletSpeed<=BulletManager()->BulletStopSpeed())
		{
			return false;
		}
	}
 
	return true;
}
 
 
void CSimulatedBullet::MakeTracer(trace_t &tr, const int &ammoType){
				
	#ifdef GAME_DLL
						
			if( !FStrEq(tr.surface.name,"tools/toolsblockbullets") && !FStrEq(tr.surface.name,"tools/toolsnodraw") && !( tr.surface.flags & SURF_SKY ) )
			{
				if(tr.fraction <= 1)
				{
					
					Vector vecTracerSrc = tr.startpos;
					//vecTracerSrc.x += 128.0f; //Tracer need to begin out of the weapon
					//vecTracerSrc.y += 128.0f;

					int iAttachment = TRACER_DONT_USE_ATTACHMENT;

					switch ( GetAmmoDef()->TracerType( bulletinfo.m_iAmmoType) )
					{
						case TRACER_SUBS:
							UTIL_ParticleTracer( "bullet_tracer_subs", vecTracerSrc, tr.endpos, 0, iAttachment, false );
							break;

						case TRACER_SUPERS:
							UTIL_ParticleTracer( "bullet_tracer_supers", vecTracerSrc, tr.endpos, 0, iAttachment, true );
							break;

						case TRACER_RED:
							UTIL_ParticleTracer( "bullet_tracer_red", vecTracerSrc, tr.endpos, 0, iAttachment, true );
							break;

						case TRACER_GREEN:
							UTIL_ParticleTracer( "bullet_tracer_green", vecTracerSrc, tr.endpos, 0, iAttachment, true );
							break;

						case TRACER_FIRE:
							UTIL_ParticleTracer( "bullet_tracer_fire", vecTracerSrc, tr.endpos, 0, iAttachment, false );
							break;

						case TRACER_BIG:
							UTIL_ParticleTracer( "bullet_tracer_big", vecTracerSrc, tr.endpos, 0, iAttachment, true );
							break;

						case TRACER_BIGFIRE:
							UTIL_ParticleTracer( "bullet_tracer_bigfire", vecTracerSrc, tr.endpos, 0, iAttachment, false );
							break;

						case TRACER_BEAM:
							UTIL_ParticleTracer( "bullet_tracer_electrical", vecTracerSrc, tr.endpos, 0, iAttachment, true );
							break;

						case TRACER_LASER:
							UTIL_ParticleTracer( "bullet_tracer_electrical", vecTracerSrc, tr.endpos, 0, iAttachment, true );
							break;

						default:
							UTIL_ParticleTracer( "bullet_tracer_sound", vecTracerSrc, tr.endpos, 0, iAttachment, true );
							break;
					}

				}
			}
						
		#endif
}

void CSimulatedBullet::MakeImpact(trace_t &tr, const int &ammoType){
				
	#ifdef GAME_DLL

			if( !FStrEq(tr.surface.name,"tools/toolsblockbullets") && !FStrEq(tr.surface.name,"tools/toolsnodraw") && !( tr.surface.flags & SURF_SKY ) )
			{
				Vector vecUp = Vector(0,0,1);
				//GetVectors( NULL, NULL, &vecUp );

				Vector vecTracerSrc = tr.startpos;
				//vecTracerSrc.x += 128.0f; //Tracer need to begin out of the weapon
				//vecTracerSrc.y += 128.0f;

				QAngle vecAngles;
				VectorAngles( tr.plane.normal, vecAngles );
				vecAngles.x += 90;

				surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
						
				//Bullets are powerful and dangerous !
				int damageType = GetAmmoDef()->DamageType(bulletinfo.m_iAmmoType);

				if( damageType & DMG_BULLET ){
						UTIL_ScreenShake( tr.endpos, 3.0, 150.0, 0.1, 50, SHAKE_START );
				}
				if( damageType & DMG_BURN ){

						//RadiusDamage( CTakeDamageInfo( this, this, 11, DMG_BURN | DMG_BULLET ), tr.endpos + ( vecUp * 6.0f ), 30, CLASS_NONE, 0 );
						DispatchParticleEffect( "balle_incendiaire", tr.endpos + ( vecUp * 2.0f ), vecAngles );
						UTIL_DecalTrace( &tr, "FadingScorch" );
						//Ignite it !
						if ( tr.m_pEnt )
						{
							if ( tr.m_pEnt->IsNPC() )
							{
								if(random->RandomInt(0,1)!=0)
									tr.m_pEnt->GetBaseAnimating()->Ignite(5.0f,true,1.0f,false);
							}
						}
						if(random->RandomInt(0,20)==0)
						{
							float randomTime = random->RandomFloat(0.5f,3.0f);
							FireSystem_StartFire(tr.endpos, random->RandomFloat(16.0f,32.0f), 5.0f, randomTime, (SF_FIRE_START_ON), p_eInfictor, FIRE_NATURAL );
						}
				}
				if( damageType & DMG_BLAST ){

						UTIL_ScreenShake( tr.endpos, 10.0, 150.0, 0.5, 200, SHAKE_START );

						DispatchParticleEffect( "balle_50BGMHEI", tr.endpos + ( vecUp * 4.0f ), vecAngles );
						
						UTIL_DecalTrace( &tr, "FadingScorch" );

						if( psurf->game.material == CHAR_TEX_BLOODYFLESH || psurf->game.material == CHAR_TEX_FLESH  )
						{	DispatchParticleEffect( "blood_human_semiexplode", tr.endpos + ( vecUp * 4.0f ), vecAngles ); }
						else{
							CTakeDamageInfo dmgInfo( p_eInfictor, bulletinfo.m_pAttacker, bulletinfo.m_flDamage / 2, GetDamageType() );
							RadiusDamage( dmgInfo, tr.endpos + ( vecUp * 4.0f ), 100, CLASS_NONE, 0 );
						}
				}
				if( damageType & ( DMG_ENERGYBEAM | DMG_PLASMA | DMG_SHOCK ) ){

						DispatchParticleEffect( "electrical_impact", tr.endpos + ( vecUp * 2.0f ), vecAngles );
						UTIL_DecalTrace( &tr, "FadingScorch" );
						//Ignite it !
						if ( tr.m_pEnt )
						{
							if ( tr.m_pEnt->IsNPC() )
							{
								if(random->RandomInt(0,20)==0)
									tr.m_pEnt->GetBaseAnimating()->Ignite(5.0f,true,1.0f,false);
							}
						}
						if(random->RandomInt(0,30)==0)
						{
							float randomTime = random->RandomFloat(0.5f,3.0f);
							FireSystem_StartFire(tr.endpos, random->RandomFloat(16.0f,32.0f), 5.0f, randomTime, (SF_FIRE_START_ON), p_eInfictor, FIRE_NATURAL );
						}
				}
				if( damageType & DMG_DISSOLVE ){

					g_pEffects->Sparks( tr.endpos, 1, 1 );

				}
				if( damageType & DMG_BUCKSHOT ){}
				if( damageType & DMG_BIG ){

						UTIL_ScreenShake( tr.endpos, 5.0, 150.0, 0.4, 150, SHAKE_START );

						DispatchParticleEffect( "balle_50BGM", tr.endpos + ( vecUp * 4.0f ), vecAngles );

						if( psurf->game.material == CHAR_TEX_BLOODYFLESH || psurf->game.material == CHAR_TEX_FLESH  )
						{	DispatchParticleEffect( "zombies_headshot_blood_melee", tr.endpos + ( vecUp * 2.0f ), vecAngles ); }
				}
				if( damageType & DMG_PERFORANT ){

					DispatchParticleEffect( "balle_AP", tr.endpos, vecAngles );
					//g_pEffects->Sparks( tr.endpos, 1, 1 );
				}

				switch ( GetAmmoDef()->TracerType( bulletinfo.m_iAmmoType) )
				{
					case TRACER_SUBS:
						break;

					case TRACER_SUPERS:
						break;

					case TRACER_RED:
						DispatchParticleEffect( "balle_tracer_red", tr.endpos + ( vecUp * 1.0f ), vecAngles );
						break;

					case TRACER_GREEN:
						DispatchParticleEffect( "balle_tracer_green", tr.endpos + ( vecUp * 1.0f ), vecAngles );
						break;

					case TRACER_BEAM:
						break;

					case TRACER_LASER:
						break;

					default:
						break;
				}
			}
						
		#endif
}

//==================================================
// Purpose:	Simulates when a solid is exited
//==================================================
bool CSimulatedBullet::StartSolid(trace_t &ptr)
{
	if(bHasInteracted) return false;
	switch(BulletManager()->BulletStopSpeed())
	{
		case BUTTER_MODE:
		{
			//Do nothing to bullet speed
			return true;
		}
		case ONE_HIT_MODE:
		{
			return false;
		}
		default:
		{
			float flPenetrationDistance = VectorLength(AbsEntry - AbsExit);
 
			m_flBulletSpeed -= flPenetrationDistance * m_flEntryDensity / sv_bullet_speed_modifier.GetFloat();
			return true;
		}
	}
	return true;
}
 
 
//==================================================
// Purpose:	Simulates when a solid is being passed through
//==================================================
bool CSimulatedBullet::AllSolid(trace_t &ptr)
{
	if(bHasInteracted) return false;
	switch(BulletManager()->BulletStopSpeed())
	{
		case BUTTER_MODE:
		{
			//Do nothing to bullet speed
			return true;
		}
		case ONE_HIT_MODE:
		{
			return false;
		}
		default:
		{
			m_flBulletSpeed -= m_flBulletSpeed * m_flEntryDensity / sv_bullet_speed_modifier.GetFloat();
			return true;
		}
	}
	return true;
}
 
 
//==================================================
// Purpose:	Simulate when a surface is hit
//==================================================
bool CSimulatedBullet::EndSolid(trace_t &ptr)
{
	m_vecEntryPosition = ptr.endpos;
 
#ifndef CLIENT_DLL
	int soundEntChannel = ( bulletinfo.m_nFlags&FIRE_BULLETS_TEMPORARY_DANGER_SOUND ) ? SOUNDENT_CHANNEL_BULLET_IMPACT : SOUNDENT_CHANNEL_UNSPECIFIED;
 
	CSoundEnt::InsertSound( SOUND_BULLET_IMPACT, m_vecEntryPosition, 200, 0.5, p_eInfictor, soundEntChannel );
#endif
 
	if(FStrEq(ptr.surface.name,"tools/toolsblockbullets"))
		return false;
	if(FStrEq(ptr.surface.name,"tools/toolsnodraw"))
		return false;
	if(FStrEq(ptr.surface.name,"tools/toolsskybox"))
		return false;
	if(FStrEq(ptr.surface.name,"glass/glasswindow007a"))
		return false;
	if(FStrEq(ptr.surface.name,"glass/glasswindow020a"))
		return false;
 
	int chanceRicochet = 100;
	DesiredDistance = 0.0f;

	m_flEntryDensity = physprops->GetSurfaceData(ptr.surface.surfaceProps)->physics.density;
 
	surfacedata_t *p_penetrsurf = physprops->GetSurfaceData( ptr.surface.surfaceProps );
	switch (p_penetrsurf->game.material)
	{
		case CHAR_TEX_WOOD:
			DesiredDistance = 9.0f; // 9 units in hammer
			chanceRicochet -=50;
			break;
		case CHAR_TEX_GRATE:
			DesiredDistance = 6.0f; // 6 units in hammer
			chanceRicochet -=80;
			break;
		case CHAR_TEX_CONCRETE:
			DesiredDistance = 4.0f; // 4 units in hammer
			chanceRicochet -=85;
			break;
		case CHAR_TEX_TILE:
			DesiredDistance = 5.0f; // 5 units in hammer
			chanceRicochet -=70;
			break;
		case CHAR_TEX_COMPUTER:
			DesiredDistance = 5.0f; // 5 units in hammer
			chanceRicochet -=80;
			break;
		case CHAR_TEX_GLASS:
			DesiredDistance = 8.0f; // maximum 8 units in hammer.
			chanceRicochet -=30;
			break;
		case CHAR_TEX_VENT:
			DesiredDistance = 4.0f; // 4 units in hammer and no more(!)
			chanceRicochet -=90;
			break;
		case CHAR_TEX_METAL:
			DesiredDistance = 1.0f; // 1.5 units in hammer. We cannot penetrate a really 'fat' metal wall. Corners are good.
			chanceRicochet -=90;
			break;
		case CHAR_TEX_PLASTIC:
			DesiredDistance = 8.0f; // 8 units in hammer: Plastic can more
			chanceRicochet -=60;
			break;
		case CHAR_TEX_BLOODYFLESH:
			DesiredDistance = 18.0f; // 18 units in hammer , Just like butter :D
			chanceRicochet -=30;
			break;
		case CHAR_TEX_FLESH:
			DesiredDistance = 18.0f; // 18 units in hammer
			chanceRicochet -=30;
			break;
		case CHAR_TEX_DIRT:
			DesiredDistance = 6.0f; // 6 units in hammer: >4 cm of plaster can be penetrated
			chanceRicochet -=60;
			break;
		default:
			DesiredDistance = 2.0f;
			break;
	}

	float flActualDamage = g_pGameRules->GetAmmoDamage( p_eInfictor, ptr.m_pEnt, bulletinfo.m_iAmmoType );
	DesiredDistance += (flActualDamage/8);

	chanceRicochet = chanceRicochet / 5;

	//DesiredDistance *= acsmod_bullet_penetration_ratio.GetFloat();
 
	Vector	reflect;
	float fldot = m_vecDirShooting.Dot( ptr.plane.normal );						//Getting angles from lasttrace
 
	bool bMustDoRico = (fldot > -MAX_RICO_DOT_ANGLE && GetBulletSpeedRatio() > MIN_RICO_SPEED_PERC && random->RandomInt(0,chanceRicochet)<=1 ); // We can't do richochet when bullet has lowest speed
 
	/*if (sv_bullet_unrealricochet.GetBool() && p_eInfictor->IsPlayer()) //Cheat is only for player,yet =)
		bMustDoRico = true;*/	
 
 	#ifdef GAME_DLL
		//Cancel making dust underwater:
		if(!m_bWasInWater)
		{
			UTIL_ImpactTrace( &ptr, GetDamageType() );
			MakeImpact( ptr, GetAmmoTypeIndex() );
		}
	#endif

	if ( bMustDoRico )
	{
		if(!sv_bullet_unrealricochet.GetBool())
		{
			if(gpGlobals->maxClients == 1) //Use more simple for multiplayer
			{
				m_flBulletSpeed *= (1.0f / -fldot) * random->RandomFloat(0.005,0.1);
			}
			else
			{
				m_flBulletSpeed *= (1.0f / -fldot) * 0.025;
			}
		}
		else
		{
			m_flBulletSpeed *= 0.9;
		}
 
		reflect = m_vecDirShooting + ( ptr.plane.normal * ( fldot*-2.0f ) );	//reflecting
 
		if(gpGlobals->maxClients == 1 && !sv_bullet_unrealricochet.GetBool()) //Use more simple for multiplayer
		{
			reflect[0] += random->RandomFloat( fldot, -fldot );
			reflect[1] += random->RandomFloat( fldot, -fldot );
			reflect[2] += random->RandomFloat( fldot, -fldot );
		}

		g_pEffects->Ricochet( ptr.endpos, reflect );
 
		m_flEntryDensity *= 0.2;
 
		m_vecDirShooting = reflect;
 
		m_vecOrigin = ptr.endpos+m_vecDirShooting;//(ptr.endpos + m_vecDirShooting*1.1) + m_vecDirShooting * m_flBulletSpeed;
	}
	else
	{
		
		Vector	testPos = ptr.endpos + ( m_vecDirShooting * DesiredDistance );

		CEffectData	data;

		data.m_vNormal = ptr.plane.normal;
		data.m_vOrigin = ptr.endpos;

		trace_t	penetrationTrace;

		// Re-trace as if the bullet had passed right through
		UTIL_TraceLine( testPos, ptr.endpos, MASK_SHOT, m_pIgnoreList, &penetrationTrace );

		AbsEntry = ptr.endpos;
		AbsExit  = penetrationTrace.endpos;

		float flPenetrationDistance = VectorLength(AbsEntry - AbsExit);
		
		//if (flPenetrationDistance > DesiredDistance || ptr.IsDispSurface() || bHasInteracted) //|| !bulletinfo.m_bMustPenetrate
		if ( ( penetrationTrace.startsolid || ptr.fraction == 0.0f || penetrationTrace.fraction == 1.0f ) || ptr.IsDispSurface() || bHasInteracted) //|| !bulletinfo.m_bMustPenetrate
		{
			bStuckInWall = true;
			
#ifdef GAME_DLL
			if(g_debug_bullets.GetBool())
			{
				NDebugOverlay::Line( AbsEntry, AbsExit, 255, 0, 0, true, 10.0f );
 
				if(!ptr.IsDispSurface())
					DevMsg("Cannot penetrate surface, The distance(%f) > %f \n",flPenetrationDistance,DesiredDistance);
				else
					DevMsg("Displacement penetration was tempolary disabled\n");
			}
#endif
			return false;
		}
		else
		{
				MakeImpact( penetrationTrace, GetAmmoTypeIndex() );
				UTIL_ImpactTrace( &penetrationTrace, GetDamageType() ); // On exit

				//bHasInteracted = true;
 
				#ifdef GAME_DLL
					if(g_debug_bullets.GetBool())
					{			
						NDebugOverlay::Line( AbsEntry, AbsExit, 0, 255, 0, true, 10.0f );
					}
				#endif
 
				if(gpGlobals->maxClients == 1) //Use more simple for multiplayer
				{
						m_vecDirShooting[0] += (random->RandomFloat( -flPenetrationDistance, flPenetrationDistance ))*0.03;
						m_vecDirShooting[1] += (random->RandomFloat( -flPenetrationDistance, flPenetrationDistance ))*0.03;
						m_vecDirShooting[2] += (random->RandomFloat( -flPenetrationDistance, flPenetrationDistance ))*0.03;
 
						VectorNormalize(m_vecDirShooting);
				}
 
				m_vecOrigin = AbsExit+m_vecDirShooting;
		}

		float passWallSpeedDecreaseMultiplier = 10.0f;

		m_flBulletSpeed -= flPenetrationDistance * (m_flEntryDensity * passWallSpeedDecreaseMultiplier) / sv_bullet_speed_modifier.GetFloat();
	}

	/*if(BulletManager()->BulletStopSpeed()==ONE_HIT_MODE)
	{
		return false;
	}*/
	return true;
}
 
//-----------------------------------------------------------------------------
// analog of HandleShotImpactingWater
//-----------------------------------------------------------------------------
bool CSimulatedBullet::WaterHit(const Vector &vecStart,const Vector &vecEnd)
{
	trace_t	waterTrace;
	// Trace again with water enabled
	UTIL_TraceLine( vecStart, vecEnd, (MASK_SHOT|CONTENTS_WATER|CONTENTS_SLIME), m_pIgnoreList, &waterTrace );
 
	// See if this is the point we entered
	if ( ( enginetrace->GetPointContents( waterTrace.endpos - Vector(0,0,0.1f) ) & (CONTENTS_WATER|CONTENTS_SLIME) ) == 0 )
		return false;
 
	int	nMinSplashSize = GetAmmoDef()->MinSplashSize(GetAmmoTypeIndex()) * (1.5 - GetBulletSpeedRatio());
	int	nMaxSplashSize = GetAmmoDef()->MaxSplashSize(GetAmmoTypeIndex()) * (1.5 - GetBulletSpeedRatio()); //High speed - small splash
 
	CEffectData	data;
	data.m_vOrigin = waterTrace.endpos;
	data.m_vNormal = waterTrace.plane.normal;
	data.m_flScale = random->RandomFloat( nMinSplashSize, nMaxSplashSize );
	if ( waterTrace.contents & CONTENTS_SLIME )
	{
		data.m_fFlags |= FX_WATER_IN_SLIME;
	}
	DispatchEffect( "gunshotsplash", data );
	return true;
}
 
 
 
//==================================================
// Purpose:	Entity impact procedure
//==================================================
void CSimulatedBullet::EntityImpact(trace_t &ptr)
{
	if(bStuckInWall)
		return;
 
	if (ptr.m_pEnt != NULL)
	{
		//Hit inflicted once to avoid perfomance errors
 
		if(!p_eInfictor->IsPlayer())
		{
			if (ptr.m_pEnt->IsPlayer())
			{
				if (m_pIgnoreList->ShouldHitEntity(ptr.m_pEnt,MASK_SHOT))
				{
					m_pIgnoreList->AddEntityToIgnore(ptr.m_pEnt);
				}
				else
				{
					return;
				}
			}
		}else{
		#ifdef GAME_DLL
			//Hitmarker !
			if(acsmod_hitmarker.GetBool())
			{
				// check to see if we hit an NPC
				if ( ptr.m_pEnt->IsNPC() )
				{
					p_eInfictor->DrawHitmarker();
				}
			}
		#endif
		}

 
		if(ptr.m_pEnt == m_hLastHit)
			return;
 
		m_hLastHit = ptr.m_pEnt;
 
		float flActualDamage = g_pGameRules->GetAmmoDamage( p_eInfictor, ptr.m_pEnt, bulletinfo.m_iAmmoType );
		float flActualForce  = bulletinfo.m_flDamageForceScale; 
 
		/*if (!ptr.m_pEnt->IsPlayer() || !ptr.m_pEnt->IsNPC())
		{
			//To make it more reallistic
			float fldot = m_vecDirShooting.Dot( ptr.plane.normal );	 
			//We affecting damage by angle. If we has lower angle of reflection, doing lower damage.
			//flActualDamage *= -fldot;
			flActualForce  *= -fldot;
			//But we not doing it on the players or NPC's
		}*/
 
		flActualDamage *= GetBulletSpeedRatio(); //And also affect damage by speed modifications
		flActualForce  *= GetBulletSpeedRatio(); //Slow bullet - bad force...

	#ifdef GAME_DLL
		if(g_debug_bullets.GetBool())
			DevMsg("Hit: %s, Damage: %f, Force: %f \n",ptr.m_pEnt->GetClassname(),flActualDamage,flActualForce);
	#endif
		CTakeDamageInfo dmgInfo( p_eInfictor, bulletinfo.m_pAttacker, flActualDamage, GetDamageType() );
 
		CalculateBulletDamageForce( &dmgInfo, bulletinfo.m_iAmmoType, m_vecDirShooting, ptr.endpos );
		dmgInfo.ScaleDamageForce( flActualForce );
		dmgInfo.SetAmmoType( bulletinfo.m_iAmmoType );
 
		ptr.m_pEnt->DispatchTraceAttack( dmgInfo, bulletinfo.m_vecDirShooting, &ptr );
 
	#ifdef GAME_DLL
		ApplyMultiDamage(); //It's requried
 
		if ( GetAmmoDef()->Flags(GetAmmoTypeIndex()) & AMMO_FORCE_DROP_IF_CARRIED )
		{
			// Make sure if the player is holding this, he drops it
			Pickup_ForcePlayerToDropThisObject( ptr.m_pEnt );		
		}
	#endif
	}
}
 
 
//==================================================
// Purpose:	Simulates all bullets every centisecond
//==================================================
#ifndef CLIENT_DLL
void CBulletManager::Think(void)
#else
void CBulletManager::ClientThink(void)
#endif
{
	unsigned short iNext=0;
	for ( unsigned short i=g_Bullets.Head(); i != g_Bullets.InvalidIndex(); i=iNext )
	{
		iNext = g_Bullets.Next( i );
		if ( !g_Bullets[i]->SimulateBullet() )
			RemoveBullet( i );
	}
	if(g_Bullets.Head()!=g_Bullets.InvalidIndex())
	{
#ifdef CLIENT_DLL
		SetNextClientThink( gpGlobals->curtime + 0.01f );
#else
		SetNextThink( gpGlobals->curtime + 0.01f );
#endif
	}
}
 
//==================================================
// Purpose:	Called by sv_bullet_stop_speed callback to keep track of resources
//==================================================
void CBulletManager::UpdateBulletStopSpeed(void)
{
	m_iBulletStopSpeed = 100;
}
 
#ifndef CLIENT_DLL
void CBulletManager::SendTraceAttackToTriggers( const CTakeDamageInfo &info, const Vector& start, const Vector& end, const Vector& dir )
{
	TraceAttackToTriggers(info,start,end,dir);
}
#endif
 
 
//==================================================
// Purpose:	Add bullet to linked list
//			Handle lag compensation with "prebullet simulation"
// Output:	Bullet's index (-1 for error)
//==================================================
int CBulletManager::AddBullet(CSimulatedBullet *pBullet)
{
	if (pBullet->GetAmmoTypeIndex() == -1)
	{
		Msg("ERROR: Undefined ammo type!\n");
		return -1;
	}
	unsigned short index = g_Bullets.AddToTail(pBullet);
#ifdef CLIENT_DLL
	DevMsg( "Client Bullet Created (%i)\n", index);
	if(g_Bullets.Count()==1)
	{
		SetNextClientThink(gpGlobals->curtime + 0.01f);
	}
#else
	if(pBullet->bulletinfo.m_flLatency !=0.0f)
		pBullet->SimulateBullet(); //Pre-simulation
 
	if(g_Bullets.Count()==1)
	{
		SetNextThink(gpGlobals->curtime + 0.01f);
	}
#endif

#ifdef GAME_DLL
	if(g_debug_bullets.GetBool())
		DevMsg( "Bullet Created (%i) LagCompensation %f\n", index, pBullet->bulletinfo.m_flLatency );
#endif
	return index;
}
 
 
//==================================================
// Purpose:	Remove the bullet at index from the linked list
// Output:	Next index
//==================================================
void CBulletManager::RemoveBullet(int index)
{
	g_Bullets.Next(index);
#ifdef CLIENT_DLL
	DevMsg("Client ");
#endif
#ifdef GAME_DLL
	if(g_debug_bullets.GetBool())
		DevMsg( "Bullet Removed (%i)\n", index );
#endif
	g_Bullets.Remove(index);
	if(g_Bullets.Count()==0)
	{
#ifdef CLIENT_DLL
		SetNextClientThink( TICK_NEVER_THINK );
#else
		SetNextThink( TICK_NEVER_THINK );
#endif
	}
}