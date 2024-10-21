#pragma once

#include <vector>

#define NS_PROTO_MENU_POS 1000001

enum
{
	NSM_FIRST = WM_USER + 100,

	// wParam = fist item
	// lParam = iLast item
	// result = number of total selected items
	NSM_SELECTITEMS = NSM_FIRST,

	//
	NSM_SEEKTIME,

	// 
	NSM_SET_OPTIONS,  // options were changed

	NSM_LAST
};

/////////////////////////////////////////////////////////////////////////////////////////
// NS get SRMM dialog
// returns a pointer to the parent SRMM dialog, if used as a log window, or nullptr otherwise

__forceinline CSrmmBaseDialog* NS_GetSrmm(HANDLE hwnd)
{
	return (CSrmmBaseDialog *)CallService("NewStory/GetSrmm", WPARAM(hwnd), 0);
}

/////////////////////////////////////////////////////////////////////////////////////////
// NS get current event

__forceinline MEVENT NS_GetCurrent(HANDLE hwnd)
{
	return (MEVENT)CallService("NewStory/GetCurrent", WPARAM(hwnd), 0);
}

/////////////////////////////////////////////////////////////////////////////////////////
// NS get selection

__forceinline std::vector<MEVENT> NS_GetSelection(HANDLE hwnd)
{
	std::vector<MEVENT> ret;
	CallService("NewStory/GetSelection", WPARAM(hwnd), LPARAM(&ret));
	return ret;
}

/////////////////////////////////////////////////////////////////////////////////////////
// NS menu item

struct NSMenuExecParam
{
	char *szServiceName;
	int iParam;
};

__forceinline HGENMENU Menu_AddNewStoryMenuItem(TMO_MenuItem *pmi, int param)
{
	return (HGENMENU)CallService("NSMenu/AddService", (WPARAM)pmi, param);
}

/////////////////////////////////////////////////////////////////////////////////////////
// service for refreshing downloaded files

__forceinline void NS_NotifyFileReady(const wchar_t *pwszFileName)
{
	CallService("NewStory/FileReady", (WPARAM)pwszFileName, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////
// service for refreshing downloaded files

__forceinline void NS_NotifyRemoteRead(MCONTACT hContact, MEVENT hEvent)
{
	CallService("NewStory/RemoteRead", hContact, hEvent);
}

/////////////////////////////////////////////////////////////////////////////////////////
// event for changing NewStory menu items
// wparam = (MCONTACT)hContact - contact id
// lparam = (DB::EventInfo*)dbei - event

#define ME_NS_PREBUILDMENU  "NewStory/PreBuildMenu"
