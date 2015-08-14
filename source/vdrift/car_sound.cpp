#include "pch.h"
#include "par.h"
#include "car.h"
#include "cardefs.h"
#include "configfile.h"
#include "collision_world.h"
#include "tracksurface.h"
#include "configfile.h"
#include "settings.h"
#include "../ogre/CGame.h"  //+ replay
#include "../ogre/CarModel.h"  //+ camera pos
#include "../ogre/FollowCamera.h"  //+ camera pos
#include "../ogre/common/Def_Str.h"
#include "../ogre/common/data/SceneXml.h"
#include "../ogre/common/CScene.h"
#include "../ogre/common/GraphView.h"
#include "../network/protocol.hpp"
#include "tobullet.h"
#include "game.h"  //sound
#include "../ogre/SplitScreen.h"  // num plr
#include <OgreCamera.h>
using namespace std;


//--------------------------------------------------------------------------------------------------------------------------
bool CAR::LoadSounds(
	const string & carpath,
	const SOUNDINFO & sound_device_info,
	const SOUND_LIB & sndLib)
{
	{
		if (!soundbuffers["engine.wav"].Load(carpath+"engine.wav", sound_device_info))
		{
			LogO("SOUND: Error: Unable to load engine sound: "+carpath+"engine.wav");
			return false;
		}
		enginesounds.push_back(pair <ENGINESOUNDINFO, SOUNDSOURCE> ());
		SOUNDSOURCE & enginesound = enginesounds.back().second;
		enginesound.Setup(soundbuffers["engine.wav"], true, true, 0.f);
		enginesound.Start();
	}

	int i;
	for (i = 0; i < numWheels; ++i)  // tires
	{
		if (!tiresqueal[i]	.Setup(sndLib, "tire_squeal",	true, true, 0.f))  return false;	tiresqueal[i].Seek4(i);
		if (!gravelsound[i]	.Setup(sndLib, "gravel",		true, true, 0.f))  return false;	gravelsound[i].Seek4(i);
		if (!grasssound[i]	.Setup(sndLib, "grass",			true, true, 0.f))  return false;	grasssound[i].Seek4(i);

		if (!tirebump[i].Setup(sndLib, i >= 2 ? "bump_rear" : "bump_front", true, false,1.f))  return false;
	}

	for (i = 1; i <= Ncrashsounds; ++i)  // crashes
	{	string s = "crash/";  s += toStr(i/10)+toStr(i%10);
		if (!crashsound[i-1].Setup(sndLib, s, true, false,1.f))  return false;
	}
	if (!crashscrap  .Setup(sndLib, "crash/scrap",	true, true, 0.f))  return false;  crashscrap.Start();
	if (!crashscreech.Setup(sndLib, "crash/screech",	true, true, 0.f))  return false;  crashscreech.Start();

	if (!roadnoise	.Setup(sndLib, "wind",		 true, true, 0.f))  return false;  roadnoise.Start();
	if (!boostsnd	.Setup(sndLib, "boost",		 true, true, 0.f))  return false;  boostsnd.Start();

	for (i = 0; i < Nwatersounds; ++i)  // fluids
		if (!watersnd[i].Setup(sndLib, "water"+toStr(i+1), true, false,0.f))  return false;

	if (!mudsnd		.Setup(sndLib, "mud1",		 true, false,0.f))  return false;
	if (!mud_cont	.Setup(sndLib, "mud_cont",	 true, true, 0.f))  return false;  mud_cont.Start();
	if (!water_cont	.Setup(sndLib, "water_cont", true, true, 0.f))  return false;  water_cont.Start();
	
	return true;
}


//--------------------------------------------------------------------------------------------------------------------------
void CAR::GetSoundList(list <SOUNDSOURCE *> & li)
{
	for (list <pair <ENGINESOUNDINFO, SOUNDSOURCE> >::iterator
		i = enginesounds.begin(); i != enginesounds.end(); ++i)
		li.push_back(&i->second);

	int i;
	for (i = 0; i < numWheels; ++i)  li.push_back(&tiresqueal[i]);
	for (i = 0; i < numWheels; ++i)  li.push_back(&grasssound[i]);
	for (i = 0; i < numWheels; ++i)  li.push_back(&gravelsound[i]);
	for (i = 0; i < numWheels; ++i)  li.push_back(&tirebump[i]);

	for (i = 0; i < Ncrashsounds; ++i)
		li.push_back(&crashsound[i]);
	li.push_back(&crashscrap);
	li.push_back(&crashscreech);

	li.push_back(&roadnoise);
	li.push_back(&boostsnd);

	for (i = 0; i < Nwatersounds; ++i)
		li.push_back(&watersnd[i]);
	li.push_back(&mudsnd);
	
	li.push_back(&mud_cont);
	li.push_back(&water_cont);
}

void CAR::GetEngineSoundList(list <SOUNDSOURCE *> & outputlist)
{
	for (list <pair <ENGINESOUNDINFO, SOUNDSOURCE> >::iterator i =
		enginesounds.begin(); i != enginesounds.end(); ++i)
	{
		outputlist.push_back(&i->second);
	}
}


//--------------------------------------------------------------------------------------------------------------------------
void CAR::UpdateSounds(float dt)
{
	float rpm, throttle, speed, dynVel;  bool hitp = false;
	MATHVECTOR<float,3> pos, engPos, whPos[MAX_WHEELS], hitPos;  // car, engine, wheels pos
	QUATERNION<float> rot;
	TRACKSURFACE::TYPE surfType[MAX_WHEELS];
	float squeal[MAX_WHEELS],whVel[MAX_WHEELS], suspVel[MAX_WHEELS],suspDisp[MAX_WHEELS];
	float whH_all = 0.f;  bool mud = false;
	float fHitForce = 0.f, boostVal = 0.f, fCarScrap = 0.f, fCarScreech = 0.f;

	bool dmg = pSet->game.damage_type > 0, reduced = pSet->game.damage_type==1;
	bool terminal = dynamics.fDamage >= 100.f;
	float fDmg = pApp->scn->sc->damageMul;
	
	///  replay play  ------------------------------------------
	if (pApp->bRplPlay)
	{	dmg = false;

		#ifdef DEBUG
		assert(id < pApp->frm.size());
		#endif
		const ReplayFrame2& fr = pApp->frm[id];
		pos = fr.pos;  rot = fr.rot;   rpm = fr.rpm;
		throttle = fr.throttle /255.f;  boostVal = fr.fboost /255.f;
		dynamics.fDamage = fr.damage /255.f*100.f;  //dmg read

		MATHVECTOR<float,3> offset = dynamics.engine.GetPosition();
		rot.RotateVector(offset);
		engPos = offset + pos;

		speed = fr.speed;  dynVel = fr.dynVel;
		whMudSpin = fr.get(b_fluid) ? fr.whMudSpin : 0.f;

		if (fr.get(b_scrap))
		{
			const RScrap& sc = fr.scrap[0];
			fCarScrap = sc.fScrap;  fCarScreech = sc.fScreech;
		}

		hitp = fr.get(b_hit);
		if (hitp)
		{
			const RHit& h = fr.hit[0];
			fHitForce = h.fHitForce;
			hitPos[0] = h.vHitPos.x;  hitPos[1] = -h.vHitPos.z;  hitPos[2] = h.vHitPos.y;
		}

		int w, ww = fr.wheels.size();
		for (w=0; w < ww; ++w)
		{
			const RWheel& wh = fr.wheels[w];
			whPos[w] = wh.pos;
			surfType[w] = (TRACKSURFACE::TYPE)wh.surfType;
			squeal[w] = wh.squeal;  whVel[w] = wh.whVel;
			suspVel[w] = wh.suspVel;  suspDisp[w] = wh.suspDisp;
			//  fluids
			if (wh.whP >= 0)  // solid no snd
				whH_all += wh.whH;
			if (wh.whP >= 1)  mud = true;
		}
	}
	else  /// game  ------------------------------------------
	{
		pos = dynamics.GetPosition();  rot = dynamics.GetOrientation();
		rpm = GetEngineRPM();
		throttle = dynamics.GetThrottle();
		engPos = dynamics.GetEnginePosition();
		speed = GetSpeed();
		dynVel = dynamics.GetVelocity().Magnitude();
		fHitForce = dynamics.fHitForce;  hitp = true;
		hitPos[0] = dynamics.vHitPos.x;  hitPos[1] = -dynamics.vHitPos.z;  hitPos[2] = dynamics.vHitPos.y;
		boostVal = dynamics.boostVal;
		
		for (int w=0; w < numWheels; ++w)
		{
			WHEEL_POSITION wp = WHEEL_POSITION(w);
			whPos[w] = dynamics.GetWheelPosition(wp);

			const TRACKSURFACE* surface = dynamics.GetWheelContact(wp).GetSurfacePtr();
			surfType[w] = !surface ? TRACKSURFACE::NONE : surface->type;
			//  squeal
			squeal[w] = GetTireSquealAmount(wp);
			whVel[w] = dynamics.GetWheelVelocity(wp).Magnitude();
			//  susp
			suspVel[w] = dynamics.GetSuspension(wp).GetVelocity();
			suspDisp[w] = dynamics.GetSuspension(wp).GetDisplacementPercent();
			//  fluids
			if (dynamics.whP[w] >= 0)  // solid no snd
				whH_all += dynamics.whH[w];
			if (dynamics.whP[w] >= 1)  mud = true;
		}

		//  wheels in mud, spinning intensity
		float mudSpin = 0.f;
		for (int w=0; w < numWheels; ++w)
		{
			float vel = std::abs(dynamics.wheel[w].GetAngularVelocity());
			if (vel <= 30.f)  continue;
			if (dynamics.whP[w] == 2)
				mudSpin += dynamics.whH[w] * std::min(80.f, 1.5f * vel) / 80.f;
			else if (dynamics.whP[w] == 1)
				mudSpin += dynamics.whH[w] * std::min(160.f, 3.f * vel) / 80.f;
		}
		whMudSpin = mudSpin * 0.5f;

		//  car scrap, screech
		float gain = std::min(1.f, dynamics.fCarScrap);
		if (dynamics.fCarScrap > 0.f)
		{	dynamics.fCarScrap -= (-gain * 0.8f + 1.2f)* dt;
			if (dynamics.fCarScrap < 0.f)  dynamics.fCarScrap = 0.f;
		}
		fCarScrap = gain;

		/// <><> Damage <><>
		if (dmg && !terminal)
			if (reduced)
				dynamics.fDamage += fDmg * fCarScrap * dt * dynamics.fHitDmgA * gPar.dmgFromScrap;
			else  // normal
				dynamics.fDamage += fDmg * fCarScrap * dt * dynamics.fHitDmgA * gPar.dmgFromScrap2;

		gain = std::min(1.f, dynamics.fCarScreech);
		if (dynamics.fCarScreech > 0.f)
		{	dynamics.fCarScreech -= 3.f * dt;
			if (dynamics.fCarScreech < 0.f)  dynamics.fCarScreech = 0.f;
		}
		fCarScreech = gain;
	}
	
	///  listener  ------------------------------------------
	if (!bRemoteCar)
	{
		MATHVECTOR<float,3> campos = pos;
		QUATERNION<float> camrot;// = rot;
		using namespace Ogre;
		if (pCarM && pCarM->fCam)
		{	//  pos
			Vector3 cp = pCarM->fCam->camPosFinal;
			campos[0] = cp.x;  campos[1] =-cp.z;  campos[2] = cp.y;
			//  rot
			Quaternion rr = pCarM->fCam->mCamera->getOrientation() * Object::qrFix2;
			Quaternion b(-rr.w,  -rr.x, -rr.y, -rr.z);
			//  fix pan
			b = b * Quaternion(Degree(90.f), Vector3::UNIT_X) * Quaternion(Degree(-90.f), Vector3::UNIT_Z);
			camrot[0] = b.x;  camrot[1] = b.y;  camrot[2] = b.z;  camrot[3] = b.w;
		}
		if (pApp->pGame->sound.Enabled())
			pApp->pGame->sound.SetListener(campos, camrot, MATHVECTOR<float,3>(),
				pApp->mSplitMgr->mNumViewports == 1);  // 3D for 1 player
	}

	//  engine
	float total_gain = 0.0, loudest = 0.0;
	list <pair <SOUNDSOURCE *, float> > gainlist;

	for (list <pair <ENGINESOUNDINFO, SOUNDSOURCE> >::iterator i = enginesounds.begin(); i != enginesounds.end(); ++i)
	{
		ENGINESOUNDINFO & info = i->first;
		SOUNDSOURCE & sound = i->second;

		float gain = 1.0;

		if (rpm < info.minrpm)	gain = 0;
		else if (rpm < info.fullgainrpmstart && info.fullgainrpmstart > info.minrpm)
			gain *= (rpm - info.minrpm)/(info.fullgainrpmstart-info.minrpm);

		if (rpm > info.maxrpm)	gain = 0;
		else if (rpm > info.fullgainrpmend && info.fullgainrpmend < info.maxrpm)
			gain *= 1.0-(rpm - info.fullgainrpmend)/(info.maxrpm-info.fullgainrpmend);

		if (info.power == ENGINESOUNDINFO::BOTH)
			gain *= throttle * 0.5 + 0.5;
		else if (info.power == ENGINESOUNDINFO::POWERON)
			gain *= throttle;
		else if (info.power == ENGINESOUNDINFO::POWEROFF)
			gain *= (1.0-throttle);

		total_gain += gain;
		if (gain > loudest)  loudest = gain;
		gainlist.push_back(pair <SOUNDSOURCE*, float> (&sound, gain));

		if (dynamics.vtype == V_Spaceship)
		{
			sound.SetPitch(1.0);
			gain = total_gain = throttle;
		}else
		{	// car
			float pitch = rpm / info.naturalrpm;
			sound.SetPitch(pitch);
		}
		sound.SetPosition(engPos);
	}

	//normalize gains engine
	//assert(total_gain >= 0.0);
	for (list <pair <SOUNDSOURCE *, float> >::iterator i = gainlist.begin(); i != gainlist.end(); ++i)
	{
		if (total_gain == 0.0)
			i->first->SetGain(0.0);
		else if (enginesounds.size() == 1 && enginesounds.back().first.power == ENGINESOUNDINFO::BOTH)
			i->first->SetGain(i->second            * dynamics.engine_vol_mul * pSet->vol_engine);
		else
			i->first->SetGain(i->second/total_gain * dynamics.engine_vol_mul * pSet->vol_engine);

		//if (i->second == loudest) cout << i->first->GetSoundBuffer().GetName() << ": " << i->second << endl;
	}


	///  tire squeal
	for (int i = 0; i < numWheels; i++)
	{
		// make sure we don't get overlap
		gravelsound[i].SetGain(0.0);
		grasssound[i].SetGain(0.0);
		tiresqueal[i].SetGain(0.0);

		float maxgain = 0.6, pitchvar = 0.4, pmul = 1.f;

		vector<SOUNDSOURCE>* snd = &gravelsound;
		switch (surfType[i])
		{
		case TRACKSURFACE::ASPHALT:		snd = &tiresqueal;	maxgain = 0.4;  pitchvar = 0.40;  pmul = 0.8f;  break;
		case TRACKSURFACE::GRASS:		snd = &grasssound;	maxgain = 0.7;	pitchvar = 0.25;  break;
		case TRACKSURFACE::GRAVEL:		snd = &gravelsound;	maxgain = 0.7;	break;
		case TRACKSURFACE::CONCRETE:	snd = &tiresqueal;	maxgain = 0.5;	pitchvar = 0.25;  pmul = 0.7f;  break;
		case TRACKSURFACE::SAND:		snd = &grasssound;	maxgain = 0.5;  pitchvar = 0.25;  break;
		case TRACKSURFACE::NONE:
						default:		snd = &tiresqueal;	maxgain = 0.0;	break;
		}	/// todo: more,sounds.. sand,snow,grass-new,mud..
		// TODO: sum slip, spin, stop tire sounds

		float pitch = std::min(1.f, std::max(0.f, (whVel[i]-5.0f)*0.1f ));
		pitch = 1.0 - pitch;
		pitch *= pitchvar;
		pitch = pitch + (1.f - pitchvar);
		pitch = std::min(4.f, std::max(0.1f, pitch ));

		(*snd)[i].SetPosition(whPos[i]);
		(*snd)[i].SetGain(squeal[i]*maxgain * pSet->vol_tires);
		(*snd)[i].SetPitch(pitch * pmul);
	}

	//  wind
	{
		float gain = dynVel;
		if (dynamics.vtype == V_Spaceship)   gain *= 0.7f;
		//if (dynamics.sphere)  gain *= 0.9f;
		if (gain < 0.f)	gain = -gain;
		gain *= 0.02f;	gain *= gain;
		if (gain > 1.f)	gain = 1.f;
		roadnoise.SetGain(gain * pSet->vol_env);
		roadnoise.SetPosition(engPos); //
		//cout << gain << endl;
	}

	//  susp bump
	if (dynamics.vtype == V_Car)
	for (int i = 0; i < numWheels; i++)
	{
		suspbump[i].Update(suspVel[i], suspDisp[i], dt);
		if (suspbump[i].JustSettled())
		{
			float bumpsize = suspbump[i].GetTotalBumpSize();

			const float breakevenms = 5.0;
			float gain = bumpsize * speed / breakevenms;
			if (gain > 1)	gain = 1;
			if (gain < 0)	gain = 0;

			if (gain > 0 && !tirebump[i].Audible())
			{
				tirebump[i].SetGain(gain * pSet->vol_susp);
				tirebump[i].SetPosition(whPos[i]);
				tirebump[i].Play();
			}
		}
	}
	
	//  fluids - hit
	bool fluidHit = whH_all > 1.f;
	//LogO(toStr(whH_all) + "  v "+ toStr(dynVel));

	if (fluidHit && !fluidHitOld)
	//if (dynVel > 10.f && whH_all > 1.f && )
	{
		int i = std::min(Nwatersounds-1, (int)(dynVel / 15.f));
		float gain = std::min(3.0f, 0.3f + dynVel / 30.f);
		SOUNDSOURCE& snd = /*mud ? mudsnd : */watersnd[i];
		
		//LogO("fluid hit i"+toStr(i)+" g"+toStr(gain)+" "+(mud?"mud":"wtr"));
		if (!snd.Audible())
		{
			snd.SetGain(gain * pSet->vol_fl_splash * (mud ? 0.6f : 1.f));
			snd.SetPosition(engPos);
			snd.Play();
		}

		if (mud)  {  SOUNDSOURCE& snd = mudsnd;
		if (!snd.Audible())
		{
			snd.SetGain(gain * pSet->vol_fl_splash);
			snd.SetPosition(engPos);
			snd.Play();
		}	}
	}
	fluidHitOld = fluidHit;

	//  fluids - continuous
	float velM = mud && whH_all > 0.1f ?
		whMudSpin * 2.5f : 0.f;
	mud_cont.SetGain(std::min(1.f, velM) * pSet->vol_fl_cont * 0.85f);
	mud_cont.SetPitch(std::max(0.7f, std::min(3.f, velM * 0.35f)));
	mud_cont.SetPosition(engPos);

	float velW = !mud && whH_all > 0.1f && whH_all < 3.9f ?
		dynVel / 30.f : 0.f;
	water_cont.SetGain(std::min(1.f, velW * 1.5f) * pSet->vol_fl_cont);
	water_cont.SetPitch(std::max(0.7f, std::min(1.3f, velW)));
	water_cont.SetPosition(engPos);

	
	//  boost
	boostsnd.SetGain(boostVal * 0.55f * pSet->vol_engine);
	boostsnd.SetPosition(engPos); //back?-
	
	//  crash
	{
		crashdetection2.Update(-fHitForce, dt);
		crashdetection2.deceltrigger = 1.f;
		float crashdecel2 = crashdetection2.GetMaxDecel();
		dynamics.fHitForce3 = crashdecel2 / 30.f;

		if (crashdecel2 > 0)
		{
			float gain = 0.9f;
			
			int f = crashdecel2 / 30.f * Ncrashsounds;
			int i = std::max(1, std::min(Ncrashsounds-1, f));
			//LogO("crash: "+toStr(i));

			if (/*gain > mingain &&*/ crashsoundtime[i] > /*ti*/0.4f)  //!crashsound.Audible())
			{
				crashsound[i].SetGain(gain * pSet->vol_car_crash);
				if (hitp)
				crashsound[i].SetPosition(hitPos);
				crashsound[i].Play();
				crashsoundtime[i] = 0.f;

				/// <><> Damage <><> 
				if (dmg && !terminal)
					if (reduced)
						dynamics.fDamage += fDmg * crashdecel2 * dynamics.fHitDmgA * gPar.dmgFromHit;
					else  // normal
					{	float f = std::min(1.f, crashdecel2 / 30.f);
						f = powf(f, gPar.dmgPow2);
						dynamics.fDamage += fDmg * crashdecel2 * dynamics.fHitDmgA * gPar.dmgFromHit2 * f;
					}
			}
			//LogO("Car Snd: " + toStr(crashdecel));// + " force " + toStr(hit.force)
		}
	}
	//  time played
	for (int i=0; i < Ncrashsounds; ++i)
		if (crashsoundtime[i] < 5.f)
			crashsoundtime[i] += dt;
	

	//  crash scrap and screech
	{
		crashscrap.SetGain(fCarScrap * pSet->vol_car_scrap);
		if (hitp)
		crashscrap.SetPosition(hitPos);

		crashscreech.SetGain(fCarScreech * pSet->vol_car_scrap * 0.6f);
		if (hitp)
		crashscreech.SetPosition(hitPos);
	}

	/// <><> Damage <><> 
	if (dmg)
		if (dynamics.fDamage > 100.f)  dynamics.fDamage = 100.f;
}
