/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * nsWinGesture - Touch input handling for tablet displays.
 */

#include "nscore.h"
#include "nsWinGesture.h"
#include "nsUXThemeData.h"
#include "nsIDOMSimpleGestureEvent.h"
#include "nsGUIEvent.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef PR_LOGGING
extern PRLogModuleInfo* gWindowsLog;
#endif

const PRUnichar nsWinGesture::kGestureLibraryName[] =  L"user32.dll";
HMODULE nsWinGesture::sLibraryHandle = nsnull;
nsWinGesture::GetGestureInfoPtr nsWinGesture::getGestureInfo = nsnull;
nsWinGesture::CloseGestureInfoHandlePtr nsWinGesture::closeGestureInfoHandle = nsnull;
nsWinGesture::GetGestureExtraArgsPtr nsWinGesture::getGestureExtraArgs = nsnull;
nsWinGesture::SetGestureConfigPtr nsWinGesture::setGestureConfig = nsnull;
nsWinGesture::GetGestureConfigPtr nsWinGesture::getGestureConfig = nsnull;
nsWinGesture::BeginPanningFeedbackPtr nsWinGesture::beginPanningFeedback = nsnull;
nsWinGesture::EndPanningFeedbackPtr nsWinGesture::endPanningFeedback = nsnull;
nsWinGesture::UpdatePanningFeedbackPtr nsWinGesture::updatePanningFeedback = nsnull;

nsWinGesture::RegisterTouchWindowPtr nsWinGesture::registerTouchWindow = nsnull;
nsWinGesture::UnregisterTouchWindowPtr nsWinGesture::unregisterTouchWindow = nsnull;
nsWinGesture::GetTouchInputInfoPtr nsWinGesture::getTouchInputInfo = nsnull;
nsWinGesture::CloseTouchInputHandlePtr nsWinGesture::closeTouchInputHandle = nsnull;

static bool gEnableSingleFingerPanEvents = false;

nsWinGesture::nsWinGesture() :
  mPanActive(false),
  mFeedbackActive(false),
  mXAxisFeedback(false),
  mYAxisFeedback(false),
  mPanInertiaActive(false)
{
  (void)InitLibrary();
  mPixelScrollOverflow = 0;
}

/* Load and shutdown */

bool nsWinGesture::InitLibrary()
{
  if (getGestureInfo) {
    return true;
  } else if (sLibraryHandle) {
    return false;
  }

  sLibraryHandle = ::LoadLibraryW(kGestureLibraryName);
  HMODULE hTheme = nsUXThemeData::GetThemeDLL();

  // gesture interfaces
  if (sLibraryHandle) {
    getGestureInfo = (GetGestureInfoPtr)GetProcAddress(sLibraryHandle, "GetGestureInfo");
    closeGestureInfoHandle = (CloseGestureInfoHandlePtr)GetProcAddress(sLibraryHandle, "CloseGestureInfoHandle");
    getGestureExtraArgs = (GetGestureExtraArgsPtr)GetProcAddress(sLibraryHandle, "GetGestureExtraArgs");
    setGestureConfig = (SetGestureConfigPtr)GetProcAddress(sLibraryHandle, "SetGestureConfig");
    getGestureConfig = (GetGestureConfigPtr)GetProcAddress(sLibraryHandle, "GetGestureConfig");
    registerTouchWindow = (RegisterTouchWindowPtr)GetProcAddress(sLibraryHandle, "RegisterTouchWindow");
    unregisterTouchWindow = (UnregisterTouchWindowPtr)GetProcAddress(sLibraryHandle, "UnregisterTouchWindow");
    getTouchInputInfo = (GetTouchInputInfoPtr)GetProcAddress(sLibraryHandle, "GetTouchInputInfo");
    closeTouchInputHandle = (CloseTouchInputHandlePtr)GetProcAddress(sLibraryHandle, "CloseTouchInputHandle");
  }

  if (!getGestureInfo || !closeGestureInfoHandle || !getGestureExtraArgs ||
    !setGestureConfig || !getGestureConfig) {
    getGestureInfo         = nsnull;
    closeGestureInfoHandle = nsnull;
    getGestureExtraArgs    = nsnull;
    setGestureConfig       = nsnull;
    getGestureConfig       = nsnull;
    return false;
  }
  
  if (!registerTouchWindow || !unregisterTouchWindow || !getTouchInputInfo || !closeTouchInputHandle) {
    registerTouchWindow   = nsnull;
    unregisterTouchWindow = nsnull;
    getTouchInputInfo     = nsnull;
    closeTouchInputHandle = nsnull;
  }

  // panning feedback interfaces
  if (hTheme) {
    beginPanningFeedback = (BeginPanningFeedbackPtr)GetProcAddress(hTheme, "BeginPanningFeedback");
    endPanningFeedback = (EndPanningFeedbackPtr)GetProcAddress(hTheme, "EndPanningFeedback");
    updatePanningFeedback = (UpdatePanningFeedbackPtr)GetProcAddress(hTheme, "UpdatePanningFeedback");
  }

  if (!beginPanningFeedback || !endPanningFeedback || !updatePanningFeedback) {
    beginPanningFeedback   = nsnull;
    endPanningFeedback     = nsnull;
    updatePanningFeedback  = nsnull;
  }

  // Check to see if we want single finger gesture input. Only do this once
  // for the app so we don't have to look it up on every window create.
  gEnableSingleFingerPanEvents =
    Preferences::GetBool("gestures.enable_single_finger_input", false);

  return true;
}

#define GCOUNT 5

bool nsWinGesture::SetWinGestureSupport(HWND hWnd, nsGestureNotifyEvent::ePanDirection aDirection)
{
  if (!getGestureInfo)
    return false;

  GESTURECONFIG config[GCOUNT];

  memset(&config, 0, sizeof(config));

  config[0].dwID = GID_ZOOM;
  config[0].dwWant = GC_ZOOM;
  config[0].dwBlock = 0;

  config[1].dwID = GID_ROTATE;
  config[1].dwWant = GC_ROTATE;
  config[1].dwBlock = 0;

  config[2].dwID = GID_PAN;
  config[2].dwWant  = GC_PAN|GC_PAN_WITH_INERTIA|
                      GC_PAN_WITH_GUTTER;
  config[2].dwBlock = GC_PAN_WITH_SINGLE_FINGER_VERTICALLY|
                      GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY;

  if (gEnableSingleFingerPanEvents) {

    if (aDirection == nsGestureNotifyEvent::ePanVertical ||
        aDirection == nsGestureNotifyEvent::ePanBoth)
    {
      config[2].dwWant  |= GC_PAN_WITH_SINGLE_FINGER_VERTICALLY;
      config[2].dwBlock -= GC_PAN_WITH_SINGLE_FINGER_VERTICALLY;
    }

    if (aDirection == nsGestureNotifyEvent::ePanHorizontal ||
        aDirection == nsGestureNotifyEvent::ePanBoth)
    {
      config[2].dwWant  |= GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY;
      config[2].dwBlock -= GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY;
    }

  }

  config[3].dwWant = GC_TWOFINGERTAP;
  config[3].dwID = GID_TWOFINGERTAP;
  config[3].dwBlock = 0;

  config[4].dwWant = GC_PRESSANDTAP;
  config[4].dwID = GID_PRESSANDTAP;
  config[4].dwBlock = 0;

  return SetGestureConfig(hWnd, GCOUNT, (PGESTURECONFIG)&config);
}

/* Helpers */

bool nsWinGesture::IsAvailable()
{
  return getGestureInfo != nsnull;
}

bool nsWinGesture::RegisterTouchWindow(HWND hWnd)
{
  if (!registerTouchWindow)
    return false;

  return registerTouchWindow(hWnd, TWF_WANTPALM);
}

bool nsWinGesture::UnregisterTouchWindow(HWND hWnd)
{
  if (!unregisterTouchWindow)
    return false;

  return unregisterTouchWindow(hWnd);
}

bool nsWinGesture::GetTouchInputInfo(HTOUCHINPUT hTouchInput, PRUint32 cInputs, PTOUCHINPUT pInputs)
{
  if (!getTouchInputInfo)
    return false;

  return getTouchInputInfo(hTouchInput, cInputs, pInputs, sizeof(TOUCHINPUT));
}

bool nsWinGesture::CloseTouchInputHandle(HTOUCHINPUT hTouchInput)
{
  if (!closeTouchInputHandle)
    return false;

  return closeTouchInputHandle(hTouchInput);
}

bool nsWinGesture::GetGestureInfo(HGESTUREINFO hGestureInfo, PGESTUREINFO pGestureInfo)
{
  if (!getGestureInfo || !hGestureInfo || !pGestureInfo)
    return false;

  ZeroMemory(pGestureInfo, sizeof(GESTUREINFO));
  pGestureInfo->cbSize = sizeof(GESTUREINFO);

  return getGestureInfo(hGestureInfo, pGestureInfo);
}

bool nsWinGesture::CloseGestureInfoHandle(HGESTUREINFO hGestureInfo)
{
  if (!getGestureInfo || !hGestureInfo)
    return false;

  return closeGestureInfoHandle(hGestureInfo);
}

bool nsWinGesture::GetGestureExtraArgs(HGESTUREINFO hGestureInfo, UINT cbExtraArgs, PBYTE pExtraArgs)
{
  if (!getGestureInfo || !hGestureInfo || !pExtraArgs)
    return false;

  return getGestureExtraArgs(hGestureInfo, cbExtraArgs, pExtraArgs);
}

bool nsWinGesture::SetGestureConfig(HWND hWnd, UINT cIDs, PGESTURECONFIG pGestureConfig)
{
  if (!getGestureInfo || !pGestureConfig)
    return false;

  return setGestureConfig(hWnd, 0, cIDs, pGestureConfig, sizeof(GESTURECONFIG));
}

bool nsWinGesture::GetGestureConfig(HWND hWnd, DWORD dwFlags, PUINT pcIDs, PGESTURECONFIG pGestureConfig)
{
  if (!getGestureInfo || !pGestureConfig)
    return false;

  return getGestureConfig(hWnd, 0, dwFlags, pcIDs, pGestureConfig, sizeof(GESTURECONFIG));
}

bool nsWinGesture::BeginPanningFeedback(HWND hWnd)
{
  if (!beginPanningFeedback)
    return false;

  return beginPanningFeedback(hWnd);
}

bool nsWinGesture::EndPanningFeedback(HWND hWnd)
{
  if (!beginPanningFeedback)
    return false;

  return endPanningFeedback(hWnd, TRUE);
}

bool nsWinGesture::UpdatePanningFeedback(HWND hWnd, LONG offsetX, LONG offsetY, BOOL fInInertia)
{
  if (!beginPanningFeedback)
    return false;

  return updatePanningFeedback(hWnd, offsetX, offsetY, fInInertia);
}

bool nsWinGesture::IsPanEvent(LPARAM lParam)
{
  GESTUREINFO gi;

  ZeroMemory(&gi,sizeof(GESTUREINFO));
  gi.cbSize = sizeof(GESTUREINFO);

  BOOL result = GetGestureInfo((HGESTUREINFO)lParam, &gi);
  if (!result)
    return false;

  if (gi.dwID == GID_PAN)
    return true;

  return false;
}

/* Gesture event processing */

bool
nsWinGesture::ProcessGestureMessage(HWND hWnd, WPARAM wParam, LPARAM lParam, nsSimpleGestureEvent& evt)
{
  GESTUREINFO gi;

  ZeroMemory(&gi,sizeof(GESTUREINFO));
  gi.cbSize = sizeof(GESTUREINFO);

  BOOL result = GetGestureInfo((HGESTUREINFO)lParam, &gi);
  if (!result)
    return false;

  // The coordinates of this event
  nsPointWin coord;
  coord = gi.ptsLocation;
  coord.ScreenToClient(hWnd);

  evt.refPoint.x = coord.x;
  evt.refPoint.y = coord.y;

  // Multiple gesture can occur at the same time so gesture state
  // info can't be shared.
  switch(gi.dwID)
  {
    case GID_BEGIN:
    case GID_END:
      // These should always fall through to DefWndProc
      return false;
      break;

    case GID_ZOOM:
    {
      if (gi.dwFlags & GF_BEGIN) {
        // Send a zoom start event

        // The low 32 bits are the distance in pixels.
        mZoomIntermediate = (float)gi.ullArguments;

        evt.message = NS_SIMPLE_GESTURE_MAGNIFY_START;
        evt.delta = 0.0;
      }
      else {
        // Send a zoom intermediate/end event, the delta is the change
        // in touch points.
        evt.message = NS_SIMPLE_GESTURE_MAGNIFY_UPDATE;
        // (positive for a "zoom in")
        evt.delta = -1.0 * (mZoomIntermediate - (float)gi.ullArguments);
        mZoomIntermediate = (float)gi.ullArguments;
      }
    }
    break;

    case GID_ROTATE:
    {
      // Send a rotate start event
      double radians = 0.0;

      // On GF_BEGIN, ullArguments contains the absolute rotation at the
      // start of the gesture. In later events it contains the offset from
      // the start angle.
      if (gi.ullArguments != 0)
        radians = GID_ROTATE_ANGLE_FROM_ARGUMENT(gi.ullArguments);

      double degrees = -1 * radians * (180/M_PI);

      if (gi.dwFlags & GF_BEGIN) {
          // At some point we should pass the initial angle in
          // along with delta. It's useful.
          degrees = mRotateIntermediate = 0.0;
      }

      evt.direction = 0;
      evt.delta = degrees - mRotateIntermediate;
      mRotateIntermediate = degrees;

      if (evt.delta > 0)
        evt.direction = nsIDOMSimpleGestureEvent::ROTATION_COUNTERCLOCKWISE;
      else if (evt.delta < 0)
        evt.direction = nsIDOMSimpleGestureEvent::ROTATION_CLOCKWISE;

      if (gi.dwFlags & GF_BEGIN)
        evt.message = NS_SIMPLE_GESTURE_ROTATE_START;
      else if (gi.dwFlags & GF_END)
        evt.message = NS_SIMPLE_GESTURE_ROTATE;
      else
        evt.message = NS_SIMPLE_GESTURE_ROTATE_UPDATE;
    }
    break;

    case GID_TWOFINGERTAP:
    {
      // Normally maps to "restore" from whatever you may have recently changed. A simple
      // double click.
      evt.message = NS_SIMPLE_GESTURE_TAP;
    }
    break;

    case GID_PRESSANDTAP:
    {
      // Two finger right click. Defaults to right click if it falls through.
      evt.message = NS_SIMPLE_GESTURE_PRESSTAP;
    }
    break;
  }

  return true;
}

bool
nsWinGesture::ProcessPanMessage(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
  GESTUREINFO gi;

  ZeroMemory(&gi,sizeof(GESTUREINFO));
  gi.cbSize = sizeof(GESTUREINFO);

  BOOL result = GetGestureInfo((HGESTUREINFO)lParam, &gi);
  if (!result)
    return false;

  // The coordinates of this event
  nsPointWin coord;
  coord = mPanRefPoint = gi.ptsLocation;
  // We want screen coordinates in our local offsets as client coordinates will change
  // when feedback is taking place. Gui events though require client coordinates. 
  mPanRefPoint.ScreenToClient(hWnd);

  switch(gi.dwID)
  {
    case GID_BEGIN:
    case GID_END:
      // These should always fall through to DefWndProc
      return false;
      break;

    // Setup pixel scroll events for both axis
    case GID_PAN:
    {
      if (gi.dwFlags & GF_BEGIN) {
        mPanIntermediate = coord;
        mPixelScrollDelta = 0;
        mPanActive = true;
        mPanInertiaActive = false;
      }
      else {

#ifdef DBG_jimm
        PRInt32 deltaX = mPanIntermediate.x - coord.x;
        PRInt32 deltaY = mPanIntermediate.y - coord.y;
        PR_LOG(gWindowsLog, PR_LOG_ALWAYS, 
               ("coordX=%d coordY=%d deltaX=%d deltaY=%d x:%d y:%d\n", coord.x,
                coord.y, deltaX, deltaY, mXAxisFeedback, mYAxisFeedback));
#endif

        mPixelScrollDelta.x = mPanIntermediate.x - coord.x;
        mPixelScrollDelta.y = mPanIntermediate.y - coord.y;
        mPanIntermediate = coord;

        if (gi.dwFlags & GF_INERTIA)
          mPanInertiaActive = true;

        if (gi.dwFlags & GF_END) {
          mPanActive = false;
          mPanInertiaActive = false;
          PanFeedbackFinalize(hWnd, true);
        }
      }
    }
    break;
  }
  return true;
}

inline bool TestTransition(PRInt32 a, PRInt32 b)
{
  // If a is zero, overflow is zero, implying the cursor has moved back to the start position.
  // If b is zero, cached overscroll is zero, implying feedback just begun. 
  if (a == 0 || b == 0) return true;
  // Test for different signs.
  return (a < 0) == (b < 0);
}

void
nsWinGesture::UpdatePanFeedbackX(HWND hWnd, PRInt32 scrollOverflow, bool& endFeedback)
{
  // If scroll overflow was returned indicating we panned past the bounds of
  // the scrollable view port, start feeback.
  if (scrollOverflow != 0) {
    if (!mFeedbackActive) {
      BeginPanningFeedback(hWnd);
      mFeedbackActive = true;
    }      
    endFeedback = false;
    mXAxisFeedback = true;
    return;
  }
  
  if (mXAxisFeedback) {
    PRInt32 newOverflow = mPixelScrollOverflow.x - mPixelScrollDelta.x;

    // Detect a reverse transition past the starting drag point. This tells us the user
    // has panned all the way back so we can stop providing feedback for this axis.
    if (!TestTransition(newOverflow, mPixelScrollOverflow.x) || newOverflow == 0)
      return;

    // Cache the total over scroll in pixels.
    mPixelScrollOverflow.x = newOverflow;
    endFeedback = false;
  }
}

void
nsWinGesture::UpdatePanFeedbackY(HWND hWnd, PRInt32 scrollOverflow, bool& endFeedback)
{
  // If scroll overflow was returned indicating we panned past the bounds of
  // the scrollable view port, start feeback.
  if (scrollOverflow != 0) {
    if (!mFeedbackActive) {
      BeginPanningFeedback(hWnd);
      mFeedbackActive = true;
    }
    endFeedback = false;
    mYAxisFeedback = true;
    return;
  }
  
  if (mYAxisFeedback) {
    PRInt32 newOverflow = mPixelScrollOverflow.y - mPixelScrollDelta.y;

    // Detect a reverse transition past the starting drag point. This tells us the user
    // has panned all the way back so we can stop providing feedback for this axis.
    if (!TestTransition(newOverflow, mPixelScrollOverflow.y) || newOverflow == 0)
      return;

    // Cache the total over scroll in pixels.
    mPixelScrollOverflow.y = newOverflow;
    endFeedback = false;
  }
}

void
nsWinGesture::PanFeedbackFinalize(HWND hWnd, bool endFeedback)
{
  if (!mFeedbackActive)
    return;

  if (endFeedback) {
    mFeedbackActive = false;
    mXAxisFeedback = false;
    mYAxisFeedback = false;
    mPixelScrollOverflow = 0;
    EndPanningFeedback(hWnd);
    return;
  }

  UpdatePanningFeedback(hWnd, mPixelScrollOverflow.x, mPixelScrollOverflow.y, mPanInertiaActive);
}

bool
nsWinGesture::PanDeltaToPixelScrollX(nsMouseScrollEvent& evt)
{
  evt.delta = 0;
  evt.scrollOverflow = 0;

  // Don't scroll the view if we are currently at a bounds, or, if we are
  // panning back from a max feedback position. This keeps the original drag point
  // constant.
  if (mXAxisFeedback)
    return false;

  if (mPixelScrollDelta.x != 0)
  {
    evt.scrollFlags = nsMouseScrollEvent::kIsHorizontal|
                      nsMouseScrollEvent::kHasPixels|
                      nsMouseScrollEvent::kNoLines|
                      nsMouseScrollEvent::kNoDefer;
    evt.delta = mPixelScrollDelta.x;
    evt.refPoint.x = mPanRefPoint.x;
    evt.refPoint.y = mPanRefPoint.y;
    return true;
  }
  return false;
}

bool
nsWinGesture::PanDeltaToPixelScrollY(nsMouseScrollEvent& evt)
{
  evt.delta = 0;
  evt.scrollOverflow = 0;

  // Don't scroll the view if we are currently at a bounds, or, if we are
  // panning back from a max feedback position. This keeps the original drag point
  // constant.
  if (mYAxisFeedback)
    return false;

  if (mPixelScrollDelta.y != 0)
  {
    evt.scrollFlags = nsMouseScrollEvent::kIsVertical|
                      nsMouseScrollEvent::kHasPixels|
                      nsMouseScrollEvent::kNoLines|
                      nsMouseScrollEvent::kNoDefer;
    evt.delta = mPixelScrollDelta.y;
    evt.refPoint.x = mPanRefPoint.x;
    evt.refPoint.y = mPanRefPoint.y;
    return true;
  }
  return false;
}
