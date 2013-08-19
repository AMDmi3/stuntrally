#include "pch.h"
#include "common/Defines.h"
#include "BaseApp.h"
#include "FollowCamera.h"
#include "../vdrift/pathmanager.h"
#include "SplitScreen.h"

#include "MyGUI_Prerequest.h"
#include "MyGUI_PointerManager.h"

#include <MyGUI.h>
#include <MyGUI_OgrePlatform.h>
using namespace Ogre;

#include "../sdl4ogre/sdlinputwrapper.hpp"


//  rendering
//-------------------------------------------------------------------------------------
bool BaseApp::frameRenderingQueued(const Ogre::FrameEvent& evt)
{
	if (mWindow->isClosed())
		return false;

	if (mShutDown)
		return false;

	//  update each device
	mInputWrapper->capture();

	mInputCtrl->update(evt.timeSinceLastFrame);
	for (int i=0; i<4; ++i)
		mInputCtrlPlayer[i]->update(evt.timeSinceLastFrame);

	   // key modifiers
	alt = mInputWrapper->isModifierHeld(SDL_Keymod(KMOD_ALT));
	ctrl = mInputWrapper->isModifierHeld(SDL_Keymod(KMOD_CTRL));
	shift = mInputWrapper->isModifierHeld(SDL_Keymod(KMOD_SHIFT));

	updateStats();
	
	// dt-
	Real time = evt.timeSinceLastFrame;
	if (time > 0.2f)  time = 0.2f;
	
	frameStart(time);
	//* */if (!frameStart())
	//	return false;
	
	return true;
}

bool BaseApp::frameEnded(const Ogre::FrameEvent& evt)
{
	// dt-
	Real time = evt.timeSinceLastFrame;
	if (time > 0.2f)  time = 0.2f;
	
	return frameEnd(time);
	//return true;
}


///  Fps stats
// ------------------------------------------------------------------------
void BaseApp::updateStats()
{
	try
	{	const RenderTarget::FrameStats& stats = mWindow->getStatistics();
		size_t mem = TextureManager::getSingleton().getMemoryUsage() + MeshManager::getSingleton().getMemoryUsage();

		int triCount = 0, batchCount = 0;
		if (AnyEffectEnabled())
		{
			CompositorInstance* c = NULL;
			CompositorChain* chain = CompositorManager::getSingleton().getCompositorChain (mSplitMgr->mViewports.front());

			// accumlate tris & batches from all compositors with all their render targets
			for (size_t i=0; i < chain->getNumCompositors(); ++i)
			if (chain->getCompositor(i)->getEnabled())
			{
				c = chain->getCompositor(i);
				RenderTarget* rt;
				for (size_t j = 0; j < c->getTechnique()->getNumTargetPasses(); ++j)
				{
					std::string textureName = c->getTechnique()->getTargetPass(j)->getOutputName();
					rt = c->getRenderTarget(textureName);
					triCount += rt->getTriangleCount();
					batchCount += rt->getBatchCount();
				}
			}
		}else
		{
			triCount = stats.triangleCount;
			batchCount = stats.batchCount;
		}

		//  update
		mOvrFps->setCaption(fToStr(stats.lastFPS,1,5) );
		mOvrTris->setCaption(iToStr(int(triCount/1000.f),4)+"k");
		mOvrBat->setCaption(iToStr(batchCount,3));
		mOvrMem->setCaption(iToStr(mem/1024/1024,3)+"M" );
	}
	catch(...) {  /*ignore*/  }
}


bool BaseApp::IsFocGui()
{
	return isFocGui || isFocRpl ||
		(mWndChampStage && mWndChampStage->getVisible()) ||
		(mWndChampEnd && mWndChampEnd->getVisible()) ||
		(mWndNetEnd && mWndNetEnd->getVisible()) ||
	(mWndTweak && mWndTweak->getVisible());
}

bool BaseApp::isTweak()
{
	return mWndTweak && mWndTweak->getVisible();
}
