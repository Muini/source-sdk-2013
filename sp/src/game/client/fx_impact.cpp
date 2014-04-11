//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//
#include "cbase.h"
#include "decals.h"
#include "materialsystem/imaterialvar.h"
#include "IEffects.h"
#include "fx.h"
#include "fx_impact.h"
#include "view.h"
#ifdef TF_CLIENT_DLL
#include "cdll_util.h"
#endif
#include "engine/IStaticPropMgr.h"
#include "c_impact_effects.h"
#include "tier0/vprof.h"
#include "dlight.h"
#include "IEfx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar  r_drawflecks( "r_drawflecks", "1" );
extern ConVar r_drawmodeldecals;

ImpactSoundRouteFn g_pImpactSoundRouteFn = NULL;

//==========================================================================================================================
// RAGDOLL ENUMERATOR
//==========================================================================================================================
CRagdollEnumerator::CRagdollEnumerator( Ray_t& shot, int iDamageType )
{
	m_rayShot = shot;
	m_iDamageType = iDamageType;
	m_bHit = false;
}

IterationRetval_t CRagdollEnumerator::EnumElement( IHandleEntity *pHandleEntity )
{
	C_BaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( pHandleEntity->GetRefEHandle() );
	if ( pEnt == NULL )
		return ITERATION_CONTINUE;

	C_BaseAnimating *pModel = static_cast< C_BaseAnimating * >( pEnt );

	// If the ragdoll was created on this tick, then the forces were already applied on the server
	if ( pModel == NULL || WasRagdollCreatedOnCurrentTick( pEnt ) )
		return ITERATION_CONTINUE;

	IPhysicsObject *pPhysicsObject = pModel->VPhysicsGetObject();
	if ( pPhysicsObject == NULL )
		return ITERATION_CONTINUE;

	trace_t tr;
	enginetrace->ClipRayToEntity( m_rayShot, MASK_SHOT, pModel, &tr );

	if ( tr.fraction < 1.0 )
	{
		pModel->ImpactTrace( &tr, m_iDamageType, NULL );
		m_bHit = true;

		//FIXME: Yes?  No?
		return ITERATION_STOP;
	}

	return ITERATION_CONTINUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool FX_AffectRagdolls( Vector vecOrigin, Vector vecStart, int iDamageType )
{
	// don't do this when lots of ragdolls are simulating
	if ( s_RagdollLRU.CountRagdolls(true) > 1 )
		return false;
	Ray_t shotRay;
	shotRay.Init( vecStart, vecOrigin );

	CRagdollEnumerator ragdollEnum( shotRay, iDamageType );
	partition->EnumerateElementsAlongRay( PARTITION_CLIENT_RESPONSIVE_EDICTS, shotRay, false, &ragdollEnum );

	return ragdollEnum.Hit();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void RagdollImpactCallback( const CEffectData &data )
{
	FX_AffectRagdolls( data.m_vOrigin, data.m_vStart, data.m_nDamageType );
}

DECLARE_CLIENT_EFFECT( "RagdollImpact", RagdollImpactCallback );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool Impact( Vector &vecOrigin, Vector &vecStart, int iMaterial, int iDamageType, int iHitbox, C_BaseEntity *pEntity, trace_t &tr, int nFlags, int maxLODToDecal )
{
	VPROF( "Impact" );

	Assert ( pEntity );

	// Clear out the trace
	memset( &tr, 0, sizeof(trace_t));
	tr.fraction = 1.0f;

	// Setup our shot information
	Vector shotDir = vecOrigin - vecStart;
	float flLength = VectorNormalize( shotDir );
	Vector traceExt;
	VectorMA( vecStart, flLength + 8.0f, shotDir, traceExt );

	// Attempt to hit ragdolls
	
	bool bHitRagdoll = false;
	
	if ( !pEntity->IsClientCreated() )
	{
		bHitRagdoll = FX_AffectRagdolls( vecOrigin, vecStart, iDamageType );
	}

	if ( (nFlags & IMPACT_NODECAL) == 0 )
	{
		const char *pchDecalName = GetImpactDecal( pEntity, iMaterial, iDamageType );
		int decalNumber = decalsystem->GetDecalIndexForName( pchDecalName );
		if ( decalNumber == -1 )
			return false;

		bool bSkipDecal = false;

#ifdef TF_CLIENT_DLL
		// Don't show blood decals if we're filtering them out (Pyro Goggles)
		if ( IsLocalPlayerUsingVisionFilterFlags( TF_VISION_FILTER_PYRO ) || UTIL_IsLowViolence() )
		{
			if ( V_strstr( pchDecalName, "Flesh" ) )
			{
				bSkipDecal = true;
			}
		}
#endif

		if ( !bSkipDecal )
		{
			if ( (pEntity->entindex() == 0) && (iHitbox != 0) )
			{
				staticpropmgr->AddDecalToStaticProp( vecStart, traceExt, iHitbox - 1, decalNumber, true, tr );
			}
			else if ( pEntity )
			{
				// Here we deal with decals on entities.
				pEntity->AddDecal( vecStart, traceExt, vecOrigin, iHitbox, decalNumber, true, tr, maxLODToDecal );
			}
		}
	}
	else
	{
		// Perform the trace ourselves
		Ray_t ray;
		ray.Init( vecStart, traceExt );

		if ( (pEntity->entindex() == 0) && (iHitbox != 0) )
		{
			// Special case for world entity with hitbox (that's a static prop)
			ICollideable *pCollideable = staticpropmgr->GetStaticPropByIndex( iHitbox - 1 ); 
			enginetrace->ClipRayToCollideable( ray, MASK_SHOT, pCollideable, &tr );
		}
		else
		{
			if ( !pEntity )
				return false;

			enginetrace->ClipRayToEntity( ray, MASK_SHOT, pEntity, &tr );
		}
	}

	// If we found the surface, emit debris flecks
	bool bReportRagdollImpacts = (nFlags & IMPACT_REPORT_RAGDOLL_IMPACTS) != 0;
	if ( ( tr.fraction == 1.0f ) || ( bHitRagdoll && !bReportRagdollImpacts ) )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
char const *GetImpactDecal( C_BaseEntity *pEntity, int iMaterial, int iDamageType )
{
	char const *decalName;
	if ( !pEntity )
	{
		decalName = "Impact.Concrete";
	}
	else
	{
		decalName = pEntity->DamageDecal( iDamageType, iMaterial );
	}

	// See if we need to offset the decal for material type
	return decalsystem->TranslateDecalForGameMaterial( decalName, iMaterial );
}

//-----------------------------------------------------------------------------
// Purpose: Perform custom effects based on the Decal index
//-----------------------------------------------------------------------------
static ConVar cl_new_impact_effects( "cl_new_impact_effects", "1" );

struct ImpactEffect_t
{
	const char *m_pName;
	const char *m_pNameNoFlecks;
};

static ImpactEffect_t s_pImpactEffect[26] = 
{
	{ "impact_antlion",		NULL },							// CHAR_TEX_ANTLION
	{ "red_blood_smoke",	NULL },							// CHAR_TEX_BLOODYFLESH	
	{ "impact_concrete",	NULL },							// CHAR_TEX_CONCRETE		
	{ "impact_dirt",		NULL },							// CHAR_TEX_DIRT			
	{ NULL,					NULL },							// CHAR_TEX_EGGSHELL		
	{ "red_blood_smoke",	NULL },							// CHAR_TEX_FLESH			
	{ NULL,					NULL },							// CHAR_TEX_GRATE			
	{ "blood_impact_yellow_01",	NULL },						// CHAR_TEX_ALIENFLESH		
	{ NULL,					NULL },							// CHAR_TEX_CLIP			
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ "impact_dirt",		NULL },							// CHAR_TEX_PLASTIC		
	{ "impact_metal",		NULL },							// CHAR_TEX_METAL			
	{ "impact_dirt",		NULL },							// CHAR_TEX_SAND			
	{ NULL,					NULL },							// CHAR_TEX_FOLIAGE		
	{ "impact_computer",	NULL },							// CHAR_TEX_COMPUTER		
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ NULL,					NULL },							// CHAR_TEX_SLOSH			
	{ "impact_concrete",	NULL },							// CHAR_TEX_TILE			
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ "impact_metal",		NULL },							// CHAR_TEX_VENT			
	{ "impact_wood",		NULL },							// CHAR_TEX_WOOD			
	{ NULL,					NULL },							// CHAR_TEX_UNUSED		
	{ "impact_glass",		NULL },							// CHAR_TEX_GLASS			
	{ "warp_shield_impact", NULL },							// CHAR_TEX_WARPSHIELD	
};

static void SetImpactControlPoint( CNewParticleEffect *pEffect, int nPoint, const Vector &vecImpactPoint, const Vector &vecForward, C_BaseEntity *pEntity )
{
	Vector vecImpactY, vecImpactZ;
	VectorVectors( vecForward, vecImpactY, vecImpactZ ); 
	vecImpactY *= -1.0f;

	pEffect->SetControlPoint( nPoint, vecImpactPoint );
	pEffect->SetControlPointOrientation( nPoint, vecForward, vecImpactY, vecImpactZ );
	pEffect->SetControlPointEntity( nPoint, pEntity );
}

static void PerformNewCustomEffects( const Vector &vecOrigin, trace_t &tr, const Vector &shotDir, int iMaterial, int iScale, int nFlags )
{
	bool bNoFlecks = !r_drawflecks.GetBool();
	if ( !bNoFlecks )
	{
		bNoFlecks = ( ( nFlags & FLAGS_CUSTIOM_EFFECTS_NOFLECKS ) != 0  );
	}

	// Compute the impact effect name
	const ImpactEffect_t &effect = s_pImpactEffect[ iMaterial - 'A' ];
	const char *pImpactName = effect.m_pName;
	if ( bNoFlecks && effect.m_pNameNoFlecks )
	{
		pImpactName = effect.m_pNameNoFlecks;
	}
	if ( !pImpactName )
		return;

	CSmartPtr<CNewParticleEffect> pEffect = CNewParticleEffect::Create( NULL, pImpactName );
	if ( !pEffect->IsValid() )
		return;

	Vector	vecReflect;
	float	flDot = DotProduct( shotDir, tr.plane.normal );
	VectorMA( shotDir, -2.0f * flDot, tr.plane.normal, vecReflect );

	Vector vecShotBackward;
	VectorMultiply( shotDir, -1.0f, vecShotBackward );

	Vector vecImpactPoint = ( tr.fraction != 1.0f ) ? tr.endpos : vecOrigin;
	Assert( VectorsAreEqual( vecOrigin, tr.endpos, 1e-1 ) );

	SetImpactControlPoint( pEffect.GetObject(), 0, vecImpactPoint, tr.plane.normal, tr.m_pEnt ); 
	SetImpactControlPoint( pEffect.GetObject(), 1, vecImpactPoint, vecReflect,		tr.m_pEnt ); 
	SetImpactControlPoint( pEffect.GetObject(), 2, vecImpactPoint, vecShotBackward,	tr.m_pEnt ); 
	pEffect->SetControlPoint( 3, Vector( iScale, iScale, iScale ) );
	if ( pEffect->m_pDef->ReadsControlPoint( 4 ) )
	{
		Vector vecColor;
		GetColorForSurface( &tr, &vecColor );
		pEffect->SetControlPoint( 4, vecColor );
	}
}

class C_CShower : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_CShower, C_BaseEntity ); // generic entity class macro
	DECLARE_CLIENTCLASS(); // this is a client representation of a server class 
};
//Link a global entity name to this class (name used in Hammer etc.)
LINK_ENTITY_TO_CLASS( spark_shower, C_CShower );
 
// Link data table DT_MyEntity to client class and map variables (RecvProps)
// DO NOT create this in the header! Put it in the main CPP file.
IMPLEMENT_CLIENTCLASS_DT( C_CShower, DT_CShower, CShower )
END_RECV_TABLE()

void PerformCustomEffects( const Vector &vecOrigin, trace_t &tr, const Vector &shotDir, int iMaterial, int iScale, int nFlags )
{
	// Throw out the effect if any of these are true
	if ( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) )
		return;

	bool bNoFlecks = !r_drawflecks.GetBool();
	if ( !bNoFlecks )
	{
		bNoFlecks = ( ( nFlags & FLAGS_CUSTIOM_EFFECTS_NOFLECKS ) != 0  );
	}

	if ( cl_new_impact_effects.GetInt() )
	{
		PerformNewCustomEffects( vecOrigin, tr, shotDir, iMaterial, iScale, nFlags ); //Particles Impacts Effects

		if ( ( iMaterial == CHAR_TEX_METAL ) || ( iMaterial == CHAR_TEX_VENT ) )
		{
			Vector	reflect;
			float	dot = shotDir.Dot( tr.plane.normal );
			reflect = shotDir + ( tr.plane.normal * ( dot*-2.0f ) );
			
			if( random->RandomInt(0,20)==0 )
			{
				//Dynamic light
				dlight_t *dl = effects->CL_AllocDlight ( 0 );

				VectorCopy (vecOrigin, dl->origin);

				dl->origin = vecOrigin;
				dl->radius = random->RandomInt( 24, 24 ); // radius of flash
				dl->decay = dl->radius / 0.04f;  // original radius is 0.05f; **needed distance from a wall**
				dl->die = gpGlobals->curtime + 0.08f;  // FIX ME: time causes somewhat weird lighting please adjust
				dl->color.r = 255;
				dl->color.g = 255;
				dl->color.b = 255;
				dl->color.exponent = 5;
			}

			reflect[0] += random->RandomFloat( -0.2f, 0.2f );
			reflect[1] += random->RandomFloat( -0.2f, 0.2f );
			reflect[2] += random->RandomFloat( -0.2f, 0.2f );

			if ( random->RandomInt(0,1)==0 )
				FX_MetalSpark( vecOrigin, reflect, tr.plane.normal, iScale );

			Vector	origin=vecOrigin;

			if ( random->RandomInt(0,10)==0 )
			{
				QAngle vecAngles;
				VectorAngles( -shotDir, vecAngles );
				DispatchParticleEffect( "metal_impact_bullet", vecOrigin, vecAngles );
			}

			if ( random->RandomInt(0,20)==0 )
			{
				QAngle vecAngles;
				VectorAngles( -shotDir, vecAngles );
				DispatchParticleEffect( "metal_spark_shower", vecOrigin, vecAngles );
			}
			//FX_ConcussiveExplosion ( origin, reflect ); Tres Rare : Grosse impact plein de spark
			//FX_MetalScrape( origin, reflect ); Rare Giclé d'étincelle fine et longue
			//FX_EnergySplash( vecOrigin, tr.plane.normal ); Jamais Bouclier Combine
			//FX_MicroExplosion ( origin, reflect ); Jamais Comme Bouclier Combine mais jaune et moche
			//FX_Explosion ( origin, reflect, CHAR_TEX_METAL ); Jamais Large Sparks with Large smoke
			if ( random->RandomInt(0,20)==0 )
				FX_MetalScrape( origin, reflect );

			if( random->RandomInt(0,4)==0 )
			{
				Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );
				g_pEffects->Sparks( offset );
			}
		}
		else if ( iMaterial == CHAR_TEX_COMPUTER )
		{
			Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );

			g_pEffects->Sparks( offset );
		}
		else if ( iMaterial == CHAR_TEX_WARPSHIELD )
		{
			QAngle vecAngles;
			VectorAngles( -shotDir, vecAngles );
			DispatchParticleEffect( "warp_shield_impact", vecOrigin, vecAngles );
		}
		else if ( ( iMaterial == CHAR_TEX_FLESH ) || ( iMaterial == CHAR_TEX_BLOODYFLESH ) )
		{
			Vector	reflect;
			float	dot = shotDir.Dot( tr.plane.normal );
			reflect = shotDir + ( tr.plane.normal * ( dot*-2.0f ) );
			Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );

			QAngle vecAngles;
			VectorAngles( shotDir, vecAngles ); //ShotDir invert
			DispatchParticleEffect( "blood_impact_red_dead", vecOrigin, vecAngles );

			UTIL_TraceLine ( offset, offset + reflect * 64,  MASK_SOLID_BRUSHONLY, null, COLLISION_GROUP_NONE, &tr);
			UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_RED );

		}
		else if ( ( iMaterial == CHAR_TEX_CONCRETE ) || ( iMaterial == CHAR_TEX_TILE ) )
		{
			if( random->RandomInt(0,6)==0 )
			{
				Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );
				g_pEffects->Sparks( offset );
			}
		}
	}
	else
	{
		// Cement and wood have dust and flecks
		if ( ( iMaterial == CHAR_TEX_CONCRETE ) || ( iMaterial == CHAR_TEX_TILE ) )
		{
			FX_DebrisFlecks( vecOrigin, &tr, iMaterial, iScale, bNoFlecks );
			FX_DustImpact( vecOrigin, &tr, iScale );

			Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );

			g_pEffects->Sparks( offset );
		}
		else if ( iMaterial == CHAR_TEX_WOOD )
		{
			FX_DebrisFlecks( vecOrigin, &tr, iMaterial, iScale, bNoFlecks );
		}
		else if ( ( iMaterial == CHAR_TEX_DIRT ) || ( iMaterial == CHAR_TEX_SAND ) )
		{
			FX_DustImpact( vecOrigin, &tr, iScale );
		}
		else if ( iMaterial == CHAR_TEX_ANTLION )
		{
			FX_AntlionImpact( vecOrigin, &tr );
			FX_DustImpact( vecOrigin, &tr, iScale );
			
			Vector	reflect;
			float	dot = shotDir.Dot( tr.plane.normal );
			reflect = shotDir + ( tr.plane.normal * ( dot*-2.0f ) );

			reflect[0] += random->RandomFloat( -0.2f, 0.2f );
			reflect[1] += random->RandomFloat( -0.2f, 0.2f );
			reflect[2] += random->RandomFloat( -0.2f, 0.2f );

			FX_MetalSpark( vecOrigin, reflect, tr.plane.normal, iScale );

			Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );
			UTIL_TraceLine ( offset, offset + reflect * 64,  MASK_SOLID_BRUSHONLY, null, COLLISION_GROUP_NONE, &tr);
			UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_YELLOW );
		}
		else if ( ( iMaterial == CHAR_TEX_METAL ) || ( iMaterial == CHAR_TEX_VENT ) )
		{
			Vector	reflect;
			float	dot = shotDir.Dot( tr.plane.normal );
			reflect = shotDir + ( tr.plane.normal * ( dot*-2.0f ) );

			reflect[0] += random->RandomFloat( -0.2f, 0.2f );
			reflect[1] += random->RandomFloat( -0.2f, 0.2f );
			reflect[2] += random->RandomFloat( -0.2f, 0.2f );

			FX_MetalSpark( vecOrigin, reflect, tr.plane.normal, iScale );

			reflect[0] += random->RandomFloat( -0.5f, 0.5f );
			reflect[1] += random->RandomFloat( -0.5f, 0.5f );
			reflect[2] += random->RandomFloat( -0.5f, 0.5f );

			FX_MetalSpark( vecOrigin, reflect, tr.plane.normal, iScale );

			reflect[0] += random->RandomFloat( -5.0f, 5.0f );
			reflect[1] += random->RandomFloat( -5.0f, 5.0f );
			reflect[2] += random->RandomFloat( -5.0f, 5.0f );

			FX_MetalSpark( vecOrigin, reflect, tr.plane.normal, iScale );

			Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );

			g_pEffects->Sparks( offset );

			QAngle vecAngles;
			VectorAngles( -shotDir, vecAngles );
			FX_Smoke( offset, vecAngles, random->RandomFloat( 1.5f, 3.0f ), 3, NULL, 200 );
		}
		else if ( iMaterial == CHAR_TEX_COMPUTER )
		{
			Vector	reflect;
			float	dot = shotDir.Dot( tr.plane.normal );
			reflect = shotDir + ( tr.plane.normal * ( dot*-2.0f ) );

			reflect[0] += random->RandomFloat( -0.6f, 0.6f );
			reflect[1] += random->RandomFloat( -0.6f, 0.6f );
			reflect[2] += random->RandomFloat( -0.6f, 0.6f );

			FX_MetalSpark( vecOrigin, reflect, tr.plane.normal, iScale );

			Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );

			g_pEffects->Sparks( offset );

			QAngle vecAngles;
			VectorAngles( -shotDir, vecAngles );
			FX_Smoke( offset, vecAngles, random->RandomFloat( 1.0f, 3.0f ), 3, NULL, 150 );
		}
		else if ( iMaterial == CHAR_TEX_WARPSHIELD )
		{
			Vector	reflect;
			float	dot = shotDir.Dot( tr.plane.normal );
			reflect = shotDir + ( tr.plane.normal * ( dot*-2.0f ) );

			reflect[0] += random->RandomFloat( -0.6f, 0.6f );
			reflect[1] += random->RandomFloat( -0.6f, 0.6f );
			reflect[2] += random->RandomFloat( -0.6f, 0.6f );

			FX_MetalSpark( vecOrigin, reflect, tr.plane.normal, iScale );

			Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );

			g_pEffects->Sparks( offset );

			QAngle vecAngles;
			VectorAngles( -shotDir, vecAngles );
			FX_Smoke( offset, vecAngles, random->RandomFloat( 1.0f, 3.0f ), 3, NULL, 150 );
		}
		else if ( ( iMaterial == CHAR_TEX_FLESH ) || ( iMaterial == CHAR_TEX_BLOODYFLESH ) )
		{
			Vector	reflect;
			float	dot = shotDir.Dot( tr.plane.normal );
			reflect = shotDir + ( tr.plane.normal * ( dot*-2.0f ) );
			Vector	offset = vecOrigin + ( tr.plane.normal * 1.0f );

			//FX_Blood( offset, reflect, 64, 0, 0, 250 );

			QAngle vecAngles;
			VectorAngles( -shotDir, vecAngles );
			FX_Smoke( offset, vecAngles, random->RandomFloat( 0.5f, 2.0f ), 3, NULL, 100 );

			UTIL_TraceLine ( offset, offset + reflect * 64,  MASK_SOLID_BRUSHONLY, null, COLLISION_GROUP_NONE, &tr);
			UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_RED );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play a sound for an impact. If tr contains a valid hit, use that. 
//			If not, use the passed in origin & surface.
//-----------------------------------------------------------------------------
void PlayImpactSound( CBaseEntity *pEntity, trace_t &tr, Vector &vecServerOrigin, int nServerSurfaceProp )
{
	VPROF( "PlayImpactSound" );
	surfacedata_t *pdata;
	Vector vecOrigin;

	// If the client-side trace hit a different entity than the server, or
	// the server didn't specify a surfaceprop, then use the client-side trace 
	// material if it's valid.
	if ( tr.DidHit() && (pEntity != tr.m_pEnt || nServerSurfaceProp == 0) )
	{
		nServerSurfaceProp = tr.surface.surfaceProps;
	}
	pdata = physprops->GetSurfaceData( nServerSurfaceProp );
	if ( tr.fraction < 1.0 )
	{
		vecOrigin = tr.endpos;
	}
	else
	{
		vecOrigin = vecServerOrigin;
	}

	// Now play the esound
	if ( pdata->sounds.bulletImpact )
	{
		const char *pbulletImpactSoundName = physprops->GetString( pdata->sounds.bulletImpact );
		
		if ( g_pImpactSoundRouteFn )
		{
			g_pImpactSoundRouteFn( pbulletImpactSoundName, vecOrigin );
		}
		else
		{
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound( filter, NULL, pbulletImpactSoundName, pdata->soundhandles.bulletImpact, &vecOrigin );
		}

		return;
	}

#ifdef _DEBUG
	Msg("***ERROR: PlayImpactSound() on a surface with 0 bulletImpactCount!\n");
#endif //_DEBUG
}


void SetImpactSoundRoute( ImpactSoundRouteFn fn )
{
	g_pImpactSoundRouteFn = fn;
}


//-----------------------------------------------------------------------------
// Purpose: Pull the impact data out
// Input  : &data - 
//			*vecOrigin - 
//			*vecAngles - 
//			*iMaterial - 
//			*iDamageType - 
//			*iHitbox - 
//			*iEntIndex - 
//-----------------------------------------------------------------------------
C_BaseEntity *ParseImpactData( const CEffectData &data, Vector *vecOrigin, Vector *vecStart, 
	Vector *vecShotDir, short &nSurfaceProp, int &iMaterial, int &iDamageType, int &iHitbox )
{
	C_BaseEntity *pEntity = data.GetEntity( );
	*vecOrigin = data.m_vOrigin;
	*vecStart = data.m_vStart;
	nSurfaceProp = data.m_nSurfaceProp;
	iDamageType = data.m_nDamageType;
	iHitbox = data.m_nHitBox;

	*vecShotDir = (*vecOrigin - *vecStart);
	VectorNormalize( *vecShotDir );

	// Get the material from the surfaceprop
	surfacedata_t *psurfaceData = physprops->GetSurfaceData( data.m_nSurfaceProp );
	iMaterial = psurfaceData->game.material;

	return pEntity;
}

