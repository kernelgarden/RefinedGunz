#include "stdafx.h"

#include "ZGame.h"
#include <windows.h>

#include "MZFileSystem.h"
#include "RealSpace2.h"
#include "FileInfo.h"
#include "MDebug.h"
#include "MBlobArray.h"
#include "MObject.h"
#include "ZConsole.h"
#include "MCommandLogFrame.h"
#include "ZInterface.h"
#include "ZGameInterface.h"
#include "ZApplication.h"
#include "ZCommandTable.h"
#include "ZPost.h"
#include "ZPostLocal.h"
#include "MStrEx.h"
#include "MMatchItem.h"
#include "ZEffectManager.h"
#include "ZEffectBillboard.h"
#include "Config.h"
#include "MProfiler.h"
#include "MMatchItem.h"
#include "ZScreenEffectManager.h"
#include "RParticleSystem.h"
#include "RDynamicLight.h"
#include "ZConfiguration.h"
#include "ZLoading.h"
#include "Physics.h"
#include "zeffectflashbang.h"
#include "ZInitialLoading.h"
#include "RealSoundEffect.h"
#include "RLenzFlare.h"
#include "ZWorldItem.h"
#include "ZMyInfo.h"
#include "ZNetCharacter.h"
#include "ZSecurity.h"
#include "ZStencilLight.h"
#include "ZMap.h"
#include "ZEffectStaticMesh.h"
#include "ZEffectAniMesh.h"
#include "ZGameAction.h"
#include "ZSkyBox.h"
#include "ZFile.h"
#include "ZObject.h"
#include "ZCharacter.h"
#include "ZMapDesc.h"

#include "MXml.h"
#include "ZGameClient.h"
#include "MUtil.h"
#include "RMeshMgr.h"
#include "RVisualMeshMgr.h"
#include "RMaterialList.h"
#include "RAnimationMgr.h"
#include "ZCamera.h"
#include "Mint4R2.h"
#include "ZItemDesc.h"

#include "MMath.h"
#include "ZQuest.h"
#include "MMatchUtil.h"
#include "ZReplay.h"
#include "ZRuleBerserker.h"
#include "ZApplication.h"
#include "ZGameConst.h"

#include "ZRuleDuel.h"
#include "ZMyCharacter.h"

#include "RGMain.h"
#include "Portal.h"
#include "Rules.h"
#include "Hitboxes.h"
#include "ZRuleSkillmap.h"
#include "VoiceChat.h"
#include <random>
#include "HitRegistration.h"
#include "stuff.h"

_USING_NAMESPACE_REALSPACE2

#define ENABLE_CHARACTER_COLLISION

struct RSnowParticle : public RParticle , CMemPoolSm<RSnowParticle>
{
	virtual bool Update(float fTimeElapsed);
};


bool RSnowParticle::Update(float fTimeElapsed)
{
	RParticle::Update(fTimeElapsed);

	if (position.z <= -1000.0f) return false;
	return true;
}

class ZSnowTownParticleSystem
{
private:
	RParticles*		m_pParticles[3];
	bool IsSnowTownMap()
	{
		if (!_strnicmp(ZGetGameClient()->GetMatchStageSetting()->GetMapName(), "snow", 4)) return true;

		return false;
	}
public:
	void Update(float fDeltaTime)
	{
		if (!IsSnowTownMap()) return;

#define SNOW_PARTICLE_COUNT_PER_SECOND		400

		int nSnowParticleCountPerSec = SNOW_PARTICLE_COUNT_PER_SECOND;
		switch (ZGetConfiguration()->GetVideo()->nEffectLevel)
		{
		case Z_VIDEO_EFFECT_HIGH:	break;
		case Z_VIDEO_EFFECT_NORMAL:	nSnowParticleCountPerSec = nSnowParticleCountPerSec / 4; break;
		case Z_VIDEO_EFFECT_LOW:	nSnowParticleCountPerSec = nSnowParticleCountPerSec / 8; break;
		default: nSnowParticleCountPerSec = 0; break;
		}

		int nCount = min(nSnowParticleCountPerSec * fDeltaTime,20);
		for(int i=0;i<nCount;i++)
		{
			RParticle *pp=new RSnowParticle();
			pp->ftime=0;
			pp->position = rvector(RandomNumber(-8000.0f, 8000.0f), RandomNumber(-8000.0f, 8000.0f), 1500.0f);
			pp->velocity = rvector(RandomNumber(-40.0f, 40.0f), RandomNumber(-40.0f, 40.0f), RandomNumber(-150.0f, -250.0f));
			pp->accel=rvector(0,0,-5.f);

			int particle_index = RandomNumber(0, 2);
			if (m_pParticles[particle_index]) m_pParticles[particle_index]->push_back(pp);
		}
	}
	void Create()
	{
		if (!IsSnowTownMap()) return;

		for (int i = 0; i < 3; i++)
		{
			m_pParticles[i] = NULL;
		}

		m_pParticles[0] = RGetParticleSystem()->AddParticles("sfx/water_splash.bmp", 25.0f);
		m_pParticles[1] = RGetParticleSystem()->AddParticles("sfx/water_splash.bmp", 10.0f);
		m_pParticles[2] = RGetParticleSystem()->AddParticles("sfx/water_splash.bmp", 5.0f);
	}
	void Destroy()
	{
		if (!IsSnowTownMap()) return;

		m_pParticles[0]->Clear();
		m_pParticles[1]->Clear();
		m_pParticles[2]->Clear();
	}
};

static ZSnowTownParticleSystem g_SnowTownParticleSystem;

ZGame*	g_pGame = NULL;
float	g_fFOV = DEFAULT_FOV;
float	g_fFarZ = DEFAULT_FAR_Z;

extern sCharacterLight g_CharLightList[NUM_LIGHT_TYPE];

MUID tempUID(0, 0);
MUID g_MyChrUID(0, 0);

#define IsKeyDown(key) ((GetAsyncKeyState(key) & 0x8000)!=0)

bool IsMyCharacter(ZObject* pObject)
{
	if ((g_pGame) && ((ZObject*)g_pGame->m_pMyCharacter == pObject)) return true;
	return false;
}

void TestCreateEffect(int nEffIndex)
{
	float fDist = RandomNumber(0.0f, 100.0f);
	rvector vTar = rvector(RandomNumber(0.0f, 100.0f), RandomNumber(0.0f, 100.0f), RandomNumber(0.0f, 100.0f));
	rvector vPos = g_pGame->m_pMyCharacter->GetPosition();
	vPos.x += RandomNumber(0.0f, 100.0f);
	vPos.y += RandomNumber(0.0f, 100.0f);
	vPos.z += RandomNumber(0.0f, 100.0f);

	rvector vTarNormal = -RealSpace2::RCameraDirection;

	vTarNormal = rvector(RandomNumber(0.0f, 100.0f), RandomNumber(0.0f, 100.0f), RandomNumber(0.0f, 100.0f));


	ZCharacter* pCharacter = g_pGame->m_pMyCharacter;
	ZEffectManager* pEM = ZGetEffectManager();

	switch (nEffIndex)
	{
	case 0:
		pEM->AddReBirthEffect(vPos);
		break;
	case 1:
		pEM->AddLandingEffect(vPos, vTarNormal);
		break;
	case 2:
		pEM->AddGrenadeEffect(vPos, vTarNormal);
		break;
	case 3:
		pEM->AddRocketEffect(vPos, vTarNormal);
		break;
	case 4:
		pEM->AddRocketSmokeEffect(vPos);
		break;
	case 5:
		pEM->AddSwordDefenceEffect(vPos,-vTarNormal);
		break;
	case 6:
		pEM->AddSwordWaveEffect(pCharacter->GetUID(), vPos, pCharacter->GetDirection());
		break;
	case 7:
		pEM->AddSwordUppercutDamageEffect(vPos, pCharacter->GetUID());
		break;
	case 8:
		pEM->AddBulletMark(vPos, vTarNormal);
		break;
	case 9:
		pEM->AddShotgunEffect(vPos, vPos, vTar, pCharacter);
		break;
	case 10:
		pEM->AddBloodEffect(vPos, vTarNormal);
		break;
	case 11:
		for (int i = 0; i < SEM_End; i++)
			pEM->AddSlashEffect(vPos, vTarNormal, i);
		break;
	case 12:
		pEM->AddSlashEffectWall(vPos, vTarNormal,0);
		break;
	case 13:
		pEM->AddLightFragment(vPos, vTarNormal);
		break;
	case 14:
		pEM->AddDashEffect(vPos, vTarNormal, pCharacter);
		break;
	case 15:
		pEM->AddSmokeEffect(vPos);
		break;
	case 16:
		pEM->AddSmokeGrenadeEffect(vPos);
		break;
	case 17:
		pEM->AddGrenadeSmokeEffect(vPos, 1.0f, 0.5f, 123);
		break;
	case 18:
		pEM->AddWaterSplashEffect(vPos, vPos);
		break;
	case 19:
		pEM->AddWorldItemEatenEffect(vPos);
		break;
	case 20:
		pEM->AddCharacterIcon(pCharacter, 0);
		break;
	case 21:
		pEM->AddCommanderIcon(pCharacter, 0);
		break;
	case 22:
		pEM->AddChatIcon(pCharacter);
		break;
	case 23:
		pEM->AddLostConIcon(pCharacter);
		break;
	case 24:
		pEM->AddChargingEffect(pCharacter);
		break;
	case 25:
		pEM->AddChargedEffect(pCharacter);
		break;
	case 26:
		pEM->AddTrackFire(vPos);
		pEM->AddTrackFire(vPos);
		pEM->AddTrackFire(vPos);
		pEM->AddTrackFire(vPos);
		break;
	case 27:

		ZEffectWeaponEnchant* pEWE = pEM->GetWeaponEnchant(ZC_ENCHANT_FIRE);

		if(pEWE) {
			float fSIze = 105.f / 100.f;
			rvector vScale = rvector(0.6f*fSIze,0.6f*fSIze,0.9f*fSIze);
			pEWE->SetUid( pCharacter->GetUID() );
			pEWE->SetAlignType(1);
			pEWE->SetScale(vScale);
			pEWE->Draw(GetGlobalTimeMS());
		}

		break;
	}
}

float CalcActualDamage(ZObject* pAttacker, ZObject* pVictim, float fDamage)
{
	if (g_pGame->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_BERSERKER)
	{
		ZRuleBerserker* pRule = (ZRuleBerserker*)g_pGame->GetMatch()->GetRule();
		if ((pAttacker) && (pAttacker != pVictim) && (pAttacker->GetUID() == pRule->GetBerserkerUID()))
		{
			return fDamage * BERSERKER_DAMAGE_RATIO;			
		}
	}

	return fDamage;
}

void TestCreateEffects()
{
	int nCount = 100;

	for (int i = 0; i < nCount; i++)
	{
		int nEffIndex = RandomNumber(25, 28);
		TestCreateEffect(nEffIndex);
	}
}

ZCharacterManager* ZGetCharacterManager()
{
	if (g_pGame == NULL) return NULL;
	return &g_pGame->m_CharacterManager;
}

ZObjectManager* ZGetObjectManager()
{
	if (g_pGame == NULL) return NULL;
	return &g_pGame->m_ObjectManager;
}

ZGame::ZGame()
{
	g_pGame	= this;

	m_bShowWireframe = false;
	m_pMyCharacter = NULL;

	m_bReserveObserver = false;

	memset(m_nLastTime, 0, sizeof(DWORD) * ZLASTTIME_MAX);

	m_fTime = 0.f;
	m_nReadyState = ZGAME_READYSTATE_INIT;
	m_pParticles = NULL;
	m_render_poly_cnt = 0;
	m_nReservedObserverTime = 0;
	m_nSpawnTime = 0;
	m_t = 0;

	m_bRecording = false;
	m_pReplayFile = NULL;

	m_bReplaying = false;
	m_bShowReplayInfo = true;
	m_bSpawnRequested = false;

	CancelSuicide();
}

ZGame::~ZGame()
{
	g_SnowTownParticleSystem.Destroy();
	RSnowParticle::Release();
}

bool ZGame::Create(MZFileSystem *pfs, ZLoadingProgress *pLoading )
{
	mlog("ZGame::Create() begin , type = %d\n",ZGetGameClient()->GetMatchStageSetting()->GetGameType());

	SetReadyState(ZGAME_READYSTATE_INIT);

#ifdef _QUEST
	{
		ZGetQuest()->OnGameCreate();
	}
#endif

	if (ZGetApplication()->GetLaunchMode()!=ZApplication::ZLAUNCH_MODE_STANDALONE_AI &&
		ZGetGameTypeManager()->IsQuestDerived(ZGetGameClient()->GetMatchStageSetting()->GetGameType())) {
		for (int i = 0; i < ZGetQuest()->GetGameInfo()->GetMapSectorCount(); i++)
		{
			MQuestMapSectorInfo* pSecInfo = ZGetQuest()->GetSectorInfo(ZGetQuest()->GetGameInfo()->GetMapSectorID(i));
			ZGetWorldManager()->AddWorld(pSecInfo->szTitle);
#ifdef _DEBUG
			mlog("map(%s)\n", pSecInfo ? pSecInfo->szTitle : "null");
#endif
		}
	}else{
		ZGetWorldManager()->AddWorld(ZGetGameClient()->GetMatchStageSetting()->GetMapName());
	}

	if(!ZGetWorldManager()->LoadAll(pLoading))
		return false;

	ZGetWorldManager()->SetCurrent(0);

	mlog("ZGame::Create() :: ReloadAllAnimation Begin \n");
	ZGetMeshMgr()->ReloadAllAnimation();
	mlog("ZGame::Create() :: ReloadAllAnimation End \n");

	if (ZGetGameClient()->IsForcedEntry())
	{
		ZPostRequestPeerList(ZGetGameClient()->GetPlayerUID(), ZGetGameClient()->GetStageUID());
	}

	g_fFOV = DEFAULT_FOV;

	rvector dir = GetMapDesc()->GetWaitCamDir();
	rvector pos = GetMapDesc()->GetWaitCamPos();
	rvector up(0,0,1);
	RSetCamera(pos, pos+dir, up);



	int nModelID = -1;

	m_Match.Create();
	
	D3DMATERIAL9 mtrl;
	ZeroMemory( &mtrl, sizeof(D3DMATERIAL9) );

	mtrl.Diffuse.r = 1.0f;
	mtrl.Diffuse.g = 1.0f;
	mtrl.Diffuse.b = 1.0f;
	mtrl.Diffuse.a = 1.0f;

	mtrl.Ambient.r = 1.0f;
	mtrl.Ambient.g = 1.0f;
	mtrl.Ambient.b = 1.0f;
	mtrl.Ambient.a = 1.0f;

	RGetDevice()->SetMaterial( &mtrl );

	m_fTime=0.f;
	m_bReserveObserver = false;

	
#ifdef _BIRDSOUND
	ZApplication::GetSoundEngine()->OpenMusic(BGMID_BATTLE);
	ZApplication::GetSoundEngine()->PlayMusic();
#else
	ZApplication::GetSoundEngine()->OpenMusic(BGMID_BATTLE, pfs);
	ZApplication::GetSoundEngine()->PlayMusic();
#endif

	m_CharacterManager.Clear();
	m_ObjectManager.Clear();

	mlog("ZGame::Create() m_CharacterManager.Clear done \n");

	m_pMyCharacter = (ZMyCharacter*)m_CharacterManager.Add(ZGetGameClient()->GetPlayerUID(), rvector(0.0f, 0.0f, 0.0f),true);

	
	{
		g_CharLightList[GUN].fLife	= 300;
		g_CharLightList[GUN].fRange = 100;
		g_CharLightList[GUN].iType	= GUN;
		g_CharLightList[GUN].vLightColor.x	= 5.0f;
		g_CharLightList[GUN].vLightColor.y	= 1.0f;
		g_CharLightList[GUN].vLightColor.z	= 1.0f;

		g_CharLightList[SHOTGUN].fLife	= 1000;
		g_CharLightList[SHOTGUN].fRange = 150;
		g_CharLightList[SHOTGUN].iType	= SHOTGUN;
		g_CharLightList[SHOTGUN].vLightColor.x	= 6.0f;
		g_CharLightList[SHOTGUN].vLightColor.y	= 1.3f;
		g_CharLightList[SHOTGUN].vLightColor.z	= 1.3f;

		g_CharLightList[CANNON].fLife	= 1300;
		g_CharLightList[CANNON].fRange	= 200;
		g_CharLightList[CANNON].iType	= CANNON;
		g_CharLightList[CANNON].vLightColor.x	= 7.0f;
		g_CharLightList[CANNON].vLightColor.y	= 1.3f;
		g_CharLightList[CANNON].vLightColor.z	= 1.3f;
	}

	ZGetFlashBangEffect()->SetBuffer();
	ZGetScreenEffectManager()->SetGuageExpFromMyInfo();

#ifndef _BIRDSOUND
	ZGetSoundEngine()->SetEffectVolume( Z_AUDIO_EFFECT_VOLUME );
	ZGetSoundEngine()->SetMusicVolume( Z_AUDIO_BGM_VOLUME );
#endif

	// Net init
	ZApplication::ResetTimer();
	m_GameTimer.Reset();
	ZSetupDataChecker_Game(&m_DataChecker);
	
	ZGetInitialLoading()->SetPercentage( 100.f );
	ZGetInitialLoading()->Draw( MODE_DEFAULT, 0 , true );


#ifndef _BIRDSOUND
	for (auto& AS : GetWorld()->GetBsp()->GetAmbSndList())
	{
		if( AS.itype & AS_AABB)
			ZGetSoundEngine()->SetAmbientSoundBox(AS.szSoundName,
				AS.min, AS.max,
				(AS.itype&AS_2D)?true:false );
		else if( AS.itype & AS_SPHERE )
			ZGetSoundEngine()->SetAmbientSoundSphere(AS.szSoundName,
				AS.center, AS.radius,
				(AS.itype&AS_2D)?true:false );
	}
#endif

	m_pMyCharacter->GetStatus()->nLoadingPercent = 100;
	ZPostLoadingComplete(ZGetGameClient()->GetPlayerUID(), 100);

	ZPostStageEnterBattle(ZGetGameClient()->GetPlayerUID(), ZGetGameClient()->GetStageUID());

	char tmpbuf[128];
	_strtime( tmpbuf );

	mlog("ZGame Created ( %s )\n",tmpbuf);

	ZGetGameInterface()->GetCamera()->SetLookMode(ZCAMERA_DEFAULT);


	g_SnowTownParticleSystem.Create();


	return true;
}

void ZGame::Destroy()
{
	StopRecording();

	g_SnowTownParticleSystem.Destroy();

	SetClearColor(0);

	mlog("ZGame::Destroy begin\n");

	ZGetGameClient()->AgentDisconnect();

	ZApplication::GetSoundEngine()->StopMusic();
	ZApplication::GetSoundEngine()->CloseMusic();

	mlog("ZGame::Destroy ZApplication::GetSoundEngine()->CloseMusic()\n");

#ifdef _QUEST
	{
		ZGetQuest()->OnGameDestroy();
	}
#endif

	m_Match.Destroy();

	mlog("ZGame::Destroy m_Match.Destroy \n");

	if (ZGetGameClient()->IsForcedEntry())
	{
		ZGetGameClient()->ReleaseForcedEntry();

		ZGetGameInterface()->SerializeStageInterface();
	}

	ZPostStageLeaveBattle(ZGetGameClient()->GetPlayerUID(), ZGetGameClient()->GetStageUID());

	ReleaseFlashBangEffect();

	mlog("ZGame::Destroy SAFE_DELETE(m_pSkyBox) \n");

	RGetLenzFlare()->Clear();

	m_WeaponManager.Clear();
#ifdef _WORLD_ITEM_
	ZGetWorldItemManager()->Clear();
#endif

#ifdef _BIRDSOUND

#else
	ZGetSoundEngine()->ClearAmbientSound();
#endif

	m_ObserverCommandList.Destroy();
	m_ReplayCommandList.Destroy();
	m_DelayedCommandList.Destroy();

	ZGetEffectManager()->Clear();
	ZGetScreenEffectManager()->Clear();

	ZGetWorldManager()->Clear();

	char tmpbuf[128];
	_strtime( tmpbuf );

	mlog("ZGame Destroyed ( %s )\n",tmpbuf);
}

bool ZGame::CreateMyCharacter(const MTD_CharInfo& CharInfo)
{
	if (!m_pMyCharacter) return false;

	m_pMyCharacter->Create(CharInfo);
	m_pMyCharacter->SetVisible(true);

	ZGetEffectManager()->AddBerserkerIcon(m_pMyCharacter);

	return true;
}

bool ZGame::CheckGameReady()
{
	if (GetReadyState() == ZGAME_READYSTATE_RUN) {
		return true;
	} else if (GetReadyState() == ZGAME_READYSTATE_INIT) {
		SetReadyState(ZGAME_READYSTATE_WAITSYNC);
		ZPostRequestTimeSync(GetTickTime());
		return false;
	} else if (GetReadyState() == ZGAME_READYSTATE_WAITSYNC) {
		return false;
	}
	return false;
}

void ZGame::OnGameResponseTimeSync(unsigned int nLocalTimeStamp, unsigned int nGlobalTimeSync)
{
	ZGameTimer* pTimer = GetGameTimer();
	int nCurrentTick = pTimer->GetGlobalTick();
	int nDelay = (nCurrentTick - nLocalTimeStamp)/2;
	int nOffset = (int)nGlobalTimeSync - (int)nCurrentTick + nDelay;
	
	pTimer->SetGlobalOffset(nOffset);

	SetReadyState(ZGAME_READYSTATE_RUN);
}

void ZGame::Update(float fElapsed)
{
	if (CheckGameReady() == false) {
		OnCameraUpdate(fElapsed);
		return;
	}

	GetWorld()->Update(fElapsed);

	ZGetEffectManager()->Update(fElapsed);
	ZGetScreenEffectManager()->UpdateEffects();

	m_GameTimer.UpdateTick(GetGlobalTimeMS());
	m_fTime+=fElapsed;

	m_ObjectManager.Update(fElapsed);

	if(m_pMyCharacter && !m_bReplaying)
	{
		PostBasicInfo();
		PostHPAPInfo();
		PostPeerPingInfo();
		PostSyncReport();
	}

	CheckMyCharDead(fElapsed);
	if (m_bReserveObserver)
	{
		OnReserveObserver();
	}

	UpdateCombo();

	g_SnowTownParticleSystem.Update(fElapsed);

#ifdef _WORLD_ITEM_
	ZGetWorldItemManager()->update();
#endif
	m_Match.Update(fElapsed);

	if(m_bReplaying) 
		OnReplayRun();
	if(ZGetGameInterface()->GetCombatInterface()->GetObserverMode())
		OnObserverRun();

	ProcessDelayedCommand();



#ifdef _QUEST
	{
		ZGetQuest()->OnGameUpdate(fElapsed);
	}
#endif

	RGetParticleSystem()->Update(fElapsed);

	if(Z_VIDEO_DYNAMICLIGHT)
		ZGetStencilLight()->Update();

	OnCameraUpdate(fElapsed);
}


void ZGame::OnCameraUpdate(float Elapsed)
{
	if (m_Match.GetRoundState() == MMATCH_ROUNDSTATE_PREPARE)
	{
		rvector dir = GetMapDesc()->GetWaitCamDir();
		rvector pos = GetMapDesc()->GetWaitCamPos();
		rvector up(0,0,1);

		RSetCamera(pos, pos+dir, up);
	}
	else
	{
		ZGetGameInterface()->GetCamera()->Update(Elapsed);
	}
}

void ZGame::CheckMyCharDead(float fElapsed)
{
	if(!m_pMyCharacter || m_pMyCharacter->IsDie()) return;

	MUID uidAttacker = MUID(0,0);

	if (m_pMyCharacter->GetPosition().z < DIE_CRITICAL_LINE)
	{
		if (ZGetGameClient()->GetMatchStageSetting()->GetGameType() == MMATCH_GAMETYPE_SKILLMAP)
		{
			((ZRuleSkillmap *)GetMatch()->GetRule())->OnFall();
		}
		else
		{
			uidAttacker = m_pMyCharacter->GetLastThrower();

			ZObject *pAttacker = ZGetObjectManager()->GetObject(uidAttacker);
			if (pAttacker == NULL || !IsAttackable(pAttacker, m_pMyCharacter))
			{
				uidAttacker = ZGetMyUID();
				pAttacker = m_pMyCharacter;
			}
			m_pMyCharacter->OnDamaged(pAttacker, m_pMyCharacter->GetPosition(), ZD_FALLING, MWT_NONE, m_pMyCharacter->GetHP());
			ZChatOutput(ZMsg(MSG_GAME_FALL_NARAK));
		}
	}

	if (ZGetGameClient()->GetMatchStageSetting()->GetNetcode() == NetcodeType::ServerBased)
		return;

	if ((m_pMyCharacter->IsDie() == false) && (m_pMyCharacter->GetHP() <= 0))
	{
		if (uidAttacker == MUID(0,0) && m_pMyCharacter->GetLastAttacker() != MUID(0,0)) 
			uidAttacker = m_pMyCharacter->GetLastAttacker();

		if(GetMatch()->GetRoundState() == MMATCH_ROUNDSTATE_FINISH) {

			m_pMyCharacter->ActDead();
			m_pMyCharacter->Die();
			return;
		}

		ZPostDie(uidAttacker);

		if (! ZGetGameTypeManager()->IsQuestDerived(ZGetGameClient()->GetMatchStageSetting()->GetGameType()))
		{
			ZPostGameKill(uidAttacker);
		}
		else
		{
			ZPostQuestGameKill();
		}
	}
}

void ZGame::OnPreDraw()
{
	__BP(19,"ZGame::sub1");

#ifdef PORTAL
	if(!g_pPortal->ForceProjection())
#endif
		RSetProjection(g_fFOV, DEFAULT_NEAR_Z, g_fFarZ);

	bool bTrilinear=RIsTrilinear();

	RGetDevice()->SetSamplerState( 0, D3DSAMP_MAGFILTER , D3DTEXF_LINEAR);
	RGetDevice()->SetSamplerState( 0, D3DSAMP_MINFILTER , D3DTEXF_LINEAR);
	RGetDevice()->SetSamplerState( 0, D3DSAMP_MIPFILTER , bTrilinear ? D3DTEXF_LINEAR : D3DTEXF_NONE );
	RGetDevice()->SetSamplerState( 1, D3DSAMP_MAGFILTER , D3DTEXF_LINEAR);
	RGetDevice()->SetSamplerState( 1, D3DSAMP_MINFILTER , D3DTEXF_LINEAR);
	RGetDevice()->SetSamplerState( 1, D3DSAMP_MIPFILTER , bTrilinear ? D3DTEXF_LINEAR : D3DTEXF_NONE );

	if(m_bShowWireframe ) {
		RGetDevice()->SetRenderState( D3DRS_FILLMODE , D3DFILL_WIREFRAME );
		RGetDevice()->SetRenderState( D3DRS_LIGHTING, FALSE );
		GetWorld()->SetFog(false);
	}
	else {
		RGetDevice()->SetRenderState( D3DRS_FILLMODE ,  D3DFILL_SOLID );
		GetWorld()->SetFog(true);
	}

	GetWorld()->GetBsp()->SetWireframeMode(m_bShowWireframe);

	rmatrix initmat;
	D3DXMatrixIdentity(&initmat);
	RGetDevice()->SetTransform( D3DTS_WORLD, &initmat );
	RGetDevice()->SetRenderState(D3DRS_ALPHABLENDENABLE, false );
	RGetDevice()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	RGetDevice()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
	RGetDevice()->SetRenderState(D3DRS_ZWRITEENABLE, true );

	LPDIRECT3DDEVICE9 pd3dDevice=RGetDevice();
	pd3dDevice->SetTexture(0,NULL);
	pd3dDevice->SetTexture(1,NULL);
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2 );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
	pd3dDevice->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1 );

	pd3dDevice->SetRenderState(D3DRS_CULLMODE  ,D3DCULL_CW);
	pd3dDevice->SetRenderState(D3DRS_NORMALIZENORMALS , TRUE );

	if(m_pMyCharacter)
	{
		if( ZGetConfiguration()->GetVideo()->bDynamicLight )
		{
			rvector pos = m_pMyCharacter->GetPosition();
			RGetDynamicLightManager()->SetPosition( pos );
		}
	}

	__EP(19);
}

int g_debug_render_mode = 0;

extern MDrawContextR2* g_pDC;

void ZGame::Draw()
{
	__BP(20,"ZGame::Draw");

	RRESULT isOK=RIsReadyToRender();

	if(isOK==R_NOTREADY)
	{
		__EP(20);
		return ;
	}

#ifdef PORTAL
	g_pPortal->PreDraw();
#endif

	if ( m_bSuicide && ( GetGlobalTimeMS() > m_dwReservedSuicideTime))
		ZGetGameClient()->RequestGameSuicide();


	OnPreDraw();

	rmatrix _mat;
	RGetDevice()->GetTransform(D3DTS_WORLD, &_mat);

	__BP(21,"ZGame::Draw::DrawWorld");
	GetWorld()->Draw();
	__EP(21);

	ZMapDesc* pMapDesc = GetMapDesc();

	if( pMapDesc ) {
		pMapDesc->DrawMapDesc();
	}

	if (m_Match.GetRoundState() != MMATCH_ROUNDSTATE_PREPARE)
	{
		__BP(22,"ZGame::Draw::DrawCharacters");

		m_ObjectManager.Draw();

		__EP(22);

		m_render_poly_cnt = RealSpace2::g_poly_render_cnt;
	}

	RGetDevice()->SetTransform(D3DTS_WORLD, &_mat);

	ZGetWorldItemManager()->Draw(0,GetWorld()->GetWaterHeight(),GetWorld()->IsWaterMap());

	m_WeaponManager.Render();

	__BP(50,"ZGame::DrawObjects");

	GetWorld()->GetBsp()->DrawObjects();

	__EP(50);

	__BP(17,"ZGame::Draw::Reflection");

	GetWorld()->GetWaters()->Render();

	__EP(17);

	if (m_Match.GetRoundState() != MMATCH_ROUNDSTATE_PREPARE)
	{
		__BP(23,"ZGame::Draw::DrawWeapons and effects");

		ZGetEffectManager()->Draw(GetGlobalTimeMS());

		__EP(23);
	}

#ifdef _WORLD_ITEM_
	__BP(34,"ZGame::Draw::ZGetWorldItemManager");

		ZGetWorldItemManager()->Draw(1,GetWorld()->GetWaterHeight(),GetWorld()->IsWaterMap());

	__EP(34);
#endif

	__BP(35,"ZGame::Draw::RGetParticleSystem");

	RGetParticleSystem()->Draw();

	__EP(35);

	__BP(36,"ZGame::Draw::LenzFlare");

	if( RReadyLenzFlare())
	{
		RGetLenzFlare()->Render( RCameraPosition, GetWorld()->GetBsp());
	}

#ifdef PORTAL
	if(!g_pPortal->ForceProjection())
#endif
		RSetProjection(g_fFOV, DEFAULT_NEAR_Z, g_fFarZ);
	RSetFog(FALSE);

	__EP(36);

	__BP(37,"ZGame::Draw::FlashBangEffect");

	if( IsActivatedFlashBangEffect() )
	{
		ShowFlashBangEffect();
	}

	__BP(505, "ZGame::Draw::RenderStencilLight");
	if(Z_VIDEO_DYNAMICLIGHT)
		ZGetStencilLight()->Render();
	__EP(505);

	__EP(37);

	__BP(38,"ZGame::Draw::DrawGameMessage");

	m_Match.OnDrawGameMessage();

	__EP(38);

	__EP(20);

#ifdef PORTAL
	g_pPortal->PostDraw();
#endif

	g_RGMain->OnGameDraw();
}

void ZGame::DrawDebugInfo()
{
	char szTemp[256] = "";
	int n = 20;
	g_pDC->SetColor(MCOLOR(0xFFffffff));

	for (ZCharacterManager::iterator itor = m_CharacterManager.begin(); itor != m_CharacterManager.end(); ++itor)
	{
		ZCharacter* pCharacter = (*itor).second;
		sprintf_safe(szTemp, "Pos = %6.3f %6.3f %6.3f  Dir = %6.3f %6.3f %6.3f", pCharacter->m_Position.x,
				pCharacter->m_Position.y, pCharacter->m_Position.z, 
				pCharacter->m_Direction.x, pCharacter->m_Direction.y, pCharacter->m_Direction.z);
		g_pDC->Text(20,n,szTemp);
		n += 15;

		RVisualMesh* pVMesh = pCharacter->m_pVMesh;

		AniFrameInfo* pAniLow = pVMesh->GetFrameInfo(ani_mode_lower);
		AniFrameInfo* pAniUp  = pVMesh->GetFrameInfo(ani_mode_upper);

		sprintf_safe(szTemp,"%s frame down %d / %d ",pAniLow->m_pAniSet->GetName() , pAniLow->m_nFrame , pAniLow->m_pAniSet->GetMaxFrame());
		g_pDC->Text(20,n,szTemp);
		n+= 15;

		if( pAniUp->m_pAniSet ) 
		{
			sprintf_safe(szTemp,"%s frame up %d / %d ",pAniUp->m_pAniSet->GetName(),pAniUp->m_nFrame,pAniUp->m_pAniSet->GetMaxFrame());
			g_pDC->Text(20,n,szTemp);
			n+= 15;
		}
	}
}


void ZGame::Draw(MDrawContextR2 &dc)
{
}

void ZGame::ParseReservedWord(char* pszDest, const char* pszSrc)
{
	char szSrc[256];
	char szWord[256];

	strcpy_safe(szSrc, pszSrc);

	char szOut[256];	ZeroMemory(szOut, 256);
	int nOutOffset = 0;

	char* pszNext = szSrc;
	while( *pszNext != NULL ) {
		pszNext = MStringCutter::GetOneArg(pszNext, szWord);

		if ( (*szWord == '$') && (_stricmp(szWord, "$player")==0) ) {
			sprintf_safe(szWord, "%d %d", m_pMyCharacter->GetUID().High, m_pMyCharacter->GetUID().Low);
		} else if ( (*szWord == '$') && (_stricmp(szWord, "$target")==0) ) {
			sprintf_safe(szWord, "%d %d", m_pMyCharacter->GetUID().High, m_pMyCharacter->GetUID().Low);
		}

		strcpy(szOut+nOutOffset, szWord);	nOutOffset += (int)strlen(szWord);
		if (*pszNext) { 
			strcpy(szOut+nOutOffset, " ");
			nOutOffset++;
		}
	}
	strcpy(pszDest, szOut);
}

#include "ZMessages.h"

extern bool g_bProfile;

bool IsIgnoreObserverCommand(int nID)
{
	switch(nID) {
		case MC_PEER_PING :
		case MC_PEER_PONG :
		case MC_PEER_OPENED	:
		case MC_MATCH_GAME_RESPONSE_TIMESYNC :
			return false;
	}
	return true;
}

void ZGame::OnCommand_Observer(MCommand* pCommand)
{
	if(!IsIgnoreObserverCommand(pCommand->GetID()))
	{
		OnCommand_Immediate(pCommand);
		return;
	}


	ZObserverCommandItem *pZCommand=new ZObserverCommandItem;
	pZCommand->pCommand=pCommand->Clone();
	pZCommand->fTime=GetTime();
	m_ObserverCommandList.push_back(pZCommand);

	if(pCommand->GetID()==MC_PEER_BASICINFO)
		OnPeerBasicInfo(pCommand,true,false);
}

void ZGame::ProcessDelayedCommand()
{
	for(ZObserverCommandList::iterator i = m_DelayedCommandList.begin(); i != m_DelayedCommandList.end();i++)
	{
		ZObserverCommandItem *pItem = *i;

		if(GetTime() > pItem->fTime) 
		{
			OnCommand_Immediate(pItem->pCommand);
			delete pItem->pCommand;
			delete pItem;
			i = m_DelayedCommandList.erase(i);
			if (i == m_DelayedCommandList.end())
				break;
		}
	}
}

void ZGame::OnReplayRun()
{
	if (ReplayIterator == m_ReplayCommandList.end() && m_bReplaying) {
		m_bReplaying=false;
		EndReplay();
		return;
	}

	while(ReplayIterator != m_ReplayCommandList.end())
	{
		ZObserverCommandItem *pItem = *ReplayIterator;

		if(GetTime() < pItem->fTime)
			return;
		
		OnCommand_Observer(pItem->pCommand);
		ReplayIterator++;
	}
}

void ZGame::OnObserverRun()
{
	while(m_ObserverCommandList.begin() != m_ObserverCommandList.end())
	{
		ZObserverCommandItem *pItem=*m_ObserverCommandList.begin();
		if(GetTime()-pItem->fTime < ZGetGameInterface()->GetCombatInterface()->GetObserver()->GetDelay())
			return;

		m_ObserverCommandList.erase(m_ObserverCommandList.begin());

		if(pItem->pCommand->GetID()==MC_PEER_BASICINFO)
			OnPeerBasicInfo(pItem->pCommand,false,true);
		else
			OnCommand_Immediate(pItem->pCommand);

		delete pItem->pCommand;
		delete pItem;
	}
}

void ZGame::FlushObserverCommands()
{
	while(m_ObserverCommandList.begin() != m_ObserverCommandList.end())
	{
		ZObserverCommandItem *pItem=*m_ObserverCommandList.begin();

		m_ObserverCommandList.erase(m_ObserverCommandList.begin());

		if(pItem->pCommand->GetID()!=MC_PEER_BASICINFO)
			OnCommand_Immediate(pItem->pCommand);

		delete pItem->pCommand;
		delete pItem;
	}
}

bool ZGame::OnCommand(MCommand* pCommand)
{
	if(m_bRecording)
	{
		ZObserverCommandItem *pItem = new ZObserverCommandItem;
		pItem->fTime = m_fTime;
		pItem->pCommand = pCommand->Clone();

		m_ReplayCommandList.push_back(pItem);

	}

	if(ZGetGameInterface()->GetCombatInterface()->GetObserverMode()){
		OnCommand_Observer(pCommand);
		return true;
	}

	if(FilterDelayedCommand(pCommand))
		return true;

	return OnCommand_Immediate(pCommand);
}

// 유저 컬러

bool GetUserGradeIDColor(MMatchUserGradeID gid,MCOLOR& UserNameColor,char* sp_name)
{
	if(gid == MMUG_DEVELOPER) 
	{ 
		UserNameColor = MCOLOR(255, 128, 64); 
		if(sp_name) { 
			strcpy(sp_name,ZMsg(MSG_WORD_DEVELOPER));
		}
		return true; 
	}
	else if(gid == MMUG_ADMIN) {
		UserNameColor = MCOLOR(255, 128, 64); 
		if(sp_name) { 
			strcpy(sp_name,ZMsg(MSG_WORD_ADMIN));
		}
		return true; 
	}

	return false;
}

bool ZGame::GetUserNameColor(MUID uid,MCOLOR& UserNameColor,char* sp_name)
{
	MMatchUserGradeID gid = MMUG_FREE;

	if(m_pMyCharacter->GetUID()==uid) {

		if(ZGetMyInfo()) {

			gid = ZGetMyInfo()->GetUGradeID();
//			gid = MMUG_DEVELOPER;//test

		} else {

			mlog("ZGame::GetUserNameColor ZGetMyInfo==NULL \n");

		}
	}
	else {

		MMatchPeerInfo* pPeer = ZGetGameClient()->FindPeer(uid);

		if(pPeer) {
			 gid = pPeer->CharInfo.nUGradeID;
		}
		
	}

	return GetUserGradeIDColor( gid , UserNameColor ,sp_name );
}

void ZTranslateCommand(MCommand* pCmd, string& strLog)
{
	char szBuf[256]="";

	// 시간
	unsigned long nGlobalClock = ZGetGame()->GetTickTime();
	itoa(nGlobalClock, szBuf, 10);
	strLog = szBuf;
	strLog += ": ";

	// Command
	strLog += pCmd->m_pCommandDesc->GetName();

	// PlayerName
	string strPlayerName;
	MUID uid = pCmd->GetSenderUID();
	ZCharacter* pChar = ZGetCharacterManager()->Find(uid);
	if (pChar)
		strPlayerName = pChar->GetProperty()->szName;
	else
		strPlayerName = "Unknown";

	strLog += " (";
	strLog += strPlayerName;
	strLog += ") ";

	// Params
	string strParams;
	for(int i=0; i<pCmd->GetParameterCount(); i++){
		char szParam[256]="";
		pCmd->GetParameter(i)->GetString(szParam);
		strParams += szParam;
		if (i<pCmd->GetParameterCount()-1)
			strParams += ", ";
	}
	strLog += strParams;
}

void ZLogCommand(MCommand* pCmd)
{
	if (pCmd->GetID() == MC_AGENT_TUNNELING_UDP) {
		return;
	}

	string strCmd;
	ZTranslateCommand(pCmd, strCmd);

	OutputDebugString(strCmd.c_str());
	OutputDebugString("\n");
}

bool ZGame::OnCommand_Immediate(MCommand* pCommand)
{
	if(GameAction.OnCommand(pCommand))
		return true;

	if (OnRuleCommand(pCommand)) return true;

	switch (pCommand->GetID())
	{
	case MC_PEER_RG_SLASH:
	{
		auto it = m_CharacterManager.find(pCommand->GetSenderUID());
		if (it == m_CharacterManager.end())
			break;

		ZCharacter &pSender = *it->second;

		rvector pos, dir;
		int type;

		if (!pCommand->GetParameter(&pos, 0, MPT_VECTOR))
			break;
		if (!pCommand->GetParameter(&dir, 1, MPT_VECTOR))
			break;
		if (!pCommand->GetParameter(&type, 2, MPT_INT))
			break;

		OnPeerSlash(&pSender, pos, dir, type);
	}
		break;
	case MC_PEER_RG_MASSIVE:
	{
		auto it = m_CharacterManager.find(pCommand->GetSenderUID());
		if (it == m_CharacterManager.end())
			break;

		ZCharacter &pSender = *it->second;

		rvector pos, dir;

		if (!pCommand->GetParameter(&pos, 0, MPT_VECTOR))
			break;
		if (!pCommand->GetParameter(&dir, 1, MPT_VECTOR))
			break;

		OnPeerMassive(&pSender, pos, dir);
	}
		break;
#ifdef PORTAL
	case MC_PEER_PORTAL:
	{
		auto it = m_CharacterManager.find(pCommand->GetSenderUID());
		if (it == m_CharacterManager.end())
			break;

		ZCharacter &pSender = *it->second;

		int iPortal;
		rvector pos, normal, up;
		pCommand->GetParameter(&iPortal, 0, MPT_INT);
		pCommand->GetParameter(&pos, 1, MPT_VECTOR);
		pCommand->GetParameter(&normal, 2, MPT_VECTOR);
		pCommand->GetParameter(&up, 3, MPT_VECTOR);
		g_pPortal->CreatePortal(&pSender, iPortal, pos, normal, up);
	}
		break;
#endif
	case MC_PEER_SPEC:
	{
		auto it = m_CharacterManager.find(pCommand->GetSenderUID());
		if (it == m_CharacterManager.end())
			break;

		ZCharacter &pSender = *it->second;

		pSender.SetTeamID(MMT_SPECTATOR);
		pSender.ForceDie();
	}
		break;
	case MC_PEER_COMPLETED_SKILLMAP:
	{
		auto it = m_CharacterManager.find(pCommand->GetSenderUID());
		if (it == m_CharacterManager.end())
			break;

		ZCharacter &pSender = *it->second;

		float fTime;
		char szCourse[64];

		if (!pCommand->GetParameter(&fTime, 0, MPT_FLOAT))
			break;
		if (!pCommand->GetParameter(szCourse, 1, MPT_STR))
			break;

		if (szCourse[0] == 0)
			ZChatOutputF("%s completed the map in %.02f seconds!", pSender.GetUserName(), fTime);
		else
			ZChatOutputF("%s completed course %s in %.02f seconds!", pSender.GetUserName(), szCourse, fTime);
	}
		break;
	case MC_MATCH_RECEIVE_VOICE_CHAT:
	{
		MUID uid;
		if (!pCommand->GetParameter(&uid, 0, MPT_UID))
			break;

		auto it = m_CharacterManager.find(uid);

		if (it == m_CharacterManager.end())
			break;

		auto Char = it->second;

		MCommandParameter* pParam = pCommand->GetParameter(1);
		if (pParam->GetType() != MPT_BLOB) break;
		void* pBlob = pParam->GetPointer();

		g_RGMain->OnReceiveVoiceChat(Char, (unsigned char *)pBlob, pParam->GetSize() - sizeof(int));
	}
		break;
	case MC_PEER_SET_SWORD_COLOR:
	{
		uint32_t Color;
		if (!pCommand->GetParameter(&Color, 0, MPT_UINT))
			break;

		g_RGMain->SetSwordColor(pCommand->GetSenderUID(), Color);
	}
	break;
	case MC_PEER_ANTILEAD_DAMAGE:
	{
		MUID Target;
		u16 Damage;
		float PiercingRatio;
		u8 DamageType;
		u8 WeaponType;

		if (!pCommand->GetParameter(&Target, 0, MPT_UID))
			break;
		if (!pCommand->GetParameter(&Damage, 1, MPT_USHORT))
			break;
		if (!pCommand->GetParameter(&PiercingRatio, 2, MPT_FLOAT))
			break;
		if (!pCommand->GetParameter(&DamageType, 3, MPT_UCHAR))
			break;
		if (!pCommand->GetParameter(&WeaponType, 4, MPT_UCHAR))
			break;

		auto it = m_CharacterManager.find(Target);
		if (it == m_CharacterManager.end())
			break;

		if (it->second != m_pMyCharacter)
			break;

		it = m_CharacterManager.find(pCommand->GetSenderUID());

		auto Attacker = it->second;

		m_pMyCharacter->OnDamaged(Attacker, Attacker->GetPosition(), ZDAMAGETYPE(DamageType), MMatchWeaponType(WeaponType), Damage, PiercingRatio);

		DMLog("Antilead damage %d\n", Damage);
	}
	break;
	case MC_MATCH_DAMAGE:
	{
		MUID uidAttacker;
		u16 Damage;
		float PiercingRatio;
		u8 DamageType;
		u8 WeaponType;

		if (!pCommand->GetParameter(&uidAttacker, 0, MPT_UID)) break;
		if (!pCommand->GetParameter(&Damage, 1, MPT_USHORT)) break;
		if (!pCommand->GetParameter(&PiercingRatio, 2, MPT_FLOAT)) break;
		if (!pCommand->GetParameter(&DamageType, 3, MPT_UCHAR)) break;
		if (!pCommand->GetParameter(&WeaponType, 4, MPT_UCHAR)) break;

		auto it = m_CharacterManager.find(uidAttacker);
		if (it == m_CharacterManager.end())
			break;

		auto Attacker = it->second;

		m_pMyCharacter->OnDamaged(Attacker, Attacker->GetPosition(), ZDAMAGETYPE(DamageType), MMatchWeaponType(WeaponType), Damage, PiercingRatio);

		DMLog("MatchServer damage %d\n", Damage);
	}
	break;
	case MC_PEER_BASICINFO_RG:
	{
		if (pCommand->GetParameter(0)->GetType() != MPT_BLOB)
			break;

		OnPeerNewBasicInfo(pCommand, true, true);
	}
	break;
	case MC_MATCH_STAGE_ENTERBATTLE:
		{	
			unsigned char nParam;
			pCommand->GetParameter(&nParam, 0, MPT_UCHAR);

			MCommandParameter* pParam = pCommand->GetParameter(1);
			if(pParam->GetType()!=MPT_BLOB) break;
			void* pBlob = pParam->GetPointer();

			MTD_PeerListNode* pPeerNode = (MTD_PeerListNode*)MGetBlobArrayElement(pBlob, 0);

			OnStageEnterBattle(MCmdEnterBattleParam(nParam), pPeerNode);
		}
		break;
	case MC_MATCH_STAGE_LEAVEBATTLE:
		{
			MUID uidChar, uidStage;

			pCommand->GetParameter(&uidChar, 0, MPT_UID);
			pCommand->GetParameter(&uidStage, 1, MPT_UID);

			OnStageLeaveBattle(uidChar, uidStage);
		}
		break;
	case MC_MATCH_RESPONSE_PEERLIST:
		{
			MUID uidStage;
			pCommand->GetParameter(&uidStage, 0, MPT_UID);
			MCommandParameter* pParam = pCommand->GetParameter(1);
			if(pParam->GetType()!=MPT_BLOB) break;
			void* pBlob = pParam->GetPointer();
			int nCount = MGetBlobArrayCount(pBlob);
			OnPeerList(uidStage,pBlob, nCount);
		}
		break;
	case MC_MATCH_GAME_ROUNDSTATE:
		{
			MUID uidStage;
			int nRoundState, nRound, nArg;

			pCommand->GetParameter(&uidStage, 0, MPT_UID);
			pCommand->GetParameter(&nRound, 1, MPT_INT);
			pCommand->GetParameter(&nRoundState, 2, MPT_INT);
			pCommand->GetParameter(&nArg, 3, MPT_INT);

			OnGameRoundState(uidStage, nRound, nRoundState, nArg);

			g_pGame->GetMatch()->SetRoundStartTime();
		}
		break;
	case MC_MATCH_GAME_RESPONSE_TIMESYNC: 
		{
			unsigned int nLocalTS, nGlobalTS;
			pCommand->GetParameter(&nLocalTS, 0, MPT_UINT);
			pCommand->GetParameter(&nGlobalTS, 1, MPT_UINT);

			OnGameResponseTimeSync(nLocalTS, nGlobalTS);
		}
		break;
	case MC_MATCH_RESPONSE_SUICIDE:
		{
			int nResult;
			MUID	uidChar;
			pCommand->GetParameter(&nResult, 0, MPT_INT);
			pCommand->GetParameter(&uidChar, 1, MPT_UID);

			if (nResult == MOK)
			{
				OnPeerDie(uidChar, uidChar);
				CancelSuicide();
			}
		}
		break;
	case MC_EVENT_UPDATE_JJANG:
		{
			MUID uidChar;
			bool bJjang;

			pCommand->GetParameter(&uidChar, 0, MPT_UID);
			pCommand->GetParameter(&bJjang, 1, MPT_BOOL);

			OnEventUpdateJjang(uidChar, bJjang);
		}
		break;
	case MC_PEER_CHAT:
		{
			int nTeam = MMT_ALL;
			char szMsg[512];
			memset(szMsg, 0, sizeof(szMsg));

			pCommand->GetParameter(&nTeam, 0, MPT_INT);
			pCommand->GetParameter(szMsg, 1, MPT_STR, sizeof(szMsg) );

			MCOLOR ChatColor = MCOLOR(0xFFD0D0D0);
			const MCOLOR TeamChatColor = MCOLOR(109,207,246);

			MUID uid=pCommand->GetSenderUID();
			ZCharacter *pChar=ZGetCharacterManager()->Find(uid);

			MCOLOR UserNameColor = MCOLOR(190,190,0);

			char sp_name[256];
			bool bSpUser = GetUserNameColor(uid, UserNameColor, sp_name);
/*
			bool GetPureUserName(char* name,char* pure_name)
			{
				//^숫자제거

			}
*/
			if(pChar) 
			{
				int nMyTeam = ZApplication::GetGame()->m_pMyCharacter->GetTeamID();

				// 일반 채팅 말 일때...
				if ( (nTeam == MMT_ALL) || (nTeam == MMT_SPECTATOR))
				{
					if ( !ZGetGameClient()->GetRejectNormalChat() || ( strcmp( pChar->GetUserName(), ZGetMyInfo()->GetCharName()) == 0))
					{
						ZGetSoundEngine()->PlaySound("if_error");
						char szTemp[sizeof(szMsg)+64];

						/*if ( ZApplication::GetGame()->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DUEL)
						{
							if ( !m_pMyCharacter->IsDie() && pChar->IsDie())
							{
								ChatColor = MCOLOR(0xFF808080);
								strcpy_safe( szMsg, "^0. . . . .");
							}
						}*/


						if(bSpUser) {
							//sprintf_safe(szTemp, "%s : %s", sp_name, szMsg);
							sprintf_safe(szTemp, "%s: %s", pChar->GetProperty()->szName, szMsg);
							ZChatOutput(UserNameColor, szTemp);
						}
						else {
							sprintf_safe(szTemp, "%s: %s", pChar->GetProperty()->szName, szMsg);
							ZChatOutput(ChatColor, szTemp);
						}
					}
				}

				// 팀 채팅 말 일때...
				else if (nTeam == nMyTeam)
				{
					if ( (!ZGetGameClient()->IsLadderGame() && !ZGetGameClient()->GetRejectTeamChat()) ||
						 ( ZGetGameClient()->IsLadderGame() && !ZGetGameClient()->GetRejectClanChat()) ||
						 ( strcmp( pChar->GetUserName(), ZGetMyInfo()->GetCharName()) == 0))
					{
						ZGetSoundEngine()->PlaySound("if_error");
						char szTemp[512];

						if(bSpUser) {
							sprintf_safe(szTemp, "(Team)%s: %s", sp_name,szMsg);
							ZChatOutput(UserNameColor, szTemp);
						}
						else {
							sprintf_safe(szTemp, "(Team)%s: %s", pChar->GetProperty()->szName,szMsg);
							ZChatOutput(TeamChatColor, szTemp);
						}
					}
				}
			}
		}
		break;

	case MC_PEER_CHAT_ICON:
		{
			bool bShow = false;
			pCommand->GetParameter(&bShow, 0, MPT_BOOL);

			MUID uid=pCommand->GetSenderUID();
			ZCharacter *pChar=ZGetCharacterManager()->Find(uid);
			if(pChar)
			{
				if(bShow)
				{
					if(!pChar->m_bChatEffect)
					{
						pChar->m_bChatEffect=true;
						ZGetEffectManager()->AddChatIcon(pChar);
					}
				}else
					pChar->m_bChatEffect=false;
			}
		}break;

		/*
		case MC_PEER_MOVE:
		{

		rvector pos, dir, velocity;
		pCommand->GetParameter(&pos, 0, MPT_POS);
		pCommand->GetParameter(&dir, 1, MPT_VECTOR);
		pCommand->GetParameter(&velocity, 2, MPT_VECTOR);
		int upper, lower;
		pCommand->GetParameter(&upper, 3, MPT_INT);
		pCommand->GetParameter(&lower, 4, MPT_INT);

		OnPeerMove(pCommand->GetSenderUID(), pos, dir, velocity, ZC_STATE_UPPER(upper), ZC_STATE_LOWER(lower));
		}
		break;
		*/
	case MC_MATCH_OBTAIN_WORLDITEM:
		{
			if (!IsReplay()) break;

			MUID uidPlayer;
			int nItemUID;

			pCommand->GetParameter(&uidPlayer, 0, MPT_UID);
			pCommand->GetParameter(&nItemUID, 1, MPT_INT);

			ZGetGameClient()->OnObtainWorldItem(uidPlayer, nItemUID);
		}
		break;
	case MC_MATCH_SPAWN_WORLDITEM:
		{
			if (!IsReplay()) break;

			MCommandParameter* pParam = pCommand->GetParameter(0);
			if (pParam->GetType()!=MPT_BLOB) break;

			void* pSpawnInfoBlob = pParam->GetPointer();

			ZGetGameClient()->OnSpawnWorldItem(pSpawnInfoBlob);
		}
		break;
	case MC_MATCH_REMOVE_WORLDITEM:
		{
			if (!IsReplay()) break;

			int nItemUID;

			pCommand->GetParameter(&nItemUID, 0, MPT_INT);

			ZGetGameClient()->OnRemoveWorldItem(nItemUID);
		}
		break;




	case MC_PEER_BASICINFO	: OnPeerBasicInfo(pCommand);break;
	case MC_PEER_HPINFO		: OnPeerHPInfo(pCommand);break;
	case MC_PEER_HPAPINFO		: OnPeerHPAPInfo(pCommand);break;
	case MC_PEER_PING		: OnPeerPing(pCommand); break;
	case MC_PEER_PONG		: OnPeerPong(pCommand); break;
	case MC_PEER_OPENED		: OnPeerOpened(pCommand); break;
	case MC_PEER_DASH	: OnPeerDash(pCommand); break;
	case MC_PEER_SHOT:
		{
			MCommandParameter* pParam = pCommand->GetParameter(0);
			if(pParam->GetType()!=MPT_BLOB) break;	// 문제가 있다

			ZPACKEDSHOTINFO *pinfo =(ZPACKEDSHOTINFO*)pParam->GetPointer();

			rvector pos = rvector(pinfo->posx,pinfo->posy,pinfo->posz);
			rvector to = rvector(pinfo->tox,pinfo->toy,pinfo->toz);

			// TODO : 시간판정을 각각해야한다
			OnPeerShot(pCommand->GetSenderUID(), pinfo->fTime, pos, to, (MMatchCharItemParts)pinfo->sel_type);
		}
		break;
	case MC_PEER_SHOT_MELEE:
		{
			float fShotTime;
			rvector pos, dir;

			pCommand->GetParameter(&fShotTime, 0, MPT_FLOAT);
			pCommand->GetParameter(&pos, 1, MPT_POS);

			OnPeerShot(pCommand->GetSenderUID(), fShotTime, pos, pos, MMCIP_MELEE);
		}
		break;

	case MC_PEER_SHOT_SP:
		{
			float fShotTime;
			rvector pos, dir;
			int sel_type,type;

			pCommand->GetParameter(&fShotTime, 0, MPT_FLOAT);
			pCommand->GetParameter(&pos, 1, MPT_POS);
			pCommand->GetParameter(&dir, 2, MPT_VECTOR);
			pCommand->GetParameter(&type, 3, MPT_INT);
			pCommand->GetParameter(&sel_type, 4, MPT_INT);

			// fShotTime 은 무시하고..
			//fShotTime=GetTime()-(float)GetPing(pCommand->GetSenderUID())*0.001f;

			OnPeerShotSp(pCommand->GetSenderUID(), fShotTime, pos, dir,type,(MMatchCharItemParts)sel_type);
		}
		break;

	case MC_PEER_RELOAD:
		{
			OnPeerReload(pCommand->GetSenderUID());
		}
		break;
	case MC_PEER_CHANGECHARACTER:
		{
			OnPeerChangeCharacter(pCommand->GetSenderUID());
		}
		break;

	case MC_PEER_DIE:
		{
			MUID	uid;
			pCommand->GetParameter(&uid , 0, MPT_UID);

			OnPeerDie(pCommand->GetSenderUID(), uid);

		}
		break;
	case MC_MATCH_GAME_DEAD:
		{
			MUID uidAttacker, uidVictim;
			unsigned long int nAttackerArg, nVictimArg;

			pCommand->GetParameter(&uidAttacker, 0, MPT_UID);
			pCommand->GetParameter(&nAttackerArg, 1, MPT_UINT);
			pCommand->GetParameter(&uidVictim, 2, MPT_UID);
			pCommand->GetParameter(&nVictimArg, 3, MPT_UINT);

			OnPeerDead(uidAttacker, nAttackerArg, uidVictim, nVictimArg);
		}
		break;
	case MC_MATCH_GAME_TEAMBONUS:
		{
			MUID uidChar;
			unsigned long int nExpArg;

			pCommand->GetParameter(&uidChar, 0, MPT_UID);
			pCommand->GetParameter(&nExpArg, 1, MPT_UINT);

			OnReceiveTeamBonus(uidChar, nExpArg);
		}
		break;
/*
	case MC_MATCH_ASSIGN_COMMANDER:
		{
			MUID uidRedCommander, uidBlueCommander;

			pCommand->GetParameter(&uidRedCommander, 0, MPT_UID);
			pCommand->GetParameter(&uidBlueCommander, 1, MPT_UID);

			OnAssignCommander(uidRedCommander, uidBlueCommander);
		}
		break;
*/
	case MC_PEER_SPAWN:
		{
			rvector pos, dir;
			pCommand->GetParameter(&pos, 0, MPT_POS);
			pCommand->GetParameter(&dir, 1, MPT_DIR);

			OnPeerSpawn(pCommand->GetSenderUID(), pos, dir);
		}
		break;
	case MC_MATCH_GAME_RESPONSE_SPAWN:
		{
			MUID uidChar;
			MShortVector s_pos, s_dir;

			pCommand->GetParameter(&uidChar, 0, MPT_UID);
			pCommand->GetParameter(&s_pos, 1, MPT_SVECTOR);
			pCommand->GetParameter(&s_dir, 2, MPT_SVECTOR);

			rvector pos, dir;
			pos = rvector((float)s_pos.x, (float)s_pos.y, (float)s_pos.z);
			dir = rvector(ShortToDirElement(s_dir.x), ShortToDirElement(s_dir.y), ShortToDirElement(s_dir.z));

			OnPeerSpawn(uidChar, pos, dir);
		}
		break;
	case MC_MATCH_SET_OBSERVER:
		{
			MUID uidChar;

			pCommand->GetParameter(&uidChar, 0, MPT_UID);

			OnSetObserver(uidChar);
		}
		break;
	case MC_PEER_CHANGE_WEAPON:
		{
			int nWeaponID;

			pCommand->GetParameter(&nWeaponID, 0, MPT_INT);

			OnChangeWeapon(pCommand->GetSenderUID(),MMatchCharItemParts(nWeaponID));
		}

		break;

	case MC_PEER_SPMOTION:
		{
			int nMotionType;

			pCommand->GetParameter(&nMotionType, 0, MPT_INT);

			OnPeerSpMotion(pCommand->GetSenderUID(),nMotionType);
		}
		break;

	case MC_PEER_CHANGE_PARTS:
		{
			int PartsType;
			int PartsID;

			pCommand->GetParameter(&PartsType, 0, MPT_INT);
			pCommand->GetParameter(&PartsID, 1, MPT_INT);

			OnChangeParts(pCommand->GetSenderUID(),PartsType,PartsID);
		}
		break;

	case MC_PEER_ATTACK:
		{
			int		type;
			rvector pos;

			pCommand->GetParameter(&type, 0, MPT_INT);
			pCommand->GetParameter(&pos , 1, MPT_POS);

			//OnAttack( pCommand->GetSenderUID(), type, pos);
		}
		break;

	case MC_PEER_DAMAGE:
		{
			MUID	tuid;
			int		damage;

			pCommand->GetParameter(&tuid   , 0, MPT_UID);
			pCommand->GetParameter(&damage , 1, MPT_INT);

			OnDamage( pCommand->GetSenderUID(), tuid, damage);
		}
		break;
	case MC_MATCH_RESET_TEAM_MEMBERS:
		{
			OnResetTeamMembers(pCommand);
		}
		break;

	case MC_REQUEST_XTRAP_HASHVALUE :
		{
			char szNewRandomValue[ 256 ] = {0,};

			pCommand->GetParameter( szNewRandomValue, 0, MPT_STR, 255 );

			ZApplication::GetGameInterface()->OnRequestNewHashValue( szNewRandomValue );
		}
		break;

	case MC_MATCH_DISCONNMSG :
		{
			DWORD dwMsgID;
			pCommand->GetParameter( &dwMsgID, 0, MPT_UINT );

			ZApplication::GetGameInterface()->OnDisconnectMsg( dwMsgID );
		}
		break;

	case ZC_TEST_INFO:
		{
			OutputToConsole("Sync : %u", ZGetGameClient()->GetGlobalClockCount());

			rvector v;
			v = m_pMyCharacter->m_Position;
			OutputToConsole("My Pos = %.2f %.2f %.2f", v.x, v.y, v.z);
		}
		break;
	case ZC_BEGIN_PROFILE:	
		g_bProfile=true;	
		break;
	case ZC_END_PROFILE:	
		g_bProfile=false;	
		break;
	case ZC_EVENT_OPTAIN_SPECIAL_WORLDITEM:
		OnLocalOptainSpecialWorldItem(pCommand);
		break;
	}

	return true;
}

rvector ZGame::GetMyCharacterFirePosition(void)
{
	rvector p = g_pGame->m_pMyCharacter->GetPosition();
	p.z += 160.f;
	return p;
}

void ZGame::OnPeerBasicInfo(MCommand *pCommand, bool bAddHistory, bool bUpdate)
{
	MCommandParameter* pParam = pCommand->GetParameter(0);
	if (pParam->GetType() != MPT_BLOB) return;

	ZPACKEDBASICINFO* ppbi = (ZPACKEDBASICINFO*)pParam->GetPointer();

	ZBasicInfo bi;
	bi.position = rvector(Roundf(ppbi->posx), Roundf(ppbi->posy), Roundf(ppbi->posz));
	bi.velocity = rvector(ppbi->velx, ppbi->vely, ppbi->velz);
	bi.direction = 1.f / 32000.f * rvector(ppbi->dirx, ppbi->diry, ppbi->dirz);

	// Reject basicinfos that have NaN values
	for (auto&& vec : { bi.position, bi.direction, bi.velocity })
		if (isnan(vec.x) || isnan(vec.y) || isnan(vec.z))
			return;
	
	MUID uid = pCommand->GetSenderUID();

	MMatchPeerInfo* pPeer = ZGetGameClient()->FindPeer(uid);
	if (pPeer) {
		if (pPeer->IsOpened() == false) {
			MCommand* pCmd = ZGetGameClient()->CreateCommand(MC_PEER_OPENED, ZGetGameClient()->GetPlayerUID());
			pCmd->AddParameter(new MCmdParamUID(pPeer->uidChar));
			ZGetGameClient()->Post(pCmd);

			pPeer->SetOpened(true);
		}
	}

	ZCharacter* pCharacter = m_CharacterManager.Find(uid);
	if (!pCharacter) return;

	pCharacter->UpdateTimeOffset(ppbi->fTime, GetTime());

	if(bAddHistory)
	{
		BasicInfoItem bii;
		ppbi->Unpack(bii);
		bii.cameradir = bii.direction;
		bii.SentTime = ppbi->fTime - pCharacter->GetTimeOffset();
		bii.RecvTime = GetTime();
		pCharacter->BasicInfoHistory.AddBasicInfo(bii);
	}

	if(bUpdate)
	{
		if(!IsReplay() && pCharacter->IsHero()) return;

		pCharacter->SetNetPosition(bi.position, bi.velocity, bi.direction);

		pCharacter->SetAnimationLower((ZC_STATE_LOWER)ppbi->lowerstate);
		pCharacter->SetAnimationUpper((ZC_STATE_UPPER)ppbi->upperstate);

		if(pCharacter->GetItems()->GetSelectedWeaponParts()!=ppbi->selweapon) {
			pCharacter->ChangeWeapon((MMatchCharItemParts)ppbi->selweapon);
		}
	}
}

void ZGame::OnPeerNewBasicInfo(MCommand * pCommand, bool bAddHistory, bool bUpdate)
{
	auto* Param = static_cast<MCmdParamBlob*>(pCommand->GetParameter(0));
	auto* pbi = static_cast<u8*>(Param->GetPointer());
	NewBasicInfo nbi;
	if (!UnpackNewBasicInfo(nbi, pbi, Param->GetPayloadSize()))
		return;

	auto it = m_CharacterManager.find(pCommand->GetSenderUID());
	if (it == m_CharacterManager.end())
		return;

	auto& Char = *it->second;
	Char.UpdateTimeOffset(nbi.Time, GetTime());

	if (bAddHistory)
	{
		nbi.bi.SentTime = nbi.Time - Char.GetTimeOffset();
		nbi.bi.RecvTime = GetTime();
		Char.BasicInfoHistory.AddBasicInfo(nbi.bi);
	}

	if (bUpdate)
	{
		if (!IsReplay() && Char.IsHero()) return;

		Char.SetNetPosition(nbi.bi.position, nbi.bi.velocity, nbi.bi.direction);

		Char.CameraDir = nbi.bi.cameradir;

		if (nbi.Flags & static_cast<int>(BasicInfoFlags::Animations))
		{
			Char.SetAnimationLower(static_cast<ZC_STATE_LOWER>(nbi.bi.lowerstate));
			Char.SetAnimationUpper(static_cast<ZC_STATE_UPPER>(nbi.bi.upperstate));
		}

		if (nbi.Flags & static_cast<int>(BasicInfoFlags::SelItem)
			&& Char.GetItems()->GetSelectedWeaponParts() != nbi.bi.SelectedSlot) {
			Char.ChangeWeapon(static_cast<MMatchCharItemParts>(nbi.bi.SelectedSlot));
		}
	}
}

void ZGame::OnPeerHPInfo(MCommand *pCommand)
{
	MUID uid = pCommand->GetSenderUID();
	ZCharacter* pCharacter = m_CharacterManager.Find(uid);
	if (!pCharacter) return;

	float fHP=0.0f;
	pCommand->GetParameter(&fHP, 0, MPT_FLOAT);

	pCharacter->SetHP(fHP);
}


void ZGame::OnPeerHPAPInfo(MCommand *pCommand)
{
	MUID uid = pCommand->GetSenderUID();
	ZCharacter* pCharacter = m_CharacterManager.Find(uid);
	if (!pCharacter) return;

	float fHP=0.0f;
	pCommand->GetParameter(&fHP, 0, MPT_FLOAT);
	float fAP=0.0f;
	pCommand->GetParameter(&fAP, 1, MPT_FLOAT);

	pCharacter->SetHP(fHP);
	pCharacter->SetAP(fAP);

	if (pCharacter == m_pMyCharacter && fHP > 0 && !m_pMyCharacter->IsAlive())
		m_pMyCharacter->Revival();
}

#ifdef _DEBUG
	static int g_nPingCount=0;
	static int g_nPongCount=0;
#endif

void ZGame::OnPeerPing(MCommand *pCommand)
{
	if(m_bReplaying) return;

	unsigned int nTimeStamp;
	pCommand->GetParameter(&nTimeStamp, 0, MPT_UINT);
	
	// PONG 으로 응답한다
	MCommandManager* MCmdMgr = ZGetGameClient()->GetCommandManager();
	MCommand* pCmd = new MCommand(MCmdMgr->GetCommandDescByID(MC_PEER_PONG), 
								  pCommand->GetSenderUID(), ZGetGameClient()->GetUID());	
	pCmd->AddParameter(new MCmdParamUInt(nTimeStamp));
	ZGetGameClient()->Post(pCmd);
}

void ZGame::OnPeerPong(MCommand *pCommand)
{
	MMatchPeerInfo* pPeer = ZGetGameClient()->FindPeer(pCommand->GetSenderUID());
	if (pPeer == NULL)
		return;

	unsigned int nTimeStamp;
	pCommand->GetParameter(&nTimeStamp, 0, MPT_UINT);

	int nPing = (GetTickTime() - nTimeStamp)/2;
    pPeer->UpdatePing(GetTickTime(), nPing);

	#ifdef _DEBUG
		g_nPongCount++;
	#endif
}

void ZGame::OnPeerOpened(MCommand *pCommand)
{
	MUID uidChar;
	pCommand->GetParameter(&uidChar, 0, MPT_UID);

	//// Show Character ////////////////////////////////////////
	ZCharacter* pCharacter = m_CharacterManager.Find(uidChar);
	if (pCharacter && pCharacter->IsDie()==false) {
		pCharacter->SetVisible(true);

		// 신입 캐릭터에게 자신의 무기를 알린다...
		ZCharacter* pMyCharacter = g_pGame->m_pMyCharacter;
		if(pMyCharacter)
		{
			int nParts = g_pGame->m_pMyCharacter->GetItems()->GetSelectedWeaponParts();
			g_pGame->m_pMyCharacter->m_bSpMotion = false;
			ZPostChangeWeapon(nParts);
		}
	}

#ifdef _DEBUG
	//// PeerOpened Debug log //////////////////////////////////
	char* pszName = "Unknown";
	char* pszNAT = "NoNAT";
	MMatchPeerInfo* pPeer = ZGetGameClient()->FindPeer(uidChar);
	if (pPeer) {
		pszName = pPeer->CharInfo.szName;
		if (pPeer->GetUDPTestResult() == false) pszNAT = "NAT";
	}

	char szBuf[64];
	sprintf_safe(szBuf, "PEER_OPENED(%s) : %s(%d%d) \n", pszNAT, pszName, uidChar.High, uidChar.Low);
	OutputDebugString(szBuf);
#endif
}

void ZGame::OnChangeWeapon(const MUID& uid, MMatchCharItemParts parts)
{
	ZCharacter* pCharacter = m_CharacterManager.Find(uid);
//	if (uid == ZGetGameClient()->GetUID()) pCharacter = m_pMyCharacter;
//	_ASSERT(pCharacter != NULL);

	if (pCharacter && pCharacter!=m_pMyCharacter)		// 내 캐릭터는 이미 바꿨다.
	{
		pCharacter->ChangeWeapon(parts);
	}
}

void ZGame::OnChangeParts(const MUID& uid,int partstype,int PartsID)
{
	ZCharacter* pCharacter = m_CharacterManager.Find(uid);
//	if (uid == ZGetGameClient()->GetUID()) pCharacter = m_pMyCharacter;

	if ( pCharacter ) {
		pCharacter->OnChangeParts( (RMeshPartsType)partstype , PartsID );
	}
}

void ZGame::OnDamage(const MUID& uid, const MUID& tuid,int damage)
{
}

void ZGame::OnPeerShotSp(const MUID& uid, float fShotTime, const rvector& pos, const rvector& dir,
	int type, MMatchCharItemParts sel_type)
{
	ZCharacter* pOwnerCharacter = NULL;

	pOwnerCharacter = m_CharacterManager.Find(uid);

	if (pOwnerCharacter == NULL) return;
	if(!pOwnerCharacter->GetInitialized()) return;
	if(!pOwnerCharacter->IsVisible()) return;

	MMatchCharItemParts parts = (MMatchCharItemParts)sel_type;

	if( parts != pOwnerCharacter->GetItems()->GetSelectedWeaponParts()) {
		OnChangeWeapon(uid,parts);
	}

	ZItem *pItem = pOwnerCharacter->GetItems()->GetItem(sel_type);
	if(!pItem) return;

	if (pOwnerCharacter->CheckValidShotTime(pItem->GetDescID(), fShotTime, pItem)) {
		pOwnerCharacter->UpdateValidShotTime(pItem->GetDescID(), fShotTime);
	} else {
		return;
	}

	if (uid == ZGetMyUID()) {
		MDataChecker* pChecker = ZApplication::GetGame()->GetDataChecker();
		MDataCheckNode* pCheckNode = pChecker->FindCheck((BYTE*)pItem->GetAMagazinePointer());
		if (pCheckNode && (pCheckNode->UpdateChecksum()==false)) {
			pChecker->BringError();
		} else {
			int nAMagazine = pItem->GetBulletAMagazine();

			if (!pItem->Shot()) return;

			if (pItem->GetBulletAMagazine() < nAMagazine)
				pChecker->RenewCheck((BYTE*)pItem->GetAMagazinePointer(), sizeof(int));
			else
				if(sel_type != MMCIP_MELEE)
					pChecker->BringError();
		}
	} else {
		if (!pItem->Shot()) return;
	}

	rvector velocity;
	rvector _pos;

	bool dLight = true;
	bool bSpend = false;

	switch(type)
	{
	case ZC_WEAPON_SP_GRENADE : {
		bSpend = true;

		velocity	= pOwnerCharacter->GetVelocity()+pOwnerCharacter->GetTargetDir() * 1200.f;
		velocity.z	+= 300.f;
		m_WeaponManager.AddGrenade(pos,velocity,pOwnerCharacter);
		}break;

	case ZC_WEAPON_SP_ROCKET: {
			m_WeaponManager.AddRocket(pos,dir,pOwnerCharacter);
			if(Z_VIDEO_DYNAMICLIGHT)
				ZGetStencilLight()->AddLightSource( pos, 2.0f, 100 );
		}break;

	case  ZC_WEAPON_SP_FLASHBANG:
		bSpend = true; 

		velocity	= pOwnerCharacter->GetVelocity() + pOwnerCharacter->GetTargetDir() * 1200.f;
		velocity.z	+= 300.0f;
		m_WeaponManager.AddFlashBang(pos,velocity,pOwnerCharacter);
		dLight	= false;
		break;

	case  ZC_WEAPON_SP_SMOKE:
		bSpend = true;

		velocity	= pOwnerCharacter->GetVelocity() + pOwnerCharacter->GetTargetDir() * 1200.f;
		velocity.z	+= 300.0f;
		m_WeaponManager.AddSmokeGrenade(pos,velocity,pOwnerCharacter);
		dLight	= false;
		break;

	case  ZC_WEAPON_SP_TEAR_GAS:
		bSpend = true;

		dLight	= false;
		break;

	case  ZC_WEAPON_SP_ITEMKIT: 
		{
			int nLinkedWorldItem = ZGetWorldItemManager()->GetLinkedWorldItemID(pItem->GetDesc());

			velocity	= dir;
			_pos = pos;

			m_WeaponManager.AddKit(_pos,velocity,pOwnerCharacter,0.2f,pItem->GetDesc()->m_szMeshName,nLinkedWorldItem);
			dLight	= false;
		}
		break;
	}

	ZApplication::GetSoundEngine()->PlaySEFire(pItem->GetDesc(), pos.x, pos.y, pos.z, (pOwnerCharacter==m_pMyCharacter));
	
	if( dLight )
	{	
		ZCharacter* pChar;

		if( ZGetConfiguration()->GetVideo()->bDynamicLight && pOwnerCharacter != NULL )	{

			pChar = pOwnerCharacter;

			if( pChar->m_bDynamicLight ) {

				pChar->m_vLightColor = g_CharLightList[CANNON].vLightColor;
				pChar->m_fLightLife = g_CharLightList[CANNON].fLife;

			} else {

				pChar->m_bDynamicLight = true;
				pChar->m_vLightColor = g_CharLightList[CANNON].vLightColor;
				pChar->m_vLightColor.x = 1.0f;
				pChar->m_iDLightType = CANNON;
				pChar->m_fLightLife = g_CharLightList[CANNON].fLife;
			}

			if( pOwnerCharacter->IsHero() )
			{
				RGetDynamicLightManager()->AddLight( GUNFIRE, pos );
			}
		}
	}
}

bool ZGame::CheckWall(ZObject* pObj1,ZObject* pObj2)
{
	if( (pObj1==NULL) || (pObj2==NULL) )
		return false;

	if( (pObj1->GetVisualMesh()==NULL) || (pObj2->GetVisualMesh()==NULL) )
		return false;

	// 에니메이션 때문에 벽을 뚫고 들어가는 경우도 있어서..
	rvector p1 = pObj1->GetPosition() + rvector(0.f,0.f,100.f);
	rvector p2 = pObj2->GetPosition() + rvector(0.f,0.f,100.f);

	rvector dir = p2 - p1;

	Normalize(dir);

	ZPICKINFO pickinfo;

	if( Pick( pObj1 , p1 , dir, &pickinfo ) ) {//벽이라면
		if(pickinfo.bBspPicked)//맵이 걸린경우
			return true;
	}

	return false;
}

void ZGame::OnExplosionGrenade(MUID uidOwner,rvector pos,float fDamage,float fRange,float fMinDamage,float fKnockBack,MMatchTeam nTeamID)
{
	if (ZGetGame()->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_SKILLMAP)
		return;

	ZObject* pTarget = NULL;

	float fDist,fDamageRange;

	for (ZObjectManager::iterator itor = m_ObjectManager.begin(); itor != m_ObjectManager.end(); ++itor) {

		pTarget = (*itor).second;

		if(pTarget && !pTarget->IsDie()) {

			fDist = Magnitude(pos-(pTarget->GetPosition()+rvector(0,0,80)));
			if(fDist < fRange) {

				// 두 캐릭터사이에 장애물이 없어야한다~ 
//				if( CheckWall(pos,pTarget) == false )
				{

					rvector dir=pos-(pTarget->GetPosition()+rvector(0,0,80));
					Normalize(dir);

					// 몸에 직접 맞았다.
					if(GetDistance(pos,pTarget->GetPosition()+rvector(0,0,50),pTarget->GetPosition()+rvector(0,0,130))<50)
					{
						fDamageRange = 1.f;
					}else
					{
#define MAX_DMG_RANGE	50.f	// 반경이만큼 까지는 최대 데미지를 다 먹는다
//#define MIN_DMG			0.4f	// 최소 기본 데미지는 이정도.

						fDamageRange = 1.f - (1.f-fMinDamage)*( max(fDist-MAX_DMG_RANGE,0) / (fRange-MAX_DMG_RANGE));
					}

					// 수류탄을 맞으면 반동으로 튀어나간다.

					ZActor* pATarget = MDynamicCast(ZActor,pTarget);

					bool bPushSkip = false;

					if(pATarget) {
						bPushSkip = pATarget->GetNPCInfo()->bNeverPushed;
					}

					if(bPushSkip==false) {
						pTarget->AddVelocity(fKnockBack*7.f*(fRange-fDist)*-dir);
					}
					else {
						ZGetSoundEngine()->PlaySound("fx_bullethit_mt_met");
					}

					ZCharacter* pOwnerCharacter = g_pGame->m_CharacterManager.Find( uidOwner );
					if(pOwnerCharacter) {
						CheckCombo(pOwnerCharacter, pTarget,!bPushSkip);
						CheckStylishAction(pOwnerCharacter);
					}

					float fActualDamage = fDamage * fDamageRange;
					float fRatio = ZItem::GetPiercingRatio( MWT_FRAGMENTATION , eq_parts_chest );//수류탄과 로켓 구분없다..
					if (ZGetGameClient()->GetMatchStageSetting()->GetNetcode() != NetcodeType::ServerBased)
						pTarget->OnDamaged(pOwnerCharacter,pos,ZD_EXPLOSION,MWT_FRAGMENTATION,fActualDamage,fRatio);
				}
			}
		}
	}

#define SHOCK_RANGE		1500.f			// 10미터까지 흔들린다

	ZCharacter *pTargetCharacter=ZGetGameInterface()->GetCombatInterface()->GetTargetCharacter();
	float fPower= (SHOCK_RANGE-Magnitude(pTargetCharacter->GetPosition()+rvector(0,0,50) - pos))/SHOCK_RANGE;

	if(fPower>0)
		ZGetGameInterface()->GetCamera()->Shock(fPower*500.f, .5f, rvector(0.0f, 0.0f, -1.0f));

	GetWorld()->GetWaters()->CheckSpearing( pos, pos + rvector(0,0,MAX_WATER_DEEP), 500, 0.8f );

//	static RealSoundEffectSource* pSES= ZApplication::GetSoundEngine()->GetSES("explosion");
//	ZApplication::GetSoundEngine()->PlaySE(pSES,pos.x,pos.y,pos.z);
}

// 매직류의 무기의 데미지를 준다
void ZGame::OnExplosionMagic(ZWeaponMagic *pWeapon, MUID uidOwner,rvector pos,float fMinDamage,float fKnockBack,MMatchTeam nTeamID,bool bSkipNpc)
{
	ZObject* pTarget = NULL;

	float fRange = 100.f * pWeapon->GetDesc()->fEffectArea;
	float fDist,fDamageRange;

	for (ZObjectManager::iterator itor = m_ObjectManager.begin(); itor != m_ObjectManager.end(); ++itor) {

		pTarget = (*itor).second;

		// 범위공격이 아니라면 타겟만 검사하면된다.
		if(pTarget->IsNPC()) continue;

		if(!pWeapon->GetDesc()->IsAreaTarget() && pWeapon->GetTarget()!=pTarget->GetUID()) continue;

		if(pTarget && !pTarget->IsDie()) {

			// 두 캐릭터사이에 장애물이 없어야한다~ 
//				if( CheckWall(pos,pTarget) == false )
			{
				fDist = Magnitude(pos-(pTarget->GetPosition()+rvector(0,0,80)));
				if(pWeapon->GetDesc()->IsAreaTarget())	// 범위공격이면 거리에 따른 데미지를 계산한다
				{
					if(fDist > fRange) continue;	// 범위를 벗어났다

					// 몸에 직접 맞았다.
					if(GetDistance(pos,pTarget->GetPosition()+rvector(0,0,50),pTarget->GetPosition()+rvector(0,0,130))<50)
					{
						fDamageRange = 1.f;
					}else
					{
#define MAX_DMG_RANGE	50.f	// 반경이만큼 까지는 최대 데미지를 다 먹는다

						fDamageRange = 1.f - (1.f-fMinDamage)*( max(fDist-MAX_DMG_RANGE,0) / (fRange-MAX_DMG_RANGE));
					}
				}else {
					fDamageRange = 1.f;
				}

				// resist 를 체크한다
				float fDamage = pWeapon->GetDesc()->nModDamage;
				if(pWeapon->GetDesc()->CheckResist(pTarget,&fDamage)) 
				{
					ZCharacter* pOwnerCharacter = g_pGame->m_CharacterManager.Find( uidOwner );
					if(pOwnerCharacter) {
						CheckCombo(pOwnerCharacter, pTarget,true);
						CheckStylishAction(pOwnerCharacter);
					}

					// 수류탄을 맞으면 반동으로 튀어나간다.
					rvector dir=pos-(pTarget->GetPosition()+rvector(0,0,80));
					Normalize(dir);
					pTarget->AddVelocity(fKnockBack*7.f*(fRange-fDist)*-dir);

					float fActualDamage = fDamage * fDamageRange;
					float fRatio = ZItem::GetPiercingRatio( MWT_FRAGMENTATION , eq_parts_chest );//수류탄과 로켓 구분없다..
					pTarget->OnDamaged(pOwnerCharacter,pos,ZD_MAGIC,MWT_FRAGMENTATION,fActualDamage,fRatio);
				} else {
					// TODO: 저항에 성공했으니 이펙트를 보여주자.
				}
			}
		}
	}

/*
#define SHOCK_RANGE		1500.f			// 10미터까지 흔들린다

	ZCharacter *pTargetCharacter=ZGetGameInterface()->GetCombatInterface()->GetTargetCharacter();
	float fPower= (SHOCK_RANGE-Magnitude(pTargetCharacter->GetPosition()+rvector(0,0,50) - pos))/SHOCK_RANGE;

	if ((fPower>0) && (pWeapon->GetDesc()->bCameraShock))
		ZGetGameInterface()->GetCamera()->Shock(fPower*500.f, .5f, rvector(0.0f, 0.0f, -1.0f));
*/

	if (pWeapon->GetDesc()->bCameraShock)
	{
		ZCharacter *pTargetCharacter=ZGetGameInterface()->GetCombatInterface()->GetTargetCharacter();
		const float fDefaultPower = 500.0f;
		float fShockRange = pWeapon->GetDesc()->fCameraRange;
		float fDuration = pWeapon->GetDesc()->fCameraDuration;
		float fPower= (fShockRange-Magnitude(pTargetCharacter->GetPosition()+rvector(0,0,50) - pos))/fShockRange;
		fPower *= pWeapon->GetDesc()->fCameraPower;
		
		if (fPower>0)
		{
			ZGetGameInterface()->GetCamera()->Shock(fPower*fDefaultPower, fDuration, rvector(0.0f, 0.0f, -1.0f));
		}
	}

	GetWorld()->GetWaters()->CheckSpearing( pos, pos + rvector(0,0,MAX_WATER_DEEP), 500, 0.8f );
}

void ZGame::OnExplosionMagicNonSplash(ZWeaponMagic *pWeapon, MUID uidOwner, MUID uidTarget,
	rvector pos, float fKnockBack)
{
	ZObject* pTarget = m_CharacterManager.Find( uidTarget );
	if (pTarget == NULL) return;

	if(pTarget->IsNPC()) return;

	if(pTarget && !pTarget->IsDie()) {
		float fDamage = pWeapon->GetDesc()->nModDamage;
		if(pWeapon->GetDesc()->CheckResist(pTarget,&fDamage)) 
		{
			ZCharacter* pOwnerCharacter = g_pGame->m_CharacterManager.Find( uidOwner );
			if(pOwnerCharacter) {
				CheckCombo(pOwnerCharacter, pTarget,true);
				CheckStylishAction(pOwnerCharacter);
			}

			// 반동으로 튀어나간다.
			rvector dir=pos-(pTarget->GetPosition()+rvector(0,0,80));
			Normalize(dir);
			pTarget->AddVelocity(fKnockBack*7.f*-dir);

			float fRatio = ZItem::GetPiercingRatio( MWT_FRAGMENTATION , eq_parts_chest );
			pTarget->OnDamaged(pOwnerCharacter,pos,ZD_MAGIC,MWT_FRAGMENTATION,fDamage,fRatio);
		} else {
			// TODO: 저항에 성공했으니 이펙트를 보여주자.
		}
	}
}

int ZGame::SelectSlashEffectMotion(ZCharacter* pCharacter)
{
	if(pCharacter==NULL) return SEM_None;

//	MWT_DAGGER
//	MWT_DUAL_DAGGER
//	MWT_KATANA
//	MWT_GREAT_SWORD
//	MWT_DOUBLE_KATANA

	auto lower = pCharacter->GetStateLower();

	int nAdd = 0;
	int ret = 0;

	MMatchWeaponType nType = pCharacter->GetSelectItemDesc()->m_nWeaponType;

	if(pCharacter->IsMan()) {

			 if(lower == ZC_STATE_LOWER_ATTACK1) {	nAdd = 0;	}
		else if(lower == ZC_STATE_LOWER_ATTACK2) {	nAdd = 1;	}
		else if(lower == ZC_STATE_LOWER_ATTACK3) {	nAdd = 2;	}
		else if(lower == ZC_STATE_LOWER_ATTACK4) {	nAdd = 3;	}
		else if(lower == ZC_STATE_LOWER_ATTACK5) {	nAdd = 4;	}
		else if(lower == ZC_STATE_LOWER_UPPERCUT) {	return SEM_ManUppercut;	}

			 if(nType == MWT_KATANA )		return SEM_ManSlash1 + nAdd;
		else if(nType == MWT_DOUBLE_KATANA)	return SEM_ManDoubleSlash1 + nAdd;
		else if(nType == MWT_GREAT_SWORD)	return SEM_ManGreatSwordSlash1 + nAdd;

	}
	else {

			 if(lower == ZC_STATE_LOWER_ATTACK1) {	nAdd = 0;	}
		else if(lower == ZC_STATE_LOWER_ATTACK2) {	nAdd = 1;	}
		else if(lower == ZC_STATE_LOWER_ATTACK3) {	nAdd = 2;	}
		else if(lower == ZC_STATE_LOWER_ATTACK4) {	nAdd = 3;	}
		else if(lower == ZC_STATE_LOWER_ATTACK5 ) {	nAdd = 4;	}
		else if(lower == ZC_STATE_LOWER_UPPERCUT) {	return SEM_WomanUppercut;	}

			 if(nType == MWT_KATANA )		return SEM_WomanSlash1 + nAdd;
		else if(nType == MWT_DOUBLE_KATANA)	return SEM_WomanDoubleSlash1 + nAdd;
		else if(nType == MWT_GREAT_SWORD)	return SEM_WomanGreatSwordSlash1 + nAdd;
	}

	return SEM_None;
}

// shot 이 너무 커서 분리..
void ZGame::OnPeerShot_Melee(const MUID& uidOwner, float fShotTime)
{
	ZObject *pOwner = m_ObjectManager.GetObject(uidOwner);
	if(!pOwner) return;

	ZItem *pItem = pOwner->GetItems()->GetItem(MMCIP_MELEE);
	if(!pItem) return;
	MMatchItemDesc *pDesc = pItem->GetDesc();
	if(!pDesc) { _ASSERT(FALSE); return; }

	ZGetSoundEngine()->PlaySound("blade_swing",pOwner->m_Position, 
		uidOwner==ZGetGameInterface()->GetCombatInterface()->GetTargetUID());

	RPickInfo info;

	bool bCheck = false;

	fShotTime=GetTime();

	rvector OwnerPosition,OwnerDir;
	OwnerPosition = pOwner->GetPosition();
	OwnerDir = pOwner->m_Direction;
	OwnerDir.z=0; 
	Normalize(OwnerDir);

	float fMeleeRange = pDesc->m_nRange;
	if (fMeleeRange == 0) {
		_ASSERT(FALSE);	// Unknown WeaponRange
		fMeleeRange = 150.f;
	}


	int cm = 0;
	
	ZCharacter *pOwnerCharacter = m_CharacterManager.Find(uidOwner);
	if(pOwnerCharacter) {
		cm = SelectSlashEffectMotion(pOwnerCharacter);
	}

	for (ZObjectManager::iterator itor = m_ObjectManager.begin();
		itor != m_ObjectManager.end(); ++itor)
	{
		ZObject* pTar = (*itor).second;
		if (pOwner == pTar) continue;

		rvector TargetPosition,TargetDir,TargetDirLow;

		if(pTar->IsDie()) continue;
		if( !pTar->GetHistory(&TargetPosition,&TargetDir,fShotTime)) continue;

		rvector swordPos = OwnerPosition + OwnerDir*100.f;
		swordPos.z += pOwner->GetCollHeight() * .5f;
		float fDist = GetDistanceLineSegment(swordPos,TargetPosition,TargetPosition+rvector(0,0,pTar->GetCollHeight()));

		if (fDist < fMeleeRange) {

			if( IsAttackable(pOwner,pTar) ) {
				
				rvector fTarDir = TargetPosition - (OwnerPosition - OwnerDir*50.f);
				Normalize(fTarDir);
				float fDot = D3DXVec3Dot(&OwnerDir, &fTarDir);

				if (fDot > 0.5f) {

					if(CheckWall(pOwner,pTar)==false)
					{
						if(pTar->IsGuard() && DotProduct(pTar->m_Direction,OwnerDir)<0 ) {
							rvector t_pos = pTar->GetPosition();
							t_pos.z += 120.f;
							ZGetEffectManager()->AddSwordDefenceEffect(t_pos+(pTar->m_Direction*50.f),pTar->m_Direction);
							pTar->OnMeleeGuardSuccess();
						}else
						{
							rvector tpos = pTar->GetPosition();

							tpos.z += 130.f;
							tpos -= pOwner->m_Direction * 50.f;

							ZGetEffectManager()->AddBloodEffect( tpos , -fTarDir);
							ZGetEffectManager()->AddSlashEffect( tpos , -fTarDir , cm);

							float fActualDamage = CalcActualDamage(pOwner, pTar, (float)pDesc->m_nDamage);
							float fRatio = pItem->GetPiercingRatio( pDesc->m_nWeaponType , eq_parts_chest );
							pTar->OnDamaged(pOwner, pOwner->GetPosition(), ZD_MELEE, pDesc->m_nWeaponType, fActualDamage, fRatio, cm);

							ZActor* pATarget = MDynamicCast(ZActor,pTar);

							bool bPushSkip = false;

							if(pATarget) {
								bPushSkip = pATarget->GetNPCInfo()->bNeverPushed;
							}

							float fKnockbackForce = pItem->GetKnockbackForce();

							if(bPushSkip) {
								ZGetSoundEngine()->PlaySound("fx_bullethit_mt_met");
								fKnockbackForce = 1.0f;
							}

							pTar->OnKnockback( pOwner->m_Direction, fKnockbackForce );

							ZGetSoundEngine()->PlaySound("blade_damage", tpos );

							bCheck = true;

							if(pOwner == m_pMyCharacter) {
								CheckCombo(m_pMyCharacter, pTar,!bPushSkip);
								CheckStylishAction(m_pMyCharacter);
							}
						}
					}
				}
			}
		}
	}

	if(!bCheck){

		rvector vPos = pOwner->GetPosition();
		rvector vDir = OwnerDir;

		vPos.z += 130.f;

		RBSPPICKINFO bpi;

		if(GetWorld()->GetBsp()->Pick(vPos, vDir, &bpi)) {

			float fDist = Magnitude(vPos - bpi.PickPos);

			if (fDist < fMeleeRange) {

				rplane r = bpi.pInfo->plane;
				rvector vWallDir = rvector( r.a, r.b, r.c );
				Normalize(vWallDir);

//				int cm = SelectSlashEffectMotion(pOwnerCharacter);//남녀 칼 휘두르는 방향

				ZGetEffectManager()->AddSlashEffectWall( bpi.PickPos - (vDir*5.f) , vWallDir ,cm);

				rvector pos = bpi.PickPos;

				ZGetSoundEngine()->PlaySound("blade_concrete", pos );

				//test 용..
				//					int cm = SelectSlashEffectMotion(pOwnerCharacter);//남녀 칼 휘두르는 방향
				//					m_EffectManager.AddSlashEffect(bpi.PickPos - (vDir*5.f), -vDir,cm);
			}
		}
	}

	return;
}

void ZGame::OnPeerSlash(ZCharacter *pOwner, const rvector &pos, const rvector &dir, int Type)
{
	//g_Attacks[pOwner].TotalSlashes++;

	g_RGMain->OnSlash(pOwner, pos, dir);

	ZItem *pItem = pOwner->GetItems()->GetItem(MMCIP_MELEE);
	if (!pItem) return;

	MMatchItemDesc *pDesc = pItem->GetDesc();
	if (!pDesc) return;

	ZGetSoundEngine()->PlaySound("blade_swing", pOwner->GetPosition(), pOwner == m_pMyCharacter, false, 0);

	int nRange = pDesc->m_nRange + 100;

	float fDamage = pDesc->m_nDamage;

	int cm = SelectSlashEffectMotion(pOwner);

	rvector OwnerHDir = dir;
	OwnerHDir.z = 0;
	Normalize(OwnerHDir);

	bool bPlayerHit = false;

	for (auto &it : m_ObjectManager)
	{
		ZObject *pTarget = it.second;

		if (pTarget == pOwner)
			continue;

		const rvector &TargetPos = pTarget->GetPosition();

		rvector swordPos = pos;
		swordPos.z += 180 * .5;
		float fDist = GetDistanceLineSegment(swordPos, TargetPos, TargetPos + rvector(0, 0, CHARACTER_HEIGHT));

		if (fDist > nRange)
			continue;

		if (!IsAttackable(pOwner, pTarget))
			continue;

		bPlayerHit = true;

		rvector HDirToTarget = TargetPos - pos;
		HDirToTarget.z = 0;
		Normalize(HDirToTarget);

		float fDot = DotProduct(OwnerHDir, HDirToTarget);
		if (fDot < 0.5)
			continue;

		// Old, buggy check allows for invulnerability
		/*int nDebugRegister;
		if (ZGameCheckWall(ZGetGame(), pOwner, pTarget, nDebugRegister))
		continue;*/

		rvector v1 = pos + rvector(0, 0, 100),
			v2 = TargetPos + rvector(0, 0, 100);

		ZPICKINFO zpi;
		Pick(pOwner, v1, v2, &zpi);

		if (zpi.bBspPicked && Magnitude(zpi.bpi.PickPos - v1) < Magnitude(v2 - v1))
			continue;

		rvector TargetHDir = pTarget->GetDirection();
		TargetHDir.z = 0;
		Normalize(TargetHDir);

		ZCharacter *pCharTarget = MDynamicCast(ZCharacter, pTarget);

		if (pCharTarget)
		{
			if (pCharTarget->IsGuardCustom() && DotProduct(OwnerHDir, TargetHDir) < 0)
			{
				rvector pos = pTarget->GetPosition() + rvector(0, 0, 120) + pTarget->GetDirection() * 50;
				ZGetEffectManager()->AddSwordDefenceEffect(pos, pTarget->m_Direction);
				pTarget->OnMeleeGuardSuccess();

				if (pOwner == m_pMyCharacter && pCharTarget->IsGuard())
					m_pMyCharacter->AddRecoilTarget(pCharTarget);

				continue;
			}
		}

		rvector AdjTargetPos = pTarget->GetPosition() + rvector(0, 0, 130) + pTarget->GetDirection() * 50;

		if (pTarget == m_pMyCharacter)
			ZGetEffectManager()->AddSlashEffect(AdjTargetPos, -dir, cm);

		pTarget->OnDamaged(pOwner, pos, ZD_MELEE, pDesc->m_nWeaponType, fDamage, pItem->GetPiercingRatio(pDesc->m_nWeaponType, eq_parts_chest), cm);

		pTarget->OnKnockback(dir, pItem->GetKnockbackForce()); // OnKnockback

		ZGetSoundEngine()->PlaySound("blade_damage", AdjTargetPos, false, false, 0);

		CheckCombo(pOwner, pTarget, true);
	}

	if (!bPlayerHit)
	{
		rvector vPos = pOwner->GetPosition();
		rvector vDir = OwnerHDir;

		vPos.z += 130.f;

		ZPICKINFO zpi;

		if (Pick(m_pMyCharacter, vPos, vDir, &zpi)) {
			if (!zpi.bBspPicked)
				return;

			float fDist = Magnitude(vPos - zpi.bpi.PickPos);

			if (fDist > nRange)
				return;

			rplane r = zpi.bpi.pInfo->plane;
			rvector vWallDir = rvector(r.a, r.b, r.c);
			Normalize(vWallDir);

			ZGetEffectManager()->AddSlashEffectWall(zpi.bpi.PickPos - (vDir*5.f), vWallDir, cm);

			rvector pos = zpi.bpi.PickPos;

			ZGetSoundEngine()->PlaySound("blade_concrete", pos, false, false, 0);
		}
	}
}

void ZGame::OnPeerMassive(ZCharacter *pOwner, const rvector &pos, const rvector &dir)
{
	if (!g_Rules.MassiveEffectEnabled())
		return;

	const int nRange = 280;

	g_RGMain->OnMassive(pOwner, pos, dir);

	ZItem *pItem = pOwner->GetItems()->GetItem(MMCIP_MELEE);
	if (!pItem) return;

	MMatchItemDesc *pDesc = pItem->GetDesc();
	if (!pDesc) return;

	pOwner->AddMassiveEffect(pos, dir);

	ZGetSoundEngine()->PlaySound("we_smash", pos, false, false, 0);

	rvector MidOwnerPos = pos + rvector(0, 0, 180 / 2);

	ZC_ENCHANT EnchantType = ZC_ENCHANT_NONE;

	for (auto &it : m_ObjectManager)
	{
		ZObject *pVictim = it.second;

		if (pOwner == m_pMyCharacter || pVictim != m_pMyCharacter)
			continue;

		const rvector &TargetPos = pVictim->GetPosition();

		float fDist = GetDistanceLineSegment(MidOwnerPos, TargetPos, TargetPos + rvector(0, 0, 180));

		if (fDist > nRange)
			continue;

		if (!IsAttackable(pOwner, pVictim))
			continue;

		/*int nDebugRegister;
		if (ZGameCheckWall(ZGetGame(), pOwner, pVictim, nDebugRegister))
		continue;*/

		rvector v1 = pos + rvector(0, 0, 100),
			v2 = TargetPos + rvector(0, 0, 100);

		ZPICKINFO zpi;
		Pick(pOwner, v1, v2, &zpi);

		if (zpi.bBspPicked && Magnitude(zpi.bpi.PickPos - v1) < Magnitude(v2 - v1))
			continue;

		const rvector &VictimDir = pVictim->GetDirection();

		if (pVictim->IsGuardCustom() && DotProduct(dir, VictimDir) < 0)
		{
			rvector addVel = TargetPos - pos;
			Normalize(addVel);
			addVel = 500.f * addVel;
			addVel.z = 200.f;
			pVictim->SetVelocity(pVictim->GetVelocity() + addVel);
			continue;
		}

		rvector AdjTargetPos = pos + rvector(0, 0, 130);

		if (!EnchantType)
		{
			ZGetEffectManager()->AddSwordUppercutDamageEffect(AdjTargetPos, pVictim->GetUID(), 0);
		}
		else
		{
			ZGetEffectManager()->AddSwordEnchantEffect(EnchantType, TargetPos, 20, 1);
		}

#define MAX_DMG_RANGE	50.f
#define MIN_DMG			0.3f

		float fDamageRange = 1.f - (1.f - MIN_DMG)*(max(fDist - MAX_DMG_RANGE, 0) / (float(nRange) - MAX_DMG_RANGE));

#define SLASH_DAMAGE	3

		int nSwordDamage = pDesc->m_nDamage;

		int damage = (1.5f - (1.5f - 0.9f) * (max(fDist - MAX_DMG_RANGE, 0) / (nRange - MAX_DMG_RANGE))) * nSwordDamage;

		// Stop dash locks
		if (pVictim == m_pMyCharacter && m_pMyCharacter->m_bTumble)
		{
			m_pMyCharacter->m_bTumble = false;
		}

		pVictim->OnDamaged(pOwner, pos, ZD_KATANA_SPLASH, MWT_KATANA, damage, .4, -1);
		pVictim->OnDamagedAnimation(pOwner, SEM_WomanSlash5);

		ZPostPeerEnchantDamage(pOwner->GetUID(), pVictim->GetUID());
	}
}

//RBspObject* test = nullptr;

void ZGame::OnPeerShot_Range(MMatchCharItemParts sel_type, const MUID& uidOwner, float fShotTime,
	rvector& pos, rvector& to, u32 seed)
{
	ZObject *pOwner = m_ObjectManager.GetObject(uidOwner);
	if(!pOwner) return;

	ZItem *pItem = pOwner->GetItems()->GetItem(sel_type);
	if(!pItem) return;
	MMatchItemDesc *pDesc = pItem->GetDesc();
	if(!pDesc) { _ASSERT(FALSE); return; }

	rvector dir = to - pos;

	Normalize(dir);

	rvector v1, v2;
	rvector BulletMarkNormal;
	bool bBulletMark = false;
	ZTargetType nTargetType = ZTT_OBJECT;

	ZCharacter* pCharacter = NULL;

	ZPICKINFO pickinfo;

	memset(&pickinfo,0,sizeof(ZPICKINFO));

	const DWORD dwPickPassFlag=RM_FLAG_ADDITIVE | RM_FLAG_HIDE | RM_FLAG_PASSROCKET | RM_FLAG_PASSBULLET;

	pOwner->Tremble(8.f, 50, 30);
	
	bool Picked = ::PickHistory<ZCharacter>(m_pMyCharacter, pos, to, GetWorld()->GetBsp(), pickinfo,
			MakePairValueAdapter(m_CharacterManager), fShotTime, dwPickPassFlag);

	//if(g_pGame->PickHistory(pOwner,fShotTime,pos,to,&pickinfo,dwPickPassFlag))
	if (Picked)
	{
		if (ZGetGameClient()->GetMatchStageSetting()->GetNetcode() == NetcodeType::ServerBased
			&& ZGetConfiguration()->GetShowHitRegDebugOutput()
			&& pickinfo.pObject)
		{
			auto Char = MDynamicCast(ZCharacter, pickinfo.pObject);
			if (Char)
				ZChatOutputF("Client: Hit %s", Char->GetUserName());
		}

		if(pickinfo.pObject)
		{
			ZObject *pObject = pickinfo.pObject;
			bool bGuard = pObject->IsGuard() && (pickinfo.info.parts!=eq_parts_legs) &&		// 다리는 막을수없다
							DotProduct(dir,pObject->GetDirection())<0;

			if(bGuard) {
				nTargetType = ZTT_CHARACTER_GUARD;
				// 막았다
				rvector t_pos = pObject->GetPosition();
				t_pos.z += 100.f;
				ZGetEffectManager()->AddSwordDefenceEffect(t_pos+(-dir*50.f),-dir);
				pObject->OnGuardSuccess();

			}else {
				nTargetType = ZTT_CHARACTER;

				ZActor* pATarget = MDynamicCast(ZActor,pickinfo.pObject);

				bool bPushSkip = false;

				if(pATarget) {
					bPushSkip = pATarget->GetNPCInfo()->bNeverPushed;
				}

				float fKnockbackForce = pItem->GetKnockbackForce();

				if(bPushSkip) 
				{
//					ZGetSoundEngine()->PlaySound("fx_bullethit_mt_met");
					rvector vPos = pOwner->GetPosition() + (pickinfo.pObject->GetPosition() - pOwner->GetPosition()) * 0.1f; 
					ZGetSoundEngine()->PlaySound("fx_bullethit_mt_met", vPos );
					fKnockbackForce = 1.0f;
				}

				pickinfo.pObject->OnKnockback( pOwner->m_Direction, fKnockbackForce );

				float fActualDamage = CalcActualDamage(pOwner, pickinfo.pObject, (float)pDesc->m_nDamage);
				float fRatio = pItem->GetPiercingRatio( pDesc->m_nWeaponType, pickinfo.info.parts );
				ZDAMAGETYPE dt = (pickinfo.info.parts==eq_parts_head) ? ZD_BULLET_HEADSHOT : ZD_BULLET;

				if (ZGetGameClient()->GetMatchStageSetting()->GetNetcode() == NetcodeType::P2PLead)
					pickinfo.pObject->OnDamaged(pOwner, pOwner->GetPosition(), dt, pDesc->m_nWeaponType, fActualDamage, fRatio );

				if(pOwner == m_pMyCharacter) {
					CheckCombo(m_pMyCharacter,pickinfo.pObject,!bPushSkip);
					CheckStylishAction(m_pMyCharacter);

					if (ZGetGameClient()->GetMatchStageSetting()->GetNetcode() == NetcodeType::P2PAntilead && !IsReplay())
					{
						auto Char = MDynamicCast(ZCharacter, pickinfo.pObject);

						if (Char)
						{
							ZPostAntileadDamage(Char->GetUID(), fActualDamage, fRatio, dt, pDesc->m_nWeaponType);
						}
					}
				}
			}

			v1 = pos;
			v2 = pickinfo.info.vOut;

		}else
			if(pickinfo.bBspPicked)
			{
				nTargetType = ZTT_OBJECT;

				v1 = pos;
				v2 = pickinfo.bpi.PickPos;

				// 총탄 흔적
				BulletMarkNormal.x = pickinfo.bpi.pInfo->plane.a;
				BulletMarkNormal.y = pickinfo.bpi.pInfo->plane.b;
				BulletMarkNormal.z = pickinfo.bpi.pInfo->plane.c;
				Normalize(BulletMarkNormal);
				bBulletMark = true;

			}
			else {
				_ASSERT(false);
				return;
			}
	}
	else {
		v1 = pos;
		v2 = pos+dir*10000.f;
		nTargetType	= ZTT_NOTHING;
	}

	bool bPlayer = false;
	//bool b3D = (pOwnerCharacter!=m_pMyCharacter);	// 자기가 내는 사운드는 2D로 출력한다.
	//rvector Pos = pOwnerCharacter->GetPosition();
	rvector Pos = v1;
	if(pOwner==m_pMyCharacter)
	{

		Pos = RCameraPosition;
		bPlayer = true;
	}

	ZCharacter *pTargetCharacter=ZGetGameInterface()->GetCombatInterface()->GetTargetCharacter();
	ZApplication::GetSoundEngine()->PlaySEFire(pDesc, Pos.x, Pos.y, Pos.z, bPlayer);
	//if(nTargetType == ZTT_OBJECT) { ZApplication::GetSoundEngine()->PlaySERicochet(v2.x, v2.y, v2.z); }
#define SOUND_CULL_DISTANCE 1500.0F
	auto vec = v2 - pTargetCharacter->m_Position;
	if( D3DXVec3LengthSq(&vec) < (SOUND_CULL_DISTANCE * SOUND_CULL_DISTANCE) )
	{
		if(nTargetType == ZTT_OBJECT) { 
			ZGetSoundEngine()->PlaySEHitObject( v2.x, v2.y, v2.z, pickinfo.bpi ); 
		}

		if(nTargetType == ZTT_CHARACTER) { 
			ZGetSoundEngine()->PlaySEHitBody(v2.x, v2.y, v2.z); 
		}
	}

	//// 보이지 않으면 이펙트를 그릴필요는 없다 - 정확한 컬링을 요망.. by bird
	//if(!pOwner->IsRendered()) return;


	// 쏘는곳 반경 100cm 가 화면에 들어가는지 체크한다
	bool bDrawFireEffects = isInViewFrustum(v1,100.f,RGetViewFrustum());

	if(!isInViewFrustum(v1,v2,RGetViewFrustum()) // 쏘는곳에서 맞는곳의 라인이 보이는지..
		&& !bDrawFireEffects) return;					// 쏘는곳에서도 그릴게 없는지..

	bool bDrawTargetEffects = isInViewFrustum(v2,100.f,RGetViewFrustum());



	/////////////////////// 이후는 이펙트 추가

	// 물튀는 이펙트 체크
	GetWorld()->GetWaters()->CheckSpearing( v1, v2, 250, 0.3 );



	// TODO: NPC 의 총구위치 인터페이스가 확정되면 마저 추가하자.
//	ZCharacter *pOwnerCharacter = m_CharacterManager.Find(uidOwner);

	
	ZCharacterObject* pCOwnerObject = MDynamicCast(ZCharacterObject, pOwner);

	if(pCOwnerObject) {

		// 총구 화염이펙트
		rvector pdir = v2-v1;
		Normalize(pdir);

		int size = 3;

		rvector v[6];

//		size = GetWeapondummyPos(pOwnerCharacter,v);
		if(pCOwnerObject->IsRendered())
			size = pCOwnerObject->GetWeapondummyPos(v);
		else
		{
			size = 6;
			v[0] = v[1] = v[2] = v1;
			v[3] = v[4] = v[5] = v[0];
		}


		MMatchWeaponType wtype = pDesc->m_nWeaponType;

		// Effect
		if(bBulletMark==false) BulletMarkNormal = -pdir;

		ZGetEffectManager()->AddShotEffect( v , size , v2, BulletMarkNormal, nTargetType, wtype,pCOwnerObject,bDrawFireEffects,bDrawTargetEffects);

		// 총 쏠때 라이트 추가
		ZCharacterObject* pChar;

		if( ZGetConfiguration()->GetVideo()->bDynamicLight && pCOwnerObject != NULL )
		{
			pChar = pCOwnerObject;
		
			if( pChar->m_bDynamicLight )
			{
				pChar->m_vLightColor = g_CharLightList[GUN].vLightColor;
				pChar->m_fLightLife = g_CharLightList[GUN].fLife;
			}
			else
			{
				pChar->m_bDynamicLight = true;
				pChar->m_vLightColor = g_CharLightList[GUN].vLightColor;
				pChar->m_vLightColor.x = 1.0f;
				pChar->m_iDLightType = GUN;
				pChar->m_fLightLife = g_CharLightList[GUN].fLife;
			}
		}
	}
	
	// 깃발에 힘 적용			   p
	GetWorld()->GetFlags()->CheckSpearing( v1, v2, BULLET_SPEAR_EMBLEM_POWER );
	if(Z_VIDEO_DYNAMICLIGHT)
		ZGetStencilLight()->AddLightSource( v1, 2.0f, 75 );
}

void ZGame::OnPeerShot_Shotgun(ZItem *pItem, ZCharacter* pOwnerCharacter, float fShotTime,
	rvector& pos, rvector& to, u32 seed)
{
	// 내 캐릭터 혹은 내가 보고있는 캐릭터
	ZCharacter *pTargetCharacter = ZGetGameInterface()->GetCombatInterface()->GetTargetCharacter();
	if(!pTargetCharacter) return;

	//// 디버그용 덤프
	//{
	//	if(strcmp("폭주천사다크",pTargetCharacter->GetProperty()->szName)==0) {
	//		mlog("shot : %3.3f \n",fShotTime);
	//	}
	//}


	MMatchItemDesc *pDesc = pItem->GetDesc();
	if(!pDesc) { _ASSERT(false); return; }

#define SHOTGUN_BULLET_COUNT	12
#define SHOTGUN_DIFFUSE_RANGE	0.1f

	bool waterSound = false;

	if (pOwnerCharacter == NULL) return;

	// 모든사람이 같은 random seed 를 갖도록 같은값으로 초기화 해준다
	/*int *seed=(int*)&fShotTime;
	srand(*seed);*/

	bool bHitGuard=false,bHitBody=false,bHitGround=false,bHitEnemy=false;
	rvector GuardPos,BodyPos,GroundPos;

	rvector v1, v2;

	rvector origdir = to - pos;
	Normalize(origdir);

	int nHitCount = 0;

	struct DamageInfo
	{
		int Damage = 0;
		float PiercingRatio = 0;
		ZDAMAGETYPE DamageType;
		MMatchWeaponType WeaponType;
	};

	std::unordered_map<MUID, DamageInfo> DamageMap;

	auto DirGen = GetShotgunPelletDirGenerator(origdir, seed);

	for(int i=0;i<SHOTGUN_BULLET_COUNT;i++)
	{
		auto dir = DirGen();

		rvector BulletMarkNormal;
		bool bBulletMark = false;
		ZTargetType nTargetType = ZTT_OBJECT;

		ZPICKINFO pickinfo;

		memset(&pickinfo,0,sizeof(ZPICKINFO));

		// 총알은 로켓이 통과하는곳도 통과한다
		const DWORD dwPickPassFlag=RM_FLAG_ADDITIVE | RM_FLAG_HIDE | RM_FLAG_PASSROCKET | RM_FLAG_PASSBULLET;

		bool Picked = ::PickHistory(pOwnerCharacter, pos, pos + 10000 * dir, GetWorld()->GetBsp(),
			pickinfo, MakePairValueAdapter(m_CharacterManager), fShotTime, dwPickPassFlag);

		//if(g_pGame->PickHistory(pOwnerCharacter,fShotTime,pos,pos+10000.f*dir,&pickinfo,dwPickPassFlag))
		if(Picked)
		{
			ZObject *pObject = pickinfo.pObject;
			if(pObject)
			{
				bool bGuard = pObject->IsGuard() && (pickinfo.info.parts!=eq_parts_legs) &&		// 다리는 막을수없다
								DotProduct(dir,pObject->GetDirection())<0;

				if(bGuard) {
					rvector t_pos = pObject->GetPosition();
					t_pos.z += 100.f;
					ZGetEffectManager()->AddSwordDefenceEffect(t_pos+(-dir*50.f),-dir);
					pObject->OnGuardSuccess();

					nTargetType = ZTT_CHARACTER_GUARD;
					bHitGuard=true;

				} else {

					ZActor* pATarget = MDynamicCast(ZActor,pObject);

					bool bPushSkip = false;

					if(pATarget) {
						bPushSkip = pATarget->GetNPCInfo()->bNeverPushed;
					}

					float fKnockbackForce = pItem->GetKnockbackForce() / (.5f*float(SHOTGUN_BULLET_COUNT));

					if(bPushSkip) {
//						ZGetSoundEngine()->PlaySound("fx_bullethit_mt_met");
						rvector vPos = pOwnerCharacter->GetPosition() + (pObject->GetPosition() - pOwnerCharacter->GetPosition()) * 0.1f;
						ZGetSoundEngine()->PlaySound("fx_bullethit_mt_met", vPos );
						fKnockbackForce = 1.0;
					}

					pObject->OnKnockback( dir, fKnockbackForce );

					float fActualDamage = CalcActualDamage(pOwnerCharacter, pObject, (float)pDesc->m_nDamage);
					float fRatio = ZItem::GetPiercingRatio( pDesc->m_nWeaponType , pickinfo.info.parts );
					ZDAMAGETYPE dt = (pickinfo.info.parts==eq_parts_head) ? ZD_BULLET_HEADSHOT : ZD_BULLET;

					if (ZGetGameClient()->GetMatchStageSetting()->GetNetcode() == NetcodeType::P2PLead
						&& pObject == m_pMyCharacter)
						pObject->OnDamaged(pOwnerCharacter, pOwnerCharacter->GetPosition(), dt, pDesc->m_nWeaponType, fActualDamage, fRatio );

					nTargetType = ZTT_CHARACTER;
					bHitBody=true;

					// 우리편 때린것은 combo 에 포함되지 않음
					if(!m_Match.IsTeamPlay() || (pTargetCharacter->GetTeamID()!=pObject->GetTeamID()))
					{
						bHitEnemy=true;
					}

					if (ZGetGameClient()->GetMatchStageSetting()->GetNetcode() == NetcodeType::P2PAntilead
						&& pOwnerCharacter == m_pMyCharacter && !IsReplay()
						|| ZGetGameClient()->GetMatchStageSetting()->GetNetcode() == NetcodeType::ServerBased)
					{
						auto Char = MDynamicCast(ZCharacter, pObject);

						if (Char)
						{
							int Damage = int(fActualDamage);

							auto& item = DamageMap[Char->GetUID()];

							int NewDamage = item.Damage + Damage;
							if (fRatio != item.PiercingRatio)
								item.PiercingRatio = (item.Damage * item.PiercingRatio + Damage * fRatio) / NewDamage;

							item.Damage = NewDamage;
							static_assert(ZD_BULLET_HEADSHOT > ZD_BULLET, "Fix me");
							item.DamageType = max(item.DamageType, dt);
							item.WeaponType = pDesc->m_nWeaponType;
						}
					}
				}

				v1 = pos;
				v2 = pickinfo.info.vOut;

			}else
				if(pickinfo.bBspPicked)
		//		if(pickinfo.bBspPicked || (pCharacter == pOwnerCharacter))
				{
					bHitGround=true;
					nTargetType = ZTT_OBJECT;

					v1 = pos;
					v2 = pickinfo.bpi.PickPos;

					// 총탄 흔적
					BulletMarkNormal.x = pickinfo.bpi.pInfo->plane.a;
					BulletMarkNormal.y = pickinfo.bpi.pInfo->plane.b;
					BulletMarkNormal.z = pickinfo.bpi.pInfo->plane.c;
					Normalize(BulletMarkNormal);
					bBulletMark = true;

					// 맞는곳 반경 20cm 가 화면에 들어오면 그린다
					bool bDrawTargetEffects = isInViewFrustum(v2,20.f,RGetViewFrustum());
					if(bDrawTargetEffects)
						ZGetEffectManager()->AddBulletMark(v2,BulletMarkNormal);
				}
				else {
					_ASSERT(false);
					return;
				}
		}
		else {
			v1 = pos;
			v2 = pos+dir*10000.f;
			nTargetType	= ZTT_NOTHING;
		}

//#ifdef _DEBUG
//		if (pickinfo.bBspPicked)
//			ZChatOutputF("Client: Hit wall at %d, %d, %d",
//				(int)pickinfo.bpi.PickPos.x, (int)pickinfo.bpi.PickPos.y, (int)pickinfo.bpi.PickPos.z);
//		else
//			ZChatOutputF("Client: Hit nothing");
//#endif

		waterSound = GetWorld()->GetWaters()->CheckSpearing( v1, v2, 250, 0.3, !waterSound );
	}

	if (ZGetGameClient()->GetMatchStageSetting()->GetNetcode() == NetcodeType::P2PAntilead
		|| ZGetGameClient()->GetMatchStageSetting()->GetNetcode() == NetcodeType::ServerBased)
	{
		for (auto& Pair : DamageMap)
		{
			if (ZGetGameClient()->GetMatchStageSetting()->GetNetcode() == NetcodeType::P2PAntilead)
				ZPostAntileadDamage(Pair.first, Pair.second.Damage, Pair.second.PiercingRatio,
					Pair.second.DamageType, Pair.second.WeaponType);
			else if (ZGetConfiguration()->GetShowHitRegDebugOutput())
			{
				auto& Char = *m_CharacterManager.at(Pair.first);
				ZChatOutputF("Client: Hit %s for %d damage",
					Char.GetUserName(), Pair.second.Damage);
				v3 Head, Origin;
				Char.GetPositions(&Head, &Origin, fShotTime);
				ZChatOutputF("Client: Head: %d, %d, %d; origin: %d, %d, %d",
					int(Head.x), int(Head.y), int(Head.z),
					int(Origin.x), int(Origin.y), int(Origin.z));
			}
		}
	}

	
	/////////////////////////////////////////////////////
	if(bHitEnemy) {
		CheckStylishAction(pOwnerCharacter);
		CheckCombo(pOwnerCharacter, NULL,true);
	}


	ZApplication::GetSoundEngine()->PlaySEFire(pItem->GetDesc(), pos.x, pos.y, pos.z, (pOwnerCharacter==m_pMyCharacter));

	// 보이지 않으면 이펙트를 그릴필요는 없다
	if(!pOwnerCharacter->IsRendered()) return;

	rvector v[6];

	int _size = pOwnerCharacter->GetWeapondummyPos(v);

	rvector dir = to - pos;
	Normalize(dir);

	ZGetEffectManager()->AddShotgunEffect(pos,v[1],dir,pOwnerCharacter);

	// 총 쏠때 라이트 추가
	ZCharacter* pChar;
	if( ZGetConfiguration()->GetVideo()->bDynamicLight && pOwnerCharacter != NULL )
	{
		pChar = pOwnerCharacter;

		if( pChar->m_bDynamicLight )
		{
			pChar->m_vLightColor = g_CharLightList[SHOTGUN].vLightColor;
			pChar->m_fLightLife = g_CharLightList[SHOTGUN].fLife;
		}
		else
		{
			pChar->m_bDynamicLight = true;
			pChar->m_vLightColor = g_CharLightList[SHOTGUN].vLightColor;
			pChar->m_vLightColor.x = 1.0f;
			pChar->m_iDLightType = SHOTGUN;
			pChar->m_fLightLife = g_CharLightList[SHOTGUN].fLife;
		}
	}

//	m_flags.CheckSpearing( v1, v2, SHOTGUN_SPEAR_EMBLEM_POWER );
	if(Z_VIDEO_DYNAMICLIGHT)
		ZGetStencilLight()->AddLightSource(v1, 2.0f, 200 );
}

// shot 을 shot_range, shot_melee, shot_shotgun 으로 command 를 각각 분리하는것도 방법이 좋을듯.
void ZGame::OnPeerShot(const MUID& uid, float fShotTime, rvector& pos, rvector& to,MMatchCharItemParts sel_type)
{
	ZCharacter* pOwnerCharacter = NULL;		// 총 쏜 사람

	pOwnerCharacter = m_CharacterManager.Find(uid);

	if (pOwnerCharacter == NULL) return;
	if(!pOwnerCharacter->IsVisible()) return;

	pOwnerCharacter->OnShot();

	u32 seed = reinterpret<u32>(fShotTime);

	// fShotTime 이 그 캐릭터의 로컬 시간이므로 내 시간으로 변환해준다
	fShotTime-=pOwnerCharacter->GetTimeOffset();

	ZItem *pItem = pOwnerCharacter->GetItems()->GetItem(sel_type);
	if(!pItem || !pItem->GetDesc()) return;

	// 비정상적인 발사속도를 무시한다.
	if (pOwnerCharacter->CheckValidShotTime(pItem->GetDescID(), fShotTime, pItem)) {
		pOwnerCharacter->UpdateValidShotTime(pItem->GetDescID(), fShotTime);
	} else {
		return;
	}

	//// 루프중 MEMORYHACK있었나 검사
	if (uid == ZGetMyUID()) {
		MDataChecker* pChecker = ZApplication::GetGame()->GetDataChecker();
		MDataCheckNode* pCheckNode = pChecker->FindCheck((BYTE*)pItem->GetAMagazinePointer());
		if (pCheckNode && (pCheckNode->UpdateChecksum()==false)) {
			pChecker->BringError();	//// MEMORYHACK 감지. Checksum 모두중단하고 끝장낸다.
		} else {
			int nAMagazine = pItem->GetBulletAMagazine();

			// 실제로 무기를 소비
			if (!pItem->Shot()) return;	//// SHOT ////

			//// MEMORYHACK 없었으면 리뉴한다.
			if (pItem->GetBulletAMagazine() < nAMagazine)	// Shot에서 총알 줄어야만 정상이다
				pChecker->RenewCheck((BYTE*)pItem->GetAMagazinePointer(), sizeof(int));
			else
				if(sel_type != MMCIP_MELEE)
					pChecker->BringError();
		}
	} else {
		// 실제로 무기를 소비
		if (!pItem->Shot()) {
//			_ASSERT(FALSE);	// 문제가있다, 치팅 ?
			return;	// SHOT
		}
	}

	// MELEE일 경우
	if (sel_type == MMCIP_MELEE)
	{
		OnPeerShot_Melee(uid,fShotTime);
		return;
	}

	if ((sel_type != MMCIP_PRIMARY) &&
		(sel_type != MMCIP_SECONDARY) &&
		(sel_type != MMCIP_CUSTOM1 )) return;


	if(!pItem->GetDesc()) return;
	MMatchWeaponType wtype = pItem->GetDesc()->m_nWeaponType;

	if(wtype == MWT_SHOTGUN)
	{
		OnPeerShot_Shotgun(pItem,pOwnerCharacter,fShotTime,pos,to, seed);
		return;
	}

	OnPeerShot_Range(sel_type,uid,fShotTime,pos,to, seed);

	rvector position;
	pOwnerCharacter->GetWeaponTypePos( weapon_dummy_muzzle_flash , &position );
	if( ZGetConfiguration()->GetVideo()->bDynamicLight )
	{
		RGetDynamicLightManager()->AddLight( GUNFIRE, position );
	}
}

void ZGame::OnPeerDie(const MUID& uidVictim, const MUID& uidAttacker)
{
	ZCharacter* pVictim = m_CharacterManager.Find(uidVictim);
	if (pVictim == NULL) return;

	pVictim->ActDead();

	if (pVictim == m_pMyCharacter)	
	{
		pVictim->Die();		// 여기서 실제로 죽는다. 나 자신은 실제로도 여기서 죽는것 처리

		// 팀플시 또는 퀘스트모드시 죽으면 옵져버 모드
		if (m_Match.IsWaitForRoundEnd())
		{
			if (m_CharacterManager.GetCount() > 2)
			{
				if (GetMatch()->GetMatchType() != MMATCH_GAMETYPE_DUEL)
					ReserveObserver();
			}
		}
#ifdef _QUEST
		else if (ZGetGameTypeManager()->IsQuestDerived(ZGetGameClient()->GetMatchStageSetting()->GetGameType()))
		{
			if (m_CharacterManager.GetCount() >= 2)
			{
				ReserveObserver();
			}
		}
#endif

		CancelSuicide();
	}


	ZCharacter* pAttacker = m_CharacterManager.Find(uidAttacker);
	if (pAttacker == NULL) return;
	if(pAttacker!=pVictim)
	{
		if (ZApplication::GetGame()->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DUEL)
		{
			if (pAttacker->GetKills() + 1 == 5)
			{
				pAttacker->GetStatus()->nFantastic++;
				pAttacker->AddIcon(ZCI_FANTASTIC);
			}
			else if (pAttacker->GetKills() + 1 == 15)
			{
				pAttacker->GetStatus()->nExcellent++;
				pAttacker->AddIcon(ZCI_EXCELLENT);
			}
			else if (pAttacker->GetKills() + 1 == 30)
			{
				pAttacker->GetStatus()->nUnbelievable++;
				pAttacker->AddIcon(ZCI_UNBELIEVABLE);
			}
		}
		else
		{
			if (pAttacker->GetStatus()->nKills>=3)
			{
				pAttacker->GetStatus()->nFantastic++;
				pAttacker->AddIcon(ZCI_FANTASTIC);
			}
		}

		if(pVictim->GetLastDamageType()==ZD_BULLET_HEADSHOT)
		{
			pAttacker->GetStatus()->nHeadShot++;
			pAttacker->AddIcon(ZCI_HEADSHOT);
		}
	}
}

void ZGame::OnPeerDead(const MUID& uidAttacker, const unsigned long int nAttackerArg, 
					   const MUID& uidVictim, const unsigned long int nVictimArg)
{
	ZCharacter* pVictim = m_CharacterManager.Find(uidVictim);
	ZCharacter* pAttacker = m_CharacterManager.Find(uidAttacker);

	bool bSuicide = false;
	if (uidAttacker == uidVictim) bSuicide = true;

	int nAttackerExp = 0, nVictimExp = 0;

	nAttackerExp = GetExpFromTransData(nAttackerArg);
	nVictimExp = -GetExpFromTransData(nVictimArg);

	if(pAttacker)
	{
		ZCharacterStatus* pAttackerCS = NULL;
		pAttackerCS = pAttacker->GetStatus();
		pAttackerCS->AddExp(nAttackerExp);

		if (!bSuicide) pAttackerCS->AddKills();
	}

	if(pVictim)
	{
		if (pVictim != m_pMyCharacter)
		{
			pVictim->Die();
		}

		ZCharacterStatus* pVictimCS = NULL;
		pVictimCS = pVictim->GetStatus();
		pVictimCS->AddExp(nVictimExp);
		pVictimCS->AddDeaths();
		if (pVictimCS->nLife > 0) pVictimCS->nLife--;
	}

	// 화면 경험치 이펙트 표시
	if(bSuicide && (ZGetCharacterManager()->Find(uidAttacker)==g_pGame->m_pMyCharacter)) 
	{
		// 자살
		ZGetScreenEffectManager()->AddExpEffect(nVictimExp);
		int nExpPercent = GetExpPercentFromTransData(nVictimArg);
		ZGetMyInfo()->SetLevelPercent(nExpPercent);

		ZGetScreenEffectManager()->SetGuageExpFromMyInfo();
	}
	else if(ZGetCharacterManager()->Find(uidAttacker)==m_pMyCharacter) 
	{
		// 내가 attacker 일때
		ZGetScreenEffectManager()->AddExpEffect(nAttackerExp);

		int nExpPercent = GetExpPercentFromTransData(nAttackerArg);
		ZGetMyInfo()->SetLevelPercent(nExpPercent);
		ZGetScreenEffectManager()->SetGuageExpFromMyInfo();
	}
	else if(ZGetCharacterManager()->Find(uidVictim)==m_pMyCharacter) 
	{
		// 내가 victim 일때
		ZGetScreenEffectManager()->AddExpEffect(nVictimExp);

		int nExpPercent = GetExpPercentFromTransData(nVictimArg);
		ZGetMyInfo()->SetLevelPercent(nExpPercent);
		ZGetScreenEffectManager()->SetGuageExpFromMyInfo();
	}

	m_Match.AddRoundKills();

	CheckKillSound(pAttacker);
	OnPeerDieMessage(pVictim, pAttacker);
}

void ZGame::CheckKillSound(ZCharacter* pAttacker)
{
	if ((!pAttacker) || (pAttacker != m_pMyCharacter)) return;

	if (m_Match.GetRoundKills() == 1)
	{
		ZApplication::GetSoundEngine()->PlayVoiceSound(VOICE_FIRST_KILL);
	}
}

void ZGame::OnReceiveTeamBonus(const MUID& uidChar, const unsigned long int nExpArg)
{
	ZCharacter* pCharacter = m_CharacterManager.Find(uidChar);
	if (pCharacter == NULL) return;

	int nExp = 0;

	nExp = GetExpFromTransData(nExpArg);

	if(pCharacter)
	{
		pCharacter->GetStatus()->AddExp(nExp);
	}

	// 화면 경험치 이펙트 표시
	if(pCharacter==m_pMyCharacter) 
	{
#ifdef _DEBUG
		char szTemp[128];
		sprintf_safe(szTemp, "TeamBonus = %d\n", nExp);
		OutputDebugString(szTemp);
#endif

		// 내가 attacker 일때
		ZGetScreenEffectManager()->AddExpEffect(nExp);

		
		int nExpPercent = GetExpPercentFromTransData(nExpArg);
		ZGetMyInfo()->SetLevelPercent(nExpPercent);
		ZGetScreenEffectManager()->SetGuageExpFromMyInfo();
	}
}

void ZGame::OnPeerDieMessage(ZCharacter* pVictim, ZCharacter* pAttacker)
{
	const char *testdeathnametable[ZD_END+1] = { "에러", "총", "칼", "추락", "폭발", "HEADSHOT", "마지막칼질" };
	char szMsg[256] = "";

	const char *szAnonymous = "?아무개?";

	char szVictim[256];
	strcpy_safe(szVictim, pVictim ? pVictim->GetUserAndClanName() : szAnonymous);

	char szAttacker[256];
	strcpy_safe(szAttacker, pAttacker ? pAttacker->GetUserAndClanName() : szAnonymous);

	// 자살일 경우
	if(pAttacker==pVictim)
	{
		if (pVictim == m_pMyCharacter)
		{
			if(m_pMyCharacter->GetLastDamageType()==ZD_EXPLOSION) {
//				sprintf_safe(szMsg, "당신은 자신의 폭탄으로 인하여 패배 하였습니다.");
				sprintf_safe( szMsg, ZMsg(MSG_GAME_LOSE_BY_MY_BOMB) );
			}
			else {
//				sprintf_safe(szMsg, "당신은 스스로 패배하였습니다.");
				sprintf_safe( szMsg, ZMsg(MSG_GAME_LOSE_MYSELF) );
			}

			ZChatOutput(MCOLOR(0xFFCF2020), szMsg);
		}
		else
		{
//			sprintf_safe(szMsg, "%s님이 스스로 패배하였습니다.", szAttacker);
			ZTransMsg( szMsg, MSG_GAME_WHO_LOSE_SELF, 1, szAttacker );
			ZChatOutput(MCOLOR(0xFF707070), szMsg);

			// Admin Grade
			if (ZGetMyInfo()->IsAdminGrade()) {
				MMatchObjCache* pCache = ZGetGameClient()->FindObjCache(ZGetMyUID());
				if (pCache && pCache->CheckFlag(MTD_PlayerFlags_AdminHide))
				{
					sprintf_safe( szMsg, "^%d%s^9 스스로 패배",
									(pAttacker->GetTeamID() == MMT_BLUE) ? 3 : 1,
									pAttacker->GetProperty()->szName);
					ZGetGameInterface()->GetCombatInterface()->m_AdminMsg.OutputChatMsg( szMsg);
				}
			}
		}

	}

	// 내가 죽였을 때
	else if (pAttacker == m_pMyCharacter)
	{
//		sprintf_safe(szMsg, "당신은 %s님으로부터 승리하였습니다.", szVictim );
		ZTransMsg( szMsg, MSG_GAME_WIN_FROM_WHO, 1, szVictim );
		ZChatOutput(MCOLOR(0xFF80FFFF), szMsg);
	}

	// 내가 죽었을 때
	else if (pVictim == m_pMyCharacter)
	{
//		sprintf_safe(szMsg, "당신은 %s님에게 패배하였습니다.", szAttacker );
		/*ZTransMsg( szMsg, MSG_GAME_LOSE_FROM_WHO, 1, szAttacker );
		ZChatOutput(MCOLOR(0xFFCF2020), szMsg);*/
		ZChatOutputF("%s has defeated you. (HP: %d / %d, AP: %d / %d)",
			pAttacker->GetProperty()->szName,
			pAttacker->GetHP(), (int)pAttacker->GetProperty()->fMaxHP,
			pAttacker->GetAP(), (int)pAttacker->GetProperty()->fMaxAP);
	}

	// 다른 사람이 다른 사람 죽였을때
	else
	{
// 		sprintf_safe(szMsg, "%s님이 %s님으로부터 승리하였습니다.", szAttacker, szVictim );
		ZTransMsg( szMsg, MSG_GAME_WHO_WIN_FROM_OTHER, 2, szAttacker, szVictim );
		ZChatOutput(MCOLOR(0xFF707070), szMsg);
		
		// Admin Grade
		if (ZGetMyInfo()->IsAdminGrade()) {
			MMatchObjCache* pCache = ZGetGameClient()->FindObjCache(ZGetMyUID());
			if (pCache && pCache->CheckFlag(MTD_PlayerFlags_AdminHide))
			{
				sprintf_safe( szMsg, "^%d%s^9 승리,  ^%d%s^9 패배",
							(pAttacker->GetTeamID() == MMT_BLUE) ? 3 : 1, pAttacker->GetProperty()->szName,
							(pVictim->GetTeamID() == MMT_BLUE) ? 3 : 1,   pVictim->GetProperty()->szName);
				ZGetGameInterface()->GetCombatInterface()->m_AdminMsg.OutputChatMsg( szMsg);
			}
		}
	}
}


void ZGame::OnReloadComplete(ZCharacter *pCharacter)
{
	ZItem* pItem = pCharacter->GetItems()->GetSelectedWeapon();
	//// 루프중 MEMORYHACK있었나 검사
	if (pCharacter->GetUID() == ZGetMyUID() && pItem!=NULL) {
		MDataChecker* pChecker = ZApplication::GetGame()->GetDataChecker();
		MDataCheckNode* pCheckNodeA = pChecker->FindCheck((BYTE*)pItem->GetBulletPointer());
		MDataCheckNode* pCheckNodeB = pChecker->FindCheck((BYTE*)pItem->GetAMagazinePointer());
		if ( (pCheckNodeA && (pCheckNodeA->UpdateChecksum()==false)) ||
		 	 (pCheckNodeB && (pCheckNodeB->UpdateChecksum()==false)) ) 
		{
			pChecker->BringError();	//// MEMORYHACK 감지. Checksum 모두중단하고 끝장낸다.
		} else {
			bool bResult = pCharacter->GetItems()->Reload();	//// RELOAD ////

			//// MEMORYHACK 없었으면 리뉴한다.
			pChecker->RenewCheck((BYTE*)pItem->GetBulletPointer(), sizeof(int));
			pChecker->RenewCheck((BYTE*)pItem->GetAMagazinePointer(), sizeof(int));
		}
	} else {
		bool bResult = pCharacter->GetItems()->Reload();	//// RELOAD ////
	}

	if(pCharacter==m_pMyCharacter) {
		ZApplication::GetSoundEngine()->PlaySound("we_weapon_rdy");
	}
	return;
}

void ZGame::OnPeerSpMotion(const MUID& uid,int nMotionType)
{
	ZCharacter* pCharacter = m_CharacterManager.Find(uid);

	if (pCharacter == NULL) return;

	pCharacter->m_bSpMotion = true;

	ZC_STATE_LOWER zsl = ZC_STATE_TAUNT;

		 if(nMotionType == ZC_SPMOTION_TAUNT) zsl = ZC_STATE_TAUNT;
	else if(nMotionType == ZC_SPMOTION_BOW  ) zsl = ZC_STATE_BOW;
	else if(nMotionType == ZC_SPMOTION_WAVE ) zsl = ZC_STATE_WAVE;
	else if(nMotionType == ZC_SPMOTION_LAUGH) zsl = ZC_STATE_LAUGH;
	else if(nMotionType == ZC_SPMOTION_CRY  ) zsl = ZC_STATE_CRY;
	else if(nMotionType == ZC_SPMOTION_DANCE) zsl = ZC_STATE_DANCE;

	pCharacter->m_SpMotion = zsl;

	pCharacter->SetAnimationLower( zsl );
}

void ZGame::OnPeerReload(const MUID& uid)
{
	ZCharacter* pCharacter = m_CharacterManager.Find(uid);
	//	if (uid == ZGetGameClient()->GetUID()) pCharacter = m_pMyCharacter;

	if (pCharacter == NULL || pCharacter->IsDie() ) return;

	// 임시.. 뭔가 상체애니메이션이 있으면 캔슬.. 리로드/샷 따위
	//	if(pCharacter->m_pVMesh->m_pAniSet[ani_mode_upper]!=NULL) return;

	// 내 캐릭터는 애니메이션이 끝날때에 실제로 리로드 시킨다.
	if(pCharacter==m_pMyCharacter)
		m_pMyCharacter->Animation_Reload();
	else
		OnReloadComplete(pCharacter);

	// Sound Effect
	if(pCharacter->GetItems()->GetSelectedWeapon()!=NULL) {
		rvector p = pCharacter->m_Position+rvector(0,0,160.f);
		ZApplication::GetSoundEngine()->PlaySEReload(pCharacter->GetItems()->GetSelectedWeapon()->GetDesc(), p.x, p.y, p.z, (pCharacter==m_pMyCharacter));
	}
}

void ZGame::OnPeerChangeCharacter(const MUID& uid)
{
	ZCharacter* pCharacter = m_CharacterManager.Find(uid);

	//	if (uid == ZGetGameClient()->GetUID()) pCharacter = m_pMyCharacter;

	if (pCharacter == NULL) return;

	pCharacter->TestToggleCharacter();
}

void ZGame::OnSetObserver(const MUID& uid)
{
	ZCharacter* pCharacter = m_CharacterManager.Find(uid);
	if (pCharacter == NULL) return;

	if(pCharacter==m_pMyCharacter)
	{
		ZApplication::GetGameInterface()->GetCombatInterface()->SetObserverMode(true);
	}
	pCharacter->SetVisible(false);
	pCharacter->ForceDie();
}

void ZGame::OnPeerSpawn(const MUID& uid, const rvector& pos, const rvector& dir)
{
	m_nSpawnTime = GetGlobalTimeMS();
	SetSpawnRequested(false);

	ZCharacter* pCharacter = m_CharacterManager.Find(uid);
	if (pCharacter == NULL) return;

	bool isRespawn	= ( pCharacter->IsDie() == true ) ? true : false;

	//	dir = rvector(-1.f,0,0);
	pCharacter->SetVisible(true);
	pCharacter->Revival();
	pCharacter->SetPosition(pos);
	pCharacter->SetDirection(dir);
	pCharacter->SetSpawnTime(GetTime());

	ZGetEffectManager()->AddReBirthEffect(pos);

	if(pCharacter==m_pMyCharacter)
	{
		m_pMyCharacter->InitSpawn();

		if( isRespawn )
		{
			ZGetSoundEngine()->PlaySound("fx_respawn");
		}
		else
		{
			ZGetSoundEngine()->PlaySound("fx_whoosh02");
		}

		ZGetScreenEffectManager()->ReSetHpPanel();// hppanel_ani
	}

#ifndef _PUBLISH
	char szLog[128];
	sprintf_safe(szLog, "ZGame::OnPeerSpawn() - %s(%u) Spawned \n", 
		pCharacter->GetProperty()->szName, pCharacter->GetUID().Low);
	OutputDebugString(szLog);
#endif

	if (GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DEATHMATCH_TEAM2)
		pCharacter->SetInvincibleTime( 5000);
}

void ZGame::OnPeerDash(MCommand* pCommand)
{
	MCommandParameter* pParam = pCommand->GetParameter(0);
	if(pParam->GetType()!=MPT_BLOB) return;

	MUID uid = pCommand->GetSenderUID();
	ZPACKEDDASHINFO* ppdi= (ZPACKEDDASHINFO*)pParam->GetPointer();

	rvector pos, dir;
	int sel_type;
	

	pos = rvector(Roundf(ppdi->posx),Roundf(ppdi->posy),Roundf(ppdi->posz));
	dir = 1.f/32000.f * rvector(ppdi->dirx,ppdi->diry,ppdi->dirz);
	sel_type = (int)ppdi->seltype;


	ZCharacter* pCharacter = m_CharacterManager.Find(uid);

	if (pCharacter == NULL) return;

	MMatchCharItemParts parts = (MMatchCharItemParts)sel_type;

	if( parts != pCharacter->GetItems()->GetSelectedWeaponParts()) {
		// 지금 들고 있는 무기와 보내진 무기가 틀리다면 보내진 무기로 바꿔준다..
		OnChangeWeapon(uid,parts);
	}

	ZGetEffectManager()->AddDashEffect(pos,dir,pCharacter);
}

rvector ZGame::GetFloor(rvector pos,rplane *pimpactplane)
{
	rvector floor=g_pGame->GetWorld()->GetBsp()->GetFloor(pos+rvector(0,0,120),CHARACTER_RADIUS-1.1f,58.f,pimpactplane);

#ifdef ENABLE_CHARACTER_COLLISION
	for (ZObjectManager::iterator itor = m_ObjectManager.begin();
		itor != m_ObjectManager.end(); ++itor)
	{
		ZObject* pObject = (*itor).second;
		if (pObject->IsCollideable())
//		if(!pCharacter->IsDie() && !pCharacter->m_bBlastDrop)
		{
			rvector diff=pObject->m_Position-pos;
			diff.z=0;

			// 나중에 radius상수값으로 된것 Object의 멤버변수로 고치자
			if(Magnitude(diff)<CHARACTER_RADIUS && pos.z>pObject->m_Position.z)
			{
				rvector newfloor=pObject->m_Position+rvector(0,0,pObject->GetCollHeight());
				if(floor.z<newfloor.z)
				{
					floor=newfloor;
					if(pimpactplane)
					{
						rvector up=rvector(0,0,1);
						D3DXPlaneFromPointNormal(pimpactplane,&floor,&up);
					}
				}
			}
		}
	}
#endif

	return floor;
}

bool ZGame::Pick(ZObject *pOwnerObject, const rvector &origin, const rvector &dir,ZPICKINFO *pickinfo,
	DWORD dwPassFlag,bool bMyChar)
{
	return PickHistory(pOwnerObject,GetTime(),origin,origin+10000.f*dir,pickinfo,dwPassFlag,bMyChar);
}

bool ZGame::PickTo(ZObject *pOwnerObject, const rvector &origin, const rvector &to,ZPICKINFO *pickinfo,
	DWORD dwPassFlag,bool bMyChar)
{
	return PickHistory(pOwnerObject,GetTime(),origin,to,pickinfo,dwPassFlag,bMyChar);
}

bool ZGame::PickHistory(ZObject *pOwnerObject,float fTime, const rvector &origin, const rvector &to,
	ZPICKINFO *pickinfo,DWORD dwPassFlag,bool bMyChar)
{
	pickinfo->pObject=NULL;
	pickinfo->bBspPicked=false;

	RPickInfo info;
	memset(&info,0,sizeof(RPickInfo));

	ZObject *pObject=NULL;

	bool bCheck = false;

	float fCharacterDist=FLT_MAX;			// 캐릭터들 사이의 최소거리지점을 찾는다
	for(ZObjectManager::iterator i=m_ObjectManager.begin();i!=m_ObjectManager.end();i++)
	{
		ZObject *pc=i->second;

		bCheck = false;

		if(bMyChar) {
			if(pc==pOwnerObject && pc->IsVisible()) {
				bCheck = true;
			}
		}
		else {
			if( pc!=pOwnerObject && pc->IsVisible() ) {
				bCheck = true;
			}
		}

		if( pc->IsDie() )//죽은넘이 몸빵한다고 해서~
			bCheck = false;


		if(bCheck)
		{
			rvector hitPos;
			ZOBJECTHITTEST ht = pc->HitTest(origin,to,fTime,&hitPos);
			if(ht!=ZOH_NONE) {
				float fDistToChar=Magnitude(hitPos-origin);
				if(fDistToChar<fCharacterDist) {
					pObject=pc;
					fCharacterDist=fDistToChar;
					info.vOut=hitPos;
					switch(ht) {
						case ZOH_HEAD : info.parts=eq_parts_head;break;
						case ZOH_BODY : info.parts=eq_parts_chest;break;
						case ZOH_LEGS :	info.parts=eq_parts_legs;break;
					}
				}
			}
		}
	}

	RBSPPICKINFO bpi;
	bool bBspPicked = GetWorld()->GetBsp()->PickTo(origin, to, &bpi, dwPassFlag);

	int nCase=0;

	if(pObject && bBspPicked)		// 둘다 맞았을때는 거리가 가까운쪽을 택한다.
	{
		if(Magnitude(info.vOut-origin)>Magnitude(bpi.PickPos-origin))
			nCase=1;
		else
			nCase=2;
	}else
		if(bBspPicked)				// 둘중 하나만 맞았으면 맞은걸 택하면 된다.
			nCase=1;
		else
			if(pObject)
				nCase=2;

	if(nCase==0) return false;

	switch(nCase)
	{
	case 1 :						// 맵에 맞은경우
		pickinfo->bBspPicked=true;
		pickinfo->bpi=bpi;
		break;
	case 2 :						// 사람에 맞은경우.
		pickinfo->pObject=pObject;
		pickinfo->info=info;
		break;
	}
	return true;
}

bool ZGame::ObjectColTest(ZObject* pOwner, const rvector& origin, const rvector& to, float fRadius, ZObject** poutTarget)
{
	// 맵에 맞는것은 체크하지 않는다.

	for(ZObjectManager::iterator i=m_ObjectManager.begin();i!=m_ObjectManager.end();i++)
	{
		ZObject *pc=i->second;

		bool bCheck = true;

		if(pc == pOwner || !pc->IsVisible())  bCheck = false;
		if( pc->IsDie() ) bCheck = false;

		if(bCheck)
		{
			if (pc->ColTest(origin, to, fRadius, GetTime()))
			{
				*poutTarget = pc;
				return true;
			}
		}
	}

	return false;
}

char* ZGame::GetSndNameFromBsp(const char* szSrcSndName, RMATERIAL* pMaterial)
{
	char szMaterial[256] = "";
	static char szRealSndName[256] = "";
	szRealSndName[0] = 0;

	if (pMaterial == NULL) return "";

	strcpy_safe(szMaterial, pMaterial->Name.c_str());

	size_t nLen = strlen(szMaterial);

#define ZMETERIAL_SNDNAME_LEN 7

	if ((nLen > ZMETERIAL_SNDNAME_LEN) && 
		(!_strnicmp(&szMaterial[nLen-ZMETERIAL_SNDNAME_LEN+1], "mt", 2)))
	{
		strcpy_safe(szRealSndName, szSrcSndName);
		strcat(szRealSndName, "_");
		strcat(szRealSndName, &szMaterial[nLen-ZMETERIAL_SNDNAME_LEN+1]);
	}
	else
	{
		strcpy_safe(szRealSndName, szSrcSndName);
	}


	return szRealSndName;
}

void ZGame::AutoAiming()
{
}

#define MAX_PLAYERS		64

void ZGame::PostHPAPInfo()
{
	DWORD nNowTime = GetTickTime();

	if (m_pMyCharacter->GetInitialized() == false) return;

	if ((nNowTime - m_nLastTime[ZLASTTIME_HPINFO]) >= PEER_HP_TICK)
	{
		m_nLastTime[ZLASTTIME_HPINFO] = nNowTime;

		ZPostHPAPInfo(m_pMyCharacter->GetHP(), m_pMyCharacter->GetAP());
	}
}

void ZGame::PostHPInfo()
{
	DWORD nNowTime = GetTickTime();

	if (m_pMyCharacter->GetInitialized() == false) return;

	if ((nNowTime - m_nLastTime[ZLASTTIME_HPINFO]) >= PEER_HP_TICK)
	{
		m_nLastTime[ZLASTTIME_HPINFO] = nNowTime;

		ZPostHPInfo(m_pMyCharacter->GetHP());
	}
}

static void PostOldBasicInfo(float Time, ZMyCharacter* MyChar)
{
	ZPACKEDBASICINFO pbi;
	pbi.fTime = Time;

	pbi.posx = MyChar->m_Position.x;
	pbi.posy = MyChar->m_Position.y;
	pbi.posz = MyChar->m_Position.z;

	pbi.velx = MyChar->GetVelocity().x;
	pbi.vely = MyChar->GetVelocity().y;
	pbi.velz = MyChar->GetVelocity().z;

	pbi.dirx = MyChar->GetTargetDir().x * 32000;
	pbi.diry = MyChar->GetTargetDir().y * 32000;
	pbi.dirz = MyChar->GetTargetDir().z * 32000;

	pbi.upperstate = MyChar->GetStateUpper();
	pbi.lowerstate = MyChar->GetStateLower();
	pbi.selweapon = MyChar->GetItems()->GetSelectedWeaponParts();

	ZPOSTCMD1(MC_PEER_BASICINFO, MCommandParameterBlob(&pbi, sizeof(pbi)));
}

void ZGame::PostNewBasicInfo()
{
	char buf[64];

	size_t Offset = 1;
	size_t Size = 1;
	auto Write = [&](auto&& Val)
	{
		// Compiler crashes here z__z
		//static_assert(std::is_pointer<decltype(Val)>::value, "Don't do that please");
		memcpy(buf + Offset, &Val, sizeof(Val));
		Offset += sizeof(Val);
		Size += sizeof(Val);
	};

	u8 Flags{};

	float Time = GetTime();
	Write(Time);

	auto CheckLongPos = [&](auto&&... Vals) {
		for (auto&& Val : { Vals... })
		{
			if (Val > SHRT_MAX || Val < SHRT_MIN)
			{
				Flags |= static_cast<int>(BasicInfoFlags::LongPos);
				break;
			}
		}
	};

	auto&& Pos = m_pMyCharacter->GetPosition();
	CheckLongPos(Pos.x, Pos.y, Pos.z);

	if (Flags & static_cast<int>(BasicInfoFlags::LongPos))
		Write(Pos);
	else
		Write(MShortVector(Pos));

	auto&& Dir = m_pMyCharacter->GetDirection();
	MByteVector Dir8{ Dir * CHAR_MAX };
	Write(Dir8);

	auto Velocity = MShortVector{ m_pMyCharacter->GetVelocity() };
	Write(Velocity);
	
	auto CheckVelocity = [&](auto&&... Vals) {
		for (auto&& Val : { Vals... })
			if (Val > SHRT_MAX || Val < SHRT_MIN)
				return false;

		return true;
	};
	assert(CheckVelocity(Velocity.x, Velocity.y, Velocity.z));

	if (m_pMyCharacter->IsDirLocked())
	{
		Flags |= static_cast<int>(BasicInfoFlags::CameraDir);
		MByteVector CameraDir{ ZGetCamera()->GetCurrentDir() * CHAR_MAX };
		Write(CameraDir);
	}

	auto Lower = static_cast<u8>(m_pMyCharacter->GetStateLower());
	auto Upper = static_cast<u8>(m_pMyCharacter->GetStateUpper());
	if (LastNetLowerAni != Lower
		|| LastNetUpperAni != Upper)
	{
		Flags |= static_cast<int>(BasicInfoFlags::Animations);
		Write(Lower);
		Write(Upper);
		LastNetLowerAni = static_cast<ZC_STATE_LOWER>(Lower);
		LastNetUpperAni = static_cast<ZC_STATE_UPPER>(Upper);
	}

	auto Slot = static_cast<u8>(m_pMyCharacter->GetItems()->GetSelectedWeaponParts());
	if (LastNetSlot != Slot)
	{
		Flags |= static_cast<int>(BasicInfoFlags::SelItem);
		Write(Slot);
		LastNetSlot = Slot;
	}

	buf[0] = Flags;

	ZPOSTCMD1(MC_PEER_BASICINFO_RG, MCommandParameterBlob(&buf, Size));
}

void ZGame::PostBasicInfo()
{
	if (m_pMyCharacter->GetInitialized() == false) return;

	if(m_pMyCharacter->IsDie() && GetTime()-m_pMyCharacter->m_fDeadTime>5.f) return;

	int nMoveTick = 20;
	if (g_Rules.IsVanillaMode())
		nMoveTick = (ZGetGameClient()->GetAllowTunneling() == false) ? PEERMOVE_TICK : PEERMOVE_AGENT_TICK;
	else
		nMoveTick = 20;

	auto nNowTime = GetGlobalTimeMS();
	if (nNowTime - m_nLastTime[ZLASTTIME_BASICINFO] >= nMoveTick)
	{
		m_nLastTime[ZLASTTIME_BASICINFO] = nNowTime;

		if (g_Rules.IsVanillaMode())
			PostOldBasicInfo(GetTime(), m_pMyCharacter);
		else
			PostNewBasicInfo();
	}
}

void ZGame::PostPeerPingInfo()
{
	if (!ZGetGameInterface()->GetCombatInterface()->IsShowScoreBoard()) return;

	if (ZGetGameClient()->GetMatchStageSetting()->GetNetcode() == NetcodeType::ServerBased)
		return;

	auto nNowTime = GetTickTime();

	if ((nNowTime - m_nLastTime[ZLASTTIME_PEERPINGINFO]) >= PEER_PING_TICK) {
		m_nLastTime[ZLASTTIME_PEERPINGINFO] = nNowTime;

		unsigned long nTimeStamp = GetTickTime();
		MMatchPeerInfoList* pPeers = ZGetGameClient()->GetPeers();
		for (MMatchPeerInfoList::iterator itor = pPeers->begin(); itor != pPeers->end(); ++itor) {
			MMatchPeerInfo* pPeerInfo = (*itor).second;
			if (pPeerInfo->uidChar != ZGetGameClient()->GetPlayerUID()) {
				_ASSERT(pPeerInfo->uidChar != MUID(0,0));

				MCommandManager* MCmdMgr = ZGetGameClient()->GetCommandManager();
				MCommand* pCmd = new MCommand(MCmdMgr->GetCommandDescByID(MC_PEER_PING), 
					pPeerInfo->uidChar, ZGetGameClient()->GetUID());	
				pCmd->AddParameter(new MCmdParamUInt(nTimeStamp));
				ZGetGameClient()->Post(pCmd);

#ifdef _DEBUG
				g_nPingCount++;
#endif
				pPeerInfo->SetLastPingTime(nTimeStamp);
			}
		}
	}
}

void ZGame::PostSyncReport()
{
	DWORD nNowTime = GetTickTime();

#ifdef _PUBLISH
	if ((nNowTime - m_nLastTime[ZLASTTIME_SYNC_REPORT]) >= MATCH_CYCLE_CHECK_SPEEDHACK) {
#else
	if ((nNowTime - m_nLastTime[ZLASTTIME_SYNC_REPORT]) >= 1000) {
#endif
		m_nLastTime[ZLASTTIME_SYNC_REPORT] = nNowTime;
		int nDataChecksum = 0;
		if (m_DataChecker.UpdateChecksum() == false) {
			nDataChecksum = m_DataChecker.GetChecksum();
			ZGetApplication()->Exit();
		}

		ZPOSTCMD2(MC_MATCH_GAME_REPORT_TIMESYNC, MCmdParamUInt(nNowTime), MCmdParamUInt(nDataChecksum));
	}
}

void ZGame::CheckCombo(ZCharacter *pOwnerCharacter, ZObject *pHitObject, bool bPlaySound)
{
	if(pOwnerCharacter==pHitObject) return;

	ZCharacter *pTargetCharacter = ZGetGameInterface()->GetCombatInterface()->GetTargetCharacter();
	if(!pTargetCharacter) return;

	if(pTargetCharacter!=pOwnerCharacter) return;

	if(pHitObject)
	{
		if(pHitObject->IsDie()) return;
	}	

	if (IsPlayerObject(pHitObject))
	{
		if(m_Match.IsTeamPlay() && (pTargetCharacter->GetTeamID()==((ZCharacter*)(pHitObject))->GetTeamID()))
			return;

		if (m_Match.IsQuestDrived()) return;
	}

	UpdateCombo(true);

	if (Z_AUDIO_HITSOUND )
	{
#ifdef _BIRDSOUND
		ZGetSoundEngine()->PlaySound("fx_myhit", 128);
#else
		if(bPlaySound && ZGetSoundEngine()->Get3DSoundUpdate())
			ZGetSoundEngine()->PlaySound("fx_myhit");
#endif
	}
}

void ZGame::UpdateCombo(bool bShot)
{
	// 내 캐릭터 혹은 내가 보고있는 캐릭터
	ZCharacter *pTargetCharacter = ZGetGameInterface()->GetCombatInterface()->GetTargetCharacter();
	if(!pTargetCharacter) return;

	// test
	static DWORD nLastShotTime = GetGlobalTimeMS();
	DWORD nNowTime = GetGlobalTimeMS();

	if (bShot) 
	{
		if(pTargetCharacter->GetStatus()->nCombo<2) {
			// hit 이펙트
			ZGetScreenEffectManager()->AddHit();
		}

		if ((nNowTime - nLastShotTime) < 700)
		{
			pTargetCharacter->GetStatus()->nCombo++;
			if (pTargetCharacter->GetStatus()->nCombo > MAX_COMBO) 
				pTargetCharacter->GetStatus()->nCombo = 1;
		}
		nLastShotTime = nNowTime;
	}
	else
	{
		if ((pTargetCharacter->GetStatus()->nCombo > 0) && ((nNowTime - nLastShotTime) > 1000))
		{
			pTargetCharacter->GetStatus()->nCombo = 0;
		}
	}
}


void ZGame::CheckStylishAction(ZCharacter* pCharacter)
{
	if (pCharacter->GetStylishShoted())
	{
		if (pCharacter == m_pMyCharacter)
		{
			ZGetScreenEffectManager()->AddCool();
		}
	}
}

#define RESERVED_OBSERVER_TIME	5000

void ZGame::OnReserveObserver()
{
	auto currentTime = GetGlobalTimeMS();

	if (currentTime - m_nReservedObserverTime > RESERVED_OBSERVER_TIME)
	{

		if ((m_Match.GetRoundState() == MMATCH_ROUNDSTATE_PLAY) ||
			(m_Match.IsWaitForRoundEnd() && ZGetGameClient()->IsForcedEntry())
			)
		{
			ZGetGameInterface()->GetCombatInterface()->SetObserverMode(true);
			m_bReserveObserver = false;
		}
		else
		{
			m_bReserveObserver = false;
		}

	}
}

void ZGame::ReserveObserver()
{
	m_bReserveObserver = true;
	m_nReservedObserverTime = GetGlobalTimeMS();
}

void ZGame::ReleaseObserver()
{
	if(!m_bReplaying)
	{
		m_bReserveObserver = false;
		ZGetGameInterface()->GetCombatInterface()->SetObserverMode(false);

		FlushObserverCommands();
	}
}

void ZGame::OnInvalidate()
{
	GetWorld()->OnInvalidate();
	ZGetFlashBangEffect()->OnInvalidate();
	m_CharacterManager.OnInvalidate();
}

void ZGame::OnRestore()
{
	GetWorld()->OnRestore();
	ZGetFlashBangEffect()->OnRestore();
	m_CharacterManager.OnRestore();
}

void ZGame::InitRound()
{
//	m_fTime=0;
	SetSpawnRequested(false);
	ZGetGameInterface()->GetCamera()->StopShock();

	ZGetFlashBangEffect()->End();

	ZGetEffectManager()->Clear();
	m_WeaponManager.Clear();

#ifdef _WORLD_ITEM_
	//ZGetWorldItemManager()->Reset();
#endif

	ZGetCharacterManager()->InitRound();
}

void ZGame::AddEffectRoundState(MMATCH_ROUNDSTATE nRoundState, int nArg)
{

	switch(nRoundState) 
	{

	case MMATCH_ROUNDSTATE_COUNTDOWN : 
		{
			// 이팩트 출력 - 팀플일 경우만 RoundStart이팩트 출력한다. 듀얼은 라운드 기다리지만 제외.
			if (m_Match.IsWaitForRoundEnd() && m_Match.GetMatchType() != MMATCH_GAMETYPE_DUEL)
			{
				if(m_Match.GetCurrRound()+1==m_Match.GetRoundCount())
				{
					ZGetScreenEffectManager()->AddFinalRoundStart();
				}
				else
				{
					// m_nCurrRound 가 0이면 1라운드이다.
					ZGetScreenEffectManager()->AddRoundStart(m_Match.GetCurrRound()+1);
				}
			}
		}
		break;
	case MMATCH_ROUNDSTATE_PLAY:
		{
			ZGetScreenEffectManager()->AddRock();
		}
		break;
	case MMATCH_ROUNDSTATE_FINISH:
		{
			if (m_Match.IsTeamPlay())
			{
				int nRedTeam, nBlueTeam;
				m_Match.GetTeamAliveCount(&nRedTeam, &nBlueTeam);

				if (nArg == MMATCH_ROUNDRESULT_DRAW)
				{
					if (GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DEATHMATCH_TEAM2)
					{
						MMatchTeam nMyTeam = (MMatchTeam)m_pMyCharacter->GetTeamID();
						MMatchTeam nEnemyTeam = (nMyTeam == MMT_BLUE ? MMT_RED : MMT_BLUE);

						int nMyScore = GetMatch()->GetTeamKills(nMyTeam);
						int nEnemyScore = GetMatch()->GetTeamKills(nEnemyTeam);

						if (nMyScore > nEnemyScore)
							ZGetScreenEffectManager()->AddWin();
						else if (nMyScore < nEnemyScore)
							ZGetScreenEffectManager()->AddLose();
						else
							ZGetScreenEffectManager()->AddDraw();
					}
					else
						ZGetScreenEffectManager()->AddDraw();
				}
				else 
				{
					if (nArg == MMATCH_ROUNDRESULT_DRAW) {
						// Do nothing
					} else {
						MMatchTeam nMyTeam = (MMatchTeam)m_pMyCharacter->GetTeamID();
						MMatchTeam nTeamWon = (nArg == MMATCH_ROUNDRESULT_REDWON ? MMT_RED : MMT_BLUE);

						// 만약 강제로 팀이 바껴진 경우에는 반대편
						if (ZGetMyInfo()->GetGameInfo()->bForcedChangeTeam)
						{
							nMyTeam = NegativeTeam(nMyTeam);
						}

						// Spectator 일경우 처리
						if (ZGetGameInterface()->GetCombatInterface()->GetObserver()->IsVisible()) {
							ZCharacter* pTarget = ZGetGameInterface()->GetCombatInterface()->GetObserver()->GetTargetCharacter();
							if (pTarget)
								nMyTeam = (MMatchTeam)pTarget->GetTeamID();
						}

						if (nTeamWon == nMyTeam)
						{
							ZGetScreenEffectManager()->AddWin();
						} else {
							ZGetScreenEffectManager()->AddLose();
						}
					}
				}

				int nTeam = 0;

				// all kill 판정
				for(int j=0;j<2;j++)
				{
					bool bAllKill=true;
					ZCharacter *pAllKillPlayer=NULL;

					for (ZCharacterManager::iterator itor = ZGetCharacterManager()->begin();
						itor != ZGetCharacterManager()->end(); ++itor)
					{
						ZCharacter* pCharacter = (*itor).second;
						if (pCharacter == NULL) return;
						
						if(j==0) {	
							nTeam = MMT_RED;
						} 
						else if(j==1) { 
							nTeam = MMT_BLUE;	
						}	

						if(pCharacter->GetTeamID() != nTeam) 
							continue;

						if(pCharacter->IsDie())
						{
							ZCharacter *pKiller = ZGetCharacterManager()->Find(pCharacter->GetLastAttacker());
							if(pAllKillPlayer==NULL)
							{
								if(!pKiller || pKiller->GetTeamID()==nTeam)			// 같은편한테 죽었으면 꽝
								{
									bAllKill=false;
									break;
								}

								pAllKillPlayer=pKiller;
							}
							else
								if(pAllKillPlayer!=pKiller)	// 여러명이 나눠 죽였으면 꽝
								{
									bAllKill=false;
									break;
								}
						}else
						{
							bAllKill=false;											// 살아있는 넘이 있어도 꽝
							break;
						}
					}

					if((bAllKill) && (pAllKillPlayer))
					{
						pAllKillPlayer->GetStatus()->nAllKill++;
						pAllKillPlayer->AddIcon(ZCI_ALLKILL);
					}
				}
			}


			// 듀얼 모드일 경우
			else if ( ZApplication::GetGame()->GetMatch()->GetMatchType() == MMATCH_GAMETYPE_DUEL)
			{
				ZRuleDuel* pDuel = (ZRuleDuel*)ZGetGameInterface()->GetGame()->GetMatch()->GetRule();
				if ( pDuel)
				{
					bool bAddWin = false;
					bool bAddLose = false;
					int nCount = 0;				// 챔피언과 도전자 모두 게임중이였는지 체크하기 위해서...


					// 옵져버 모드일때
					MUID uidTarget;
					ZObserver* pObserver = ZGetGameInterface()->GetCombatInterface()->GetObserver();
					if ( pObserver && pObserver->IsVisible())
						uidTarget = pObserver->GetTargetCharacter()->GetUID();

					// 옵져버 모드가 아닐때
					else
						uidTarget = m_pMyCharacter->GetUID();


					for (ZCharacterManager::iterator itor = ZGetCharacterManager()->begin(); itor != ZGetCharacterManager()->end(); ++itor)
					{
						ZCharacter* pCharacter = (*itor).second;

						// Is champion or challenger
						if ( (pCharacter->GetUID() == pDuel->QInfo.m_uidChampion) || (pCharacter->GetUID() == pDuel->QInfo.m_uidChallenger))
						{
							if ( uidTarget == pCharacter->GetUID())
							{
								if ( pCharacter->IsDie())
									bAddLose |= true;
								else
									bAddWin |= true;
							}
							else
							{
								if ( pCharacter->IsDie())
									bAddWin |= true;
								else
									bAddLose |= true;
							}


							// 챔피온과 도전자 수를 모두 더해서 2가 되어야 한다
							nCount++;
						}
					}


					// Draw
					if ( (nCount < 2) || (bAddWin == bAddLose))
						ZGetScreenEffectManager()->AddDraw();
					
					// Win
					else if ( bAddWin)
						ZGetScreenEffectManager()->AddWin();

					// Lose
					else
						ZGetScreenEffectManager()->AddLose();
				}
			}
		}
		break;
	};

}

void ZGame::StartRecording()
{
	int nsscount=0;

	char replayfilename[_MAX_PATH];
	char replayfoldername[_MAX_PATH];

	TCHAR szPath[MAX_PATH];
	if(GetMyDocumentsPath(szPath)) {
		strcpy_safe(replayfoldername,szPath);
		strcat(replayfoldername,GUNZ_FOLDER);
		CreatePath( replayfoldername );
		strcat(replayfoldername,REPLAY_FOLDER);
		CreatePath( replayfoldername );
	}

	do {
		sprintf_safe(replayfilename, "%s/Gunz%03d." GUNZ_REC_FILE_EXT, replayfoldername, nsscount);
		m_nGunzReplayNumber = nsscount;
		nsscount++;
	}
	while( IsExist(replayfilename) && nsscount<1000);

	if(nsscount==1000) goto RECORDING_FAIL;

	m_pReplayFile = zfopen(replayfilename,true);
	if(!m_pReplayFile) goto RECORDING_FAIL;

	int nWritten;
	bool Success = false;

	u32 header;
	header = RG_REPLAY_MAGIC_NUMBER;//GUNZ_REC_FILE_ID;
	nWritten = zfwrite(&header,sizeof(header),1,m_pReplayFile);
	if(nWritten==0) goto RECORDING_FAIL;

	header = RG_REPLAY_BINARY_VERSION;//GUNZ_REC_FILE_VERSION;
	nWritten = zfwrite(&header,sizeof(header),1,m_pReplayFile);
	if(nWritten==0) goto RECORDING_FAIL;

	if (!m_pReplayFile->Write<u32>(RGUNZ_VERSION))
		goto RECORDING_FAIL;

	i64 Timestamp = time(nullptr);
	if (!m_pReplayFile->Write(Timestamp))
		goto RECORDING_FAIL;

	{
		REPLAY_STAGE_SETTING_NODE rssn;
		GetReplayStageSetting(rssn, *ZGetGameClient()->GetMatchStageSetting()->GetStageSetting());
		Success = m_pReplayFile->Write(rssn);
		if (!Success)
			goto RECORDING_FAIL;
	}

	// duel 모드일때 상태 추가 저장
	if(ZGetGameClient()->GetMatchStageSetting()->GetGameType() == MMATCH_GAMETYPE_DUEL)
	{
		ZRuleDuel* pDuel = (ZRuleDuel*)ZGetGameInterface()->GetGame()->GetMatch()->GetRule();
		nWritten = zfwrite(&pDuel->QInfo,sizeof(MTD_DuelQueueInfo),1,m_pReplayFile);
		if(nWritten==0) goto RECORDING_FAIL;
	}

	int nCharacterCount= (int)m_CharacterManager.size();
	nWritten = zfwrite(&nCharacterCount,sizeof(nCharacterCount),1,m_pReplayFile);
	if(nWritten==0) goto RECORDING_FAIL;

	for (auto& Item : m_CharacterManager)
	{
		auto Char = Item.second;
		ReplayPlayerInfo Info;
		Char->Save(Info);
		if (!m_pReplayFile->Write(Info))
			goto RECORDING_FAIL;
	}	

	nWritten = zfwrite(&m_fTime,sizeof(m_fTime),1,m_pReplayFile);
	if(nWritten==0) goto RECORDING_FAIL;


	m_bRecording=true;
	ZChatOutput(MCOLOR(ZCOLOR_CHAT_SYSTEM), 
		ZMsg(MSG_RECORD_STARTING));
	return;

RECORDING_FAIL:

	if(m_pReplayFile)
	{
		zfclose(m_pReplayFile);
		m_pReplayFile = NULL;
	}

	ZChatOutput(MCOLOR(ZCOLOR_CHAT_SYSTEM), ZMsg(MSG_RECORD_CANT_SAVE));
}

void ZGame::StopRecording()
{
	if(!m_bRecording) return;

	bool bError = false;

	m_bRecording=false;

	ZObserverCommandList::iterator itr = m_ReplayCommandList.begin();
	for(size_t i=0;i<m_ReplayCommandList.size();i++)
	{
		ZObserverCommandItem *pItem = *itr;
		MCommand *pCommand = pItem->pCommand;

		const int BUF_SIZE = 1024;
		char CommandBuffer[BUF_SIZE];
		int nSize = pCommand->GetData(CommandBuffer, BUF_SIZE);

		int nWritten;
		nWritten = zfwrite(&pItem->fTime,sizeof(pItem->fTime),1,m_pReplayFile);
		if(nWritten==0) { bError=true; break; }
		nWritten = zfwrite(&pCommand->m_Sender,sizeof(pCommand->m_Sender),1,m_pReplayFile);
		if(nWritten==0) { bError=true; break; }
		nWritten = zfwrite(&nSize,sizeof(nSize),1,m_pReplayFile);
		if(nWritten==0) { bError=true; break; }
		nWritten = zfwrite(CommandBuffer,nSize,1,m_pReplayFile);
		if(nWritten==0) { bError=true; break; }

		itr++;
	}

	while(m_ReplayCommandList.size())
	{
		ZObserverCommandItem *pItem = *m_ReplayCommandList.begin();
		delete pItem->pCommand;
		delete pItem;
		m_ReplayCommandList.pop_front();
	}

	if(!zfclose(m_pReplayFile))
		bError = true;

	if(bError)
	{
		ZChatOutput(MCOLOR(ZCOLOR_CHAT_SYSTEM), ZMsg(MSG_RECORD_CANT_SAVE));
	}
	else
	{
		char szOutputFilename[256];
		sprintf_safe(szOutputFilename, GUNZ_FOLDER REPLAY_FOLDER "/Gunz%03d." GUNZ_REC_FILE_EXT, m_nGunzReplayNumber);

		char szOutput[256];
		// ZTranslateMessage(szOutput,MSG_RECORD_SAVED,1,szOutputFilename);
		ZTransMsg(szOutput,MSG_RECORD_SAVED,1,szOutputFilename);
		ZChatOutput(MCOLOR(ZCOLOR_CHAT_SYSTEM), szOutput);
	}

}



void ZGame::ToggleRecording()
{
	if(m_bReplaying)
		return;

	if (ZGetGameTypeManager()->IsQuestDerived(ZGetGameClient()->GetMatchStageSetting()->GetGameType()))
		return;

	if(!m_bRecording)
		StartRecording();
	else
		StopRecording();
}

DWORD dwReplayStartTime;

bool ZGame::OnLoadReplay(ZReplayLoader* pLoader)
{
	m_fTime = pLoader->GetGameTime();
	ReplayStartGameTime = m_fTime;

	m_bReplaying = true;
	SetReadyState(ZGAME_READYSTATE_RUN);
	GetMatch()->SetRoundState(MMATCH_ROUNDSTATE_FREE);
	ZGetGameInterface()->GetCombatInterface()->SetObserverMode(true);
	// This bugs out the replay when the target is an admin going into admin hiding
	//ZGetGameInterface()->GetCombatInterface()->GetObserver()->SetTarget(g_pGame->m_pMyCharacter->GetUID());
	g_bProfile=true;	
	dwReplayStartTime=GetGlobalTimeMS();

	ReplayIterator = m_ReplayCommandList.begin();
	return true;
}

void ZGame::EndReplay()
{
	g_bProfile = false;

	DWORD dwReplayEndTime=GetGlobalTimeMS();
	
	mlog("replay end. profile saved. playtime = %3.3f seconds , average fps = %3.3f \n",
		float(dwReplayEndTime-dwReplayStartTime)/1000.f,
		1000.f*g_nFrameCount/float(dwReplayEndTime-dwReplayStartTime));

	ZChangeGameState(GUNZ_LOBBY);
}

void ZGame::SetReplayTime(float Time)
{
	if (!m_bReplaying)
		return;

	if (Time < 0)
		return;

	auto it = m_ReplayCommandList.begin();
	for (; it != m_ReplayCommandList.end(); it++)
	{
		if ((*it)->fTime - ReplayStartGameTime >= Time)
			break;
	}

	if (it == m_ReplayCommandList.end())
		return;

	ReplayIterator = it;
	m_fTime = ReplayStartGameTime + Time;

	ClearObserverCommandList();
}

float ZGame::GetReplayTime() const
{
	if (!m_bReplaying)
		return -1;

	return m_fTime - ReplayStartGameTime;
}

float ZGame::GetReplayLength() const
{
	if (!m_bReplaying)
		return -1;

	return (*m_ReplayCommandList.crbegin())->fTime - ReplayStartGameTime;
}

void ZGame::ClearObserverCommandList()
{
	while (m_ObserverCommandList.begin() != m_ObserverCommandList.end())
	{
		ZObserverCommandItem *pItem = *m_ObserverCommandList.begin();

		m_ObserverCommandList.erase(m_ObserverCommandList.begin());

		delete pItem->pCommand;
		delete pItem;
	}
}

void ZGame::ConfigureCharacter(const MUID& uidChar, MMatchTeam nTeam, unsigned char nPlayerFlags)
{
	ZCharacterManager* pCharMgr = ZGetCharacterManager();
	ZCharacter* pChar = pCharMgr->Find(uidChar);
	if (pChar == NULL) return;

	pChar->SetAdminHide((nPlayerFlags & MTD_PlayerFlags_AdminHide) !=0);
	pChar->SetTeamID(nTeam);
	pChar->InitStatus();
	pChar->InitRound();

	ZGetCombatInterface()->OnAddCharacter(pChar);
}

void ZGame::RefreshCharacters()
{
	for (MMatchPeerInfoList::iterator itor = ZGetGameClient()->GetPeers()->begin();
		itor != ZGetGameClient()->GetPeers()->end(); ++itor)
	{
		MMatchPeerInfo* pPeerInfo = (*itor).second;
		ZCharacter* pCharacter = m_CharacterManager.Find(pPeerInfo->uidChar);

		if (!pCharacter) {

			pCharacter = m_CharacterManager.Add(pPeerInfo->uidChar, rvector(0.0f, 0.0f, 0.0f));
			pCharacter->Create(pPeerInfo->CharInfo);

			if (m_Match.GetRoundState() == MMATCH_ROUNDSTATE_PREPARE)
			{
				if (m_Match.IsTeamPlay())
				{
//					pCharacter->SetVisible(true);		// RAONHAJE: PeerOpened TEST
				}
			}


			/*
			// TODO: AGENT 통한 peer들에게는 씹힌다. 수정요망.

			//신입 캐릭터에게 자신의 무기를 알린다...

			ZCharacter* pMyCharacter = g_pGame->m_pMyCharacter;
			if(pMyCharacter)
			{
				//			if(pMyCharacter != pCharacter) { // 자신이 새로 생긴 캐릭이 아니라면
				int nParts = g_pGame->m_pMyCharacter->GetItems()->GetSelectedWeaponParts();
				ZPostChangeWeapon(nParts);
				//			}
			}
			*/
		}
	}
}

void ZGame::DeleteCharacter(const MUID& uid)
{
	bool bObserverDel = false;
	ZCharacter* pCharacter = ZGetCharacterManager()->Find(uid);

	// 옵져버 타겟인 경우 다른 타겟으로 바꿔준다.
	ZObserver* pObserver = ZGetGameInterface()->GetCombatInterface()->GetObserver();
	if (pObserver->IsVisible())
	{
		if ((pCharacter != NULL) && (pCharacter == pObserver->GetTargetCharacter()))
		{
			bObserverDel = true;				
		}
	}

	m_CharacterManager.Delete(uid);


	if (bObserverDel)
	{
		if (pObserver) pObserver->SetFirstTarget();
	}
}


void ZGame::OnStageEnterBattle(MCmdEnterBattleParam nParam, MTD_PeerListNode* pPeerNode)
{
	if (ZApplication::GetGameInterface()->GetState() != GUNZ_GAME) return;

	MUID uidChar = pPeerNode->uidChar;

	if (uidChar == ZGetMyUID())		// enter한사람이 나자신일 경우
	{
		if (g_pGame->CreateMyCharacter(pPeerNode->CharInfo) == true)
		{
			ConfigureCharacter(uidChar, (MMatchTeam)pPeerNode->ExtendInfo.nTeam, pPeerNode->ExtendInfo.nPlayerFlags);	// Player Character 포함
		}
	}
	else		// enter한사람이 나 자신이 아닐경우
	{
		OnAddPeer(pPeerNode->uidChar, pPeerNode->dwIP, pPeerNode->nPort, pPeerNode);
	}


	if (nParam == MCEP_FORCED)
	{
		ZCharacter* pChar = ZGetCharacterManager()->Find(uidChar);
		GetMatch()->OnForcedEntry(pChar);

		char temp[256] = "";
		if((pPeerNode->ExtendInfo.nPlayerFlags & MTD_PlayerFlags_AdminHide)==0) {
			ZTransMsg(temp, MSG_GAME_JOIN_BATTLE, 1, pChar->GetUserAndClanName());
			ZChatOutput(MCOLOR(ZCOLOR_GAME_INFO), temp);
		}
	}

	ZGetGameClient()->OnStageEnterBattle(uidChar,nParam,pPeerNode);
}

void ZGame::OnStageLeaveBattle(const MUID& uidChar, const MUID& uidStage)
{
	if (ZApplication::GetGameInterface()->GetState() != GUNZ_GAME) return;

	if (uidChar != ZGetMyUID()) {

		ZCharacter* pChar = ZGetCharacterManager()->Find(uidChar);

		if(pChar && !pChar->IsAdminHide()) {
			char temp[256] = "";
			ZTransMsg(temp, MSG_GAME_LEAVE_BATTLE, 1, pChar->GetUserAndClanName());
			ZChatOutput(MCOLOR(ZCOLOR_GAME_INFO), temp);
		}

		ZGetGameClient()->DeletePeer(uidChar);
		if (ZApplication::GetGameInterface()->GetState() == GUNZ_GAME) {
			DeleteCharacter(uidChar);
		}

		ZGetGameClient()->SetVoteInProgress( false );
		ZGetGameClient()->SetCanVote( false );
	}
}

void ZGame::OnAddPeer(const MUID& uidChar, DWORD dwIP, const int nPort, MTD_PeerListNode* pNode)
{
	if ((ZApplication::GetGameInterface()->GetState() != GUNZ_GAME) || (g_pGame == NULL)) return;

	/*
	//// UDPTEST LOG ////////////////////////////////
	char szLog[256];
	sprintf_safe(szLog, "[%d:%d] ADDPEER: Char(%d:%d) IP:%s, Port:%d \n", 
	GetPlayerUID().High, GetPlayerUID().Low, uidChar.High, uidChar.Low, szIP, nPort);
	mlog(szLog);
	/////////////////////////////////////////////////
	*/

	// 난입한 사람이 자기 자신이 아니면
	if (uidChar != ZGetMyUID())
	{
		if (pNode == NULL)
		{
			/* // -_-?
			MTD_CharInfo charinfo;
			pCharInfo = &charinfo;
			pCharInfo->szName[0] = 0;
			memset(pCharInfo->nEquipedItemDesc, 0, sizeof(pCharInfo->nEquipedItemDesc));
			*/
			_ASSERT(0);
		}

		ZGetGameClient()->DeletePeer(uidChar);	// Delete exist info

		MMatchPeerInfo* pNewPeerInfo = new MMatchPeerInfo;

		if (uidChar == MUID(0,0)) pNewPeerInfo->uidChar = MUID(0, nPort);	// 로컬테스트를 위해서
		else pNewPeerInfo->uidChar = uidChar;

		pNewPeerInfo->dwIP = dwIP;
		in_addr addr;
		addr.s_addr = dwIP;
		char* pszIP = inet_ntoa(addr);
		strcpy_safe(pNewPeerInfo->szIP, pszIP);

		pNewPeerInfo->nPort = nPort;
		memcpy(&pNewPeerInfo->CharInfo, &(pNode->CharInfo), sizeof(MTD_CharInfo));
		memcpy(&pNewPeerInfo->ExtendInfo, &(pNode->ExtendInfo), sizeof(MTD_ExtendInfo));

		ZGetGameClient()->AddPeer(pNewPeerInfo);	

		RefreshCharacters();
	}

	ConfigureCharacter(uidChar, (MMatchTeam)pNode->ExtendInfo.nTeam, pNode->ExtendInfo.nPlayerFlags);	// Player Character 포함
}

void ZGame::OnPeerList(const MUID& uidStage, void* pBlob, int nCount)
{
	if (ZGetGameClient()->GetStageUID() != uidStage) return;
	if (ZApplication::GetGameInterface()->GetState() != GUNZ_GAME) return;
	if ((ZApplication::GetGame() == NULL) || (ZGetCharacterManager() == NULL)) return;

	for(int i=0; i<nCount; i++) {
		MTD_PeerListNode* pNode = (MTD_PeerListNode*)MGetBlobArrayElement(pBlob, i);
		OnAddPeer(pNode->uidChar, pNode->dwIP, pNode->nPort, pNode);

		ZCharacter* pChar = ZGetCharacterManager()->Find(pNode->uidChar);
		if (pChar)
		{
			pChar->SetVisible(false);
		}
	}
}

void ZGame::OnGameRoundState(const MUID& uidStage, int nRound, int nRoundState, int nArg)
{
	if (ZApplication::GetGameInterface()->GetState() != GUNZ_GAME) return;
	ZMatch* pMatch = GetMatch();
	if (pMatch == NULL) return;

	MMATCH_ROUNDSTATE RoundState = MMATCH_ROUNDSTATE(nRoundState);

	// 프리모드일경우
	if ((RoundState == MMATCH_ROUNDSTATE_FREE) && (pMatch->GetRoundState() != RoundState))
	{
		pMatch->InitCharactersPosition();
		m_pMyCharacter->SetVisible(true);
		m_pMyCharacter->Revival();
		ReleaseObserver();
	}

	pMatch->SetRound(nRound);
	pMatch->SetRoundState(RoundState, nArg);
	AddEffectRoundState(RoundState, nArg);

	if (RoundState == MMATCH_ROUNDSTATE_FINISH)
	{
		ZGetMyInfo()->GetGameInfo()->InitRound();	// 제일 마지막에 불려져야 한다.
	}
}


bool ZGame::FilterDelayedCommand(MCommand *pCommand)
{
	bool bFiltered = true;
	float fDelayTime = 0;

	MUID uid=pCommand->GetSenderUID();
	ZCharacter *pChar=ZGetCharacterManager()->Find(uid);
	if(!pChar) return false;

	switch (pCommand->GetID())
	{
		case MC_PEER_SKILL:
			{
				int nSkill;
				pCommand->GetParameter(&nSkill, 0, MPT_INT);
				fDelayTime = .15f;
				switch(nSkill)	{
					case ZC_SKILL_UPPERCUT		: 
						if(pChar!=m_pMyCharacter) pChar->SetAnimationLower(ZC_STATE_LOWER_UPPERCUT);
						break;
					case ZC_SKILL_SPLASHSHOT	: break;
					case ZC_SKILL_DASH			: break;
				}

				////////////////////////////////////////////////////////////////////
				int sel_type;
				pCommand->GetParameter(&sel_type, 2, MPT_INT);
				MMatchCharItemParts parts = (MMatchCharItemParts)sel_type;
				if( parts != pChar->GetItems()->GetSelectedWeaponParts()) {
					// 지금 들고 있는 무기와 보내진 무기가 틀리다면 보내진 무기로 바꿔준다..
					OnChangeWeapon(uid,parts);
				}
				////////////////////////////////////////////////////////////////////


			}break;

		case MC_PEER_SHOT:
			{
				MCommandParameter* pParam = pCommand->GetParameter(0);
				if(pParam->GetType()!=MPT_BLOB) break;	// 문제가 있다
				ZPACKEDSHOTINFO *pinfo =(ZPACKEDSHOTINFO*)pParam->GetPointer();

				// 칼질만 딜레이가 있다
				if(pinfo->sel_type!=MMCIP_MELEE) return false;

				if(pChar!=m_pMyCharacter &&
					( pChar->m_pVMesh->m_SelectWeaponMotionType==eq_wd_dagger ||
					pChar->m_pVMesh->m_SelectWeaponMotionType==eq_ws_dagger )) { // dagger
						pChar->SetAnimationUpper(ZC_STATE_UPPER_SHOT);
					}

				fDelayTime = .15f;

				////////////////////////////////////////////////////////////////////
				MMatchCharItemParts parts = (MMatchCharItemParts)pinfo->sel_type;
				if( parts != pChar->GetItems()->GetSelectedWeaponParts()) {
					// 지금 들고 있는 무기와 보내진 무기가 틀리다면 보내진 무기로 바꿔준다..
					OnChangeWeapon(uid,parts);
				}
				///////////////////////////////////////////////////////////////////////////////
			}
			break;

		// 새로 추가된 근접공격 커맨드
		case MC_PEER_SHOT_MELEE:
			{
				float fShotTime;
				rvector pos;
				int nShot;

				pCommand->GetParameter(&fShotTime, 0, MPT_FLOAT);
				pCommand->GetParameter(&pos, 1, MPT_POS);
				pCommand->GetParameter(&nShot, 2, MPT_INT);

				if(pChar!=m_pMyCharacter &&
					( pChar->m_pVMesh->m_SelectWeaponMotionType==eq_wd_dagger ||
					pChar->m_pVMesh->m_SelectWeaponMotionType==eq_ws_dagger )) { // dagger
						pChar->SetAnimationUpper(ZC_STATE_UPPER_SHOT);
					}

				fDelayTime = .1f;
				switch(nShot) {
					case 1 : fDelayTime = .10f;break;
					case 2 : fDelayTime = .15f;break;
					case 3 : fDelayTime = .2f;break;
					case 4 : fDelayTime = .25f;break;
				}
			}
			break;

// 퀘스트 커맨드들
		case MC_QUEST_PEER_NPC_ATTACK_MELEE :	// npc 칼질
			ZGetQuest()->OnPrePeerNPCAttackMelee(pCommand);
			fDelayTime = .4f;
			break;


		// 이것들 이외의 것들은 
		default:
			bFiltered = false;
			break;
	}

	
	if(bFiltered)
	{
		ZObserverCommandItem *pZCommand=new ZObserverCommandItem;
		pZCommand->pCommand=pCommand->Clone();
		pZCommand->fTime=GetTime()+fDelayTime;
		m_DelayedCommandList.push_back(pZCommand);
		return true;
	}

	return false;
}

void ZGame::PostSpMotion(ZC_SPMOTION_TYPE mtype)
{
	if(m_pMyCharacter==NULL) return;

	if( (m_pMyCharacter->GetStateLower() == ZC_STATE_LOWER_IDLE1) ||
		(m_pMyCharacter->GetStateLower() == ZC_STATE_LOWER_IDLE2) ||
		(m_pMyCharacter->GetStateLower() == ZC_STATE_LOWER_IDLE3) ||
		(m_pMyCharacter->GetStateLower() == ZC_STATE_LOWER_IDLE4) )
	{

		MMatchWeaponType type = MWT_NONE;

		ZItem* pSItem = m_pMyCharacter->GetItems()->GetSelectedWeapon();

		if( pSItem && pSItem->GetDesc() ) {
			type = pSItem->GetDesc()->m_nWeaponType;
		}

		if( mtype == ZC_SPMOTION_TAUNT ) // taunt 일 경우 모션이 없어서...
			if( (type == MWT_MED_KIT) || 
				(type == MWT_REPAIR_KIT) || 
				(type == MWT_FOOD) ||
				(type == MWT_BULLET_KIT)) 
			{
				return;
			}

		ZPostSpMotion(mtype);
	}
}

void ZGame::OnEventUpdateJjang(const MUID& uidChar, bool bJjang)
{
	ZCharacter* pCharacter = m_CharacterManager.Find(uidChar);
	if (pCharacter == NULL) return;

	if (bJjang) 
		ZGetEffectManager()->AddStarEffect(pCharacter);        
}


bool ZGame::IsAttackable(ZObject *pAttacker, ZObject *pTarget)
{
	if(GetMatch()->GetRoundState() != MMATCH_ROUNDSTATE_PLAY) return false;
	if(pAttacker==NULL) return true;

	if ( GetMatch()->IsTeamPlay() ) {
		if (pAttacker->GetTeamID() == pTarget->GetTeamID()) {
			if (!GetMatch()->GetTeamKillEnabled()) 
				return false;
		}
	}

#ifdef _QUEST
	if (ZGetGameTypeManager()->IsQuestDerived(ZGetGameClient()->GetMatchStageSetting()->GetGameType()))
	{
		if (pAttacker->GetTeamID() == pTarget->GetTeamID())
		{
			return false;
		}
	}

#endif
	return true;
}

void ZGame::ShowReplayInfo( bool bShow)
{
	MWidget* pWidget = ZGetGameInterface()->GetIDLResource()->FindWidget( "CombatChatOutput");
	if ( pWidget)
		pWidget->Show( bShow);

	m_bShowReplayInfo = bShow;
}

void ZGame::OnLocalOptainSpecialWorldItem(MCommand* pCommand)
{
	int nWorldItemID;
	pCommand->GetParameter(&nWorldItemID   , 0, MPT_INT);

	switch (nWorldItemID)
	{
	case WORLDITEM_PORTAL_ID:
		{
			if (ZGetGameClient()->GetMatchStageSetting()->GetGameType() != MMATCH_GAMETYPE_QUEST) break;

			// 서버에 포탈로 이동한다고 전송
			char nCurrSectorIndex = ZGetQuest()->GetGameInfo()->GetCurrSectorIndex();
			ZPostQuestRequestMovetoPortal(nCurrSectorIndex);
		}
		break;
	};
}


void ZGame::ReserveSuicide( void)
{
	m_dwReservedSuicideTime = GetGlobalTimeMS() + 10000;
	m_bSuicide = true;
}


bool ZGame::OnRuleCommand(MCommand* pCommand)
{
#ifdef _QUEST
	if (ZGetQuest()->OnGameCommand(pCommand)) return true;
#endif

	switch (pCommand->GetID())
	{
	case MC_MATCH_ASSIGN_COMMANDER:
	case MC_MATCH_ASSIGN_BERSERKER:
	case MC_MATCH_GAME_DEAD:
	case MC_MATCH_DUEL_QUEUEINFO:
		{
			if (m_Match.OnCommand(pCommand)) return true;
		};
	};

	return false;
}

void ZGame::OnResetTeamMembers(MCommand* pCommand)
{
	if (!m_Match.IsTeamPlay()) return;

	ZChatOutput( MCOLOR(ZCOLOR_GAME_INFO), ZMsg(MSG_GAME_MAKE_AUTO_BALANCED_TEAM) );

	MCommandParameter* pParam = pCommand->GetParameter(0);
	if(pParam->GetType()!=MPT_BLOB) return;
	void* pBlob = pParam->GetPointer();
	int nCount = MGetBlobArrayCount(pBlob);

	ZCharacterManager* pCharMgr = ZGetCharacterManager();

	for (int i = 0; i < nCount; i++)
	{
		MTD_ResetTeamMembersData* pDataNode = (MTD_ResetTeamMembersData*)MGetBlobArrayElement(pBlob, i);

		ZCharacter* pChar = pCharMgr->Find(pDataNode->m_uidPlayer);
		if (pChar == NULL) continue;

		if (pChar->GetTeamID() != ( (MMatchTeam)pDataNode->nTeam) )
		{
			// 만약 나자신이 팀변경이 되었으면 팀변경되었는지를 남긴다.
			if (pDataNode->m_uidPlayer == ZGetMyUID())
			{
				ZGetMyInfo()->GetGameInfo()->bForcedChangeTeam = true;
			}

			pChar->SetTeamID((MMatchTeam)pDataNode->nTeam);
		}

	}
}