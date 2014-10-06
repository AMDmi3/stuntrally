#include "pch.h"
#include "common/Def_Str.h"
#include "common/Gui_Def.h"
#include "common/GuiCom.h"
#include "../vdrift/pathmanager.h"
#include "../vdrift/game.h"
#include "CGame.h"
#include "CHud.h"
#include "CGui.h"
#include "FollowCamera.h"
#include <time.h>
#include <boost/filesystem.hpp>
using namespace std;
using namespace Ogre;
using namespace MyGUI;
namespace fs = boost::filesystem;


///  [Replay]  . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
//------------------------------------------------------------------------------------------------------------------

void CGui::slRplPosEv(SL)  // change play pos
{
	if (!app->bRplPlay)  return;
	double oldt = pGame->timer.GetReplayTime(0);
	double v = val;  v = std::max(0.0, std::min(1.0, v));  v *= app->replay.GetTimeLength();
	pGame->timer.SetReplayTime(0, v);

	FollowCamera* fCam = (*app->carModels.begin())->fCam;
	fCam->First();  // instant change
	//for (int i=0; i < 10; ++i)
	//	fCam->update(fabs(v-oldt)/10.f, 0);  //..?
}

void CGui::btnRplLoad(WP)  // Load
{
	//  from list
	int i = rplList->getIndexSelected();
	if (i == MyGUI::ITEM_NONE)  return;

	String name = rplList->getItemNameAt(i).substr(7);
	string file = GetRplListDir() + "/" + name + ".rpl";

	if (!app->replay.LoadFile(file))
	{
		Message::createMessageBox(
			"Message", TR("#{Replay} - #{RplLoad}"), TR("#{Error}."),
			MessageBoxStyle::IconWarning | MessageBoxStyle::Ok);
	}
	else  // car, track change
	{
		const ReplayHeader& h = app->replay.header;
		string car = h.car, trk = h.track;
		bool usr = h.track_user == 1;
		//todo: check if cars, trk exist..

		//trackreverse 	num_laps
		//collis_veget, collis_cars, collis_roadw, dyn_objects;
		//int boost_type, flip_type;  float boost_power;
		//float pre_time;

		//  set game config from replay
		pSet->game = pSet->gui;
		pSet->game.car[0] = car;  pSet->game.track = trk;  pSet->game.track_user = usr;
		pSet->game.car_hue[0] = h.hue[0];  pSet->game.car_sat[0] = h.sat[0];  pSet->game.car_val[0] = h.val[0];

		pSet->game.trees = h.ver < 6 ? 1.f : h.trees;  // older didnt have trees saved, so use default 1
		pSet->game.local_players = h.numPlayers;
		BackFromChs();
		LogO("RPL btn Load  players: "+toStr(h.numPlayers)+" netw: "+ toStr(h.networked));

		for (int p=1; p < h.numPlayers; ++p)
		{	pSet->game.car[p] = h.cars[p-1];
			pSet->game.car_hue[p] = h.hue[p];  pSet->game.car_sat[p] = h.sat[p];  pSet->game.car_val[p] = h.val[p];
		}
		app->newGameRpl = true;
		btnNewGame(0);
		app->bRplPlay = 1;
	}
}

void CGui::btnRplSave(WP)  // Save
{
	String edit = edRplName->getCaption();
	String file = PATHMANAGER::Replays() + "/" + pSet->game.track + "_" + edit + ".rpl";
	///  save
	if (PATHMANAGER::FileExists(file))
	{
		Message::createMessageBox(
			"Message", TR("#{Replay} - #{RplSave}"), TR("#{AlreadyExists}."),
			MessageBoxStyle::IconWarning | MessageBoxStyle::Ok);
		return;
	}
	if (!app->replay.SaveFile(file.c_str()))
	{
		Message::createMessageBox(
			"Message", TR("#{Replay} - #{RplSave}"), TR("#{Error}."),
			MessageBoxStyle::IconWarning | MessageBoxStyle::Ok);
	}
	updReplaysList();
}

//  list change
void CGui::listRplChng(List* li, size_t pos)
{
	size_t i = li->getIndexSelected();  if (i == ITEM_NONE)  return;
	String name = li->getItemNameAt(i).substr(7);
	string file = GetRplListDir() + "/" + name + ".rpl";
	if (!valRplName)  return;  valRplName->setCaption(name);
	if (!valRplInfo)  return;  edRplName->setCaption(name);
	
	//  load replay header, upd info text
	Replay rpl;  char stm[128];
	if (rpl.LoadFile(file,true))
	{
		String ss = String(TR("#{Track}: ")) + gcom->GetSceneryColor(rpl.header.track) +
			rpl.header.track + (rpl.header.track_user ? "  *"+TR("#{TweakUser}")+"*" : "");
		valRplName->setCaption(ss);

		ss = String(TR("#{Car}: ")) + rpl.header.car + "       "+
			(rpl.header.numPlayers == 1 ? "" : (TR("#{Players}: ") + toStr(rpl.header.numPlayers))) + "  " +
			(rpl.header.networked == 0 ? "" : "M") +  //TR("#{Multiplayer}")
			"\n#C0D8F0" + TR("#{RplTime}: ") + CHud::StrTime(rpl.GetTimeLength()) +
			"\n#90A0B0" + TR("#{Simulation}: ") + rpl.header.sim_mode;
		if (rpl.header.networked == 1)  // list nicks
			ss += String("\n#90C0E0")+rpl.header.nicks[0]+"  "+rpl.header.nicks[1]+"  "+rpl.header.nicks[2]+"  "+rpl.header.nicks[3];
		//else  // other cars, car colors ..
			//ss += String("\n#90C0E0")+rpl.header.cars[0]+"  "+rpl.header.cars[1]+"  "+rpl.header.cars[2];
		valRplInfo->setCaption(ss);

		//  file stats
		int size = fs::file_size(file);
		std::time_t ti = fs::last_write_time(file);
		if (!std::strftime(stm, 126, "%d.%b'%y  %a %H:%M", std::localtime(&ti)))  stm[0]=0;
		
		ss =/*"Time: "+*/String(stm)+"\n#A0A0A0"+
			String(TR("#{RplFileSize}: ")) + fToStr( float(size)/1000000.f, 2,5) + TR(" #{UnitMB}") + "\n#808080" +
			TR("#{RplVersion}: ") + toStr(rpl.header.ver) + "     " + toStr(rpl.header.frameSize) + "B";
		if (valRplInfo2)  valRplInfo2->setCaption(ss);
	}
	//edRplDesc
}


//  replay settings

void CGui::slRplNumViewports(SL)
{
	int v = 1.f + 3.f * val;	if (bGI)  pSet->rpl_numViews = v;
	if (valRplNumViewports)  valRplNumViewports->setCaption(toStr(v));
}


//  replays list filtering

void CGui::btnRplAll(WP)
{
	rbRplCur->setStateSelected(false);  rbRplAll->setStateSelected(true);
	pSet->rpl_listview = 0;  updReplaysList();
}

void CGui::btnRplCur(WP)
{
	rbRplCur->setStateSelected(true);  rbRplAll->setStateSelected(false);
	pSet->rpl_listview = 1;  updReplaysList();
}

void CGui::chkRplGhosts(Ck*)
{
	updReplaysList();
}


//  replay controls

void CGui::btnRplToStart(WP)
{
	pGame->timer.RestartReplay(0);
}

void CGui::btnRplToEnd(WP)
{
}

void CGui::btnRplBackDn(WP, int, int, MouseButton){	bRplBack = true;  }
void CGui::btnRplBackUp(WP, int, int, MouseButton){	bRplBack = false;  }
void CGui::btnRplFwdDn(WP, int, int, MouseButton){	bRplFwd = true;  }
void CGui::btnRplFwdUp(WP, int, int, MouseButton){	bRplFwd = false;  }

void CGui::btnRplPlay(WP)  // play / pause
{
	app->bRplPause = !app->bRplPause;
	UpdRplPlayBtn();
}

void CGui::UpdRplPlayBtn()
{
	String sign = app->bRplPause ? "|>" : "||";
	if (btRplPl)
		btRplPl->setCaption(sign);
}


void CGui::updReplaysList()
{
	if (!rplList)  return;
	//Ogre::Timer ti;

	rplList->removeAllItems();

	strlist li;
	PATHMANAGER::DirList(GetRplListDir(), li, "rpl");
	
	for (strlist::iterator i = li.begin(); i != li.end(); ++i)
	if (StringUtil::endsWith(*i, ".rpl"))
	{
		String s = *i;  s = StringUtil::replaceAll(s,".rpl","");
		String slow = s;  StringUtil::toLowerCase(slow);
		if (sRplFind == "" || strstr(slow.c_str(), sRplFind.c_str()) != 0)
		if (pSet->rpl_listview != 1 || StringUtil::startsWith(s,pSet->game.track, false))
		{
			size_type f = s.find_first_of("_-0123456789");
			string ss;
			if (f != string::npos)  ss = s.substr(0,f);
			else  ss = s.substr(0,1);  //gcom->scnClr[gcom->scnN[]];
			rplList->addItem(gcom->GetSceneryColor(ss) + s);
		}
	}
	//LogO(String("::: Time ReplaysList: ") + fToStr(ti.getMilliseconds(),0,3) + " ms");
}


//  Delete
void CGui::btnRplDelete(WP)
{
	size_t i = rplList->getIndexSelected();  if (i == ITEM_NONE)  return;
	string name = rplList->getItemNameAt(i).substr(7);

	Message* message = Message::createMessageBox(
		"Message", TR("#{Replay} - #{RplDelete} ?"), name,
		MessageBoxStyle::IconQuest | MessageBoxStyle::Yes | MessageBoxStyle::No);
	message->eventMessageBoxResult += newDelegate(this, &CGui::msgRplDelete);
}
void CGui::msgRplDelete(Message* sender, MessageBoxStyle result)
{
	if (result != MessageBoxStyle::Yes)  return;
	size_t i = rplList->getIndexSelected();  if (i == ITEM_NONE)  return;
	string name = rplList->getItemNameAt(i).substr(7);
	string file = GetRplListDir() +"/"+ name + ".rpl";

	if (fs::exists(file))
		fs::remove(file);
	updReplaysList();
}

//  Rename
void CGui::btnRplRename(WP)
{
	if (pSet->rpl_listghosts)  return;  // cant rename ghosts
	size_t i = rplList->getIndexSelected();  if (i == ITEM_NONE)  return;
	string name = rplList->getItemNameAt(i).substr(7);
	string edit = edRplName->getCaption();

	if (name == edit)  // same name
	{	Message::createMessageBox(
			"Message", TR("#{Replay} - #{RplRename}"), TR("#{AlreadyExists}."),
			MessageBoxStyle::IconInfo | MessageBoxStyle::Ok);
		return;  }
	
	string file = PATHMANAGER::Replays() + "/" + name + ".rpl";
	string fileNew = PATHMANAGER::Replays() + "/" + edit + ".rpl";

	if (fs::exists(fileNew))
	{	Message::createMessageBox(
			"Message", TR("#{Replay} - #{RplRename}"), TR("#{AlreadyExists}."),
			MessageBoxStyle::IconInfo | MessageBoxStyle::Ok);
		return;  }

	if (fs::exists(file))
		fs::rename(file, fileNew);
	updReplaysList();
}
