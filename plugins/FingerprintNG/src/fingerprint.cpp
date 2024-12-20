/*
Fingerprint NG (client version) icons module for Miranda NG
Copyright © 2006-24 ghazan, mataes, HierOS, FYR, Bio, nullbie, faith_healer and all respective contributors.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

//Start of header
#include "stdafx.h"

KN_FP_MASK *g_pNotFound, *g_pUnknown;
LIST<KN_FP_MASK> g_arCommon(10), g_overlay1(10), g_overlay2(10), g_overlay3(10), g_overlay4(10);
wchar_t g_szSkinLib[MAX_PATH];

static UINT g_LPCodePage;
static HANDLE hExtraIcon = nullptr;
static HANDLE hFolderChanged = nullptr, hIconFolder = nullptr;

static int CompareFI(const FOUNDINFO *p1, const FOUNDINFO *p2)
{
	if (p1->iBase != p2->iBase)
		return p1->iBase - p2->iBase;
	return p1->iOverlay - p2->iOverlay;
}

static OBJLIST<FOUNDINFO> arFI(50, CompareFI);

static LIST<void> arMonitoredWindows(3, PtrKeySortT);

/////////////////////////////////////////////////////////////////////////////////////////
// Futher routines is for creating joined 'overlay' icons.

//	CreateBitmap32 - Create DIB 32 bitmap with sizes cx*cy and put reference
//			to new bitmap pixel image memory area to void ** bits

HBITMAP __fastcall CreateBitmap32(int cx, int cy, LPBYTE &bits)
{
	if (cx < 0 || cy < 0)
		return nullptr;

	LPVOID ptPixels = nullptr;
	BITMAPINFO bmpi = {};
	bmpi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmpi.bmiHeader.biWidth = cx;
	bmpi.bmiHeader.biHeight = cy;
	bmpi.bmiHeader.biPlanes = 1;
	bmpi.bmiHeader.biBitCount = 32;
	
	HBITMAP DirectBitmap = CreateDIBSection(nullptr, &bmpi, DIB_RGB_COLORS, &ptPixels, nullptr, 0);

	GdiFlush();
	if (ptPixels)
		memset(ptPixels, 0, cx * cy * 4);

	bits = (LPBYTE)ptPixels;
	return DirectBitmap;
}

/////////////////////////////////////////////////////////////////////////////////////////
// checkHasAlfa - checks if image has at least one uint8_t in alpha channel
//			 that is not a 0. (is image real 32 bit or just 24 bit)

static bool checkHasAlfa(LPBYTE from, int width, int height)
{
	LPDWORD pt = (LPDWORD)from;
	LPDWORD lim = pt + width * height;
	while (pt < lim) {
		if (*pt & 0xFF000000)
			return true;
		pt++;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
// checkMaskUsed - checks if mask image has at least one that is not a 0.
// Not sure is it required or not

static bool checkMaskUsed(LPBYTE from)
{
	for (int i = 0; i < 16 * 16 / 8; i++)
		if (from[i] != 0)
			return true;

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
// GetMaskBit - return value of apropriate mask bit in line at x position

BOOL __inline GetMaskBit(LPBYTE line, int x)
{
	return ((*(line + (x >> 3))) & (0x01 << (7 - (x & 0x07)))) != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// blend - alpha blend ARGB values of 2 pixels. X1 - underlaying, X2 - overlaying points.

static uint32_t blend(uint32_t X1, uint32_t X2)
{
	RGBA *q1 = (RGBA *)&X1;
	RGBA *q2 = (RGBA *)&X2;
	uint8_t a_1 = ~q1->a;
	uint8_t a_2 = ~q2->a;
	uint16_t am = q1->a * a_2;

	uint16_t ar = q1->a + ((a_1 * q2->a) / 255);
	// if a2 more than 0 than result should be more
	// or equal (if a1==0) to a2, else in combination
	// with mask we can get here black points

	ar = (q2->a > ar) ? q2->a : ar;

	if (ar == 0) return 0;

	{
		uint16_t arm = ar * 255;
		uint16_t rr = ((q1->r * am + q2->r * q2->a * 255)) / arm;
		uint16_t gr = ((q1->g * am + q2->g * q2->a * 255)) / arm;
		uint16_t br = ((q1->b * am + q2->b * q2->a * 255)) / arm;
		return (ar << 24) | (rr << 16) | (gr << 8) | br;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// CreateJoinedIcon - creates new icon by drawing hTop over hBottom.

static HICON CreateJoinedIcon(HICON hBottom, HICON hTop)
{
	BOOL drawn = FALSE;
	HDC tempDC, tempDC2, tempDC3;
	HICON res = nullptr;
	HBITMAP oImage, nImage;
	HBITMAP nMask, hbm, obmp, obmp2;
	LPBYTE ptPixels = nullptr;
	ICONINFO iNew = { 0 };
	uint8_t p[32] = { 0 };

	tempDC = CreateCompatibleDC(nullptr);
	nImage = CreateBitmap32(16, 16, ptPixels);
	oImage = (HBITMAP)SelectObject(tempDC, nImage);

	ICONINFO iciBottom = { 0 };
	ICONINFO iciTop = { 0 };

	BITMAP bmp_top = { 0 };
	BITMAP bmp_top_mask = { 0 };

	BITMAP bmp_bottom = { 0 };
	BITMAP bmp_bottom_mask = { 0 };

	GetIconInfo(hBottom, &iciBottom);
	GetObject(iciBottom.hbmColor, sizeof(BITMAP), &bmp_bottom);
	GetObject(iciBottom.hbmMask, sizeof(BITMAP), &bmp_bottom_mask);

	GetIconInfo(hTop, &iciTop);
	GetObject(iciTop.hbmColor, sizeof(BITMAP), &bmp_top);
	GetObject(iciTop.hbmMask, sizeof(BITMAP), &bmp_top_mask);

	if (bmp_bottom.bmBitsPixel == 32 && bmp_top.bmBitsPixel == 32) {
		LPBYTE BottomBuffer, TopBuffer, BottomMaskBuffer, TopMaskBuffer;
		LPBYTE bb, tb, bmb, tmb;
		LPBYTE db = ptPixels;
		int vstep_d = 16 * 4;
		int vstep_b = bmp_bottom.bmWidthBytes;
		int vstep_t = bmp_top.bmWidthBytes;
		int vstep_bm = bmp_bottom_mask.bmWidthBytes;
		int vstep_tm = bmp_top_mask.bmWidthBytes;

		if (bmp_bottom.bmBits)
			bb = BottomBuffer = (LPBYTE)bmp_bottom.bmBits;
		else {
			BottomBuffer = (LPBYTE)_alloca(bmp_bottom.bmHeight * bmp_bottom.bmWidthBytes);
			GetBitmapBits(iciBottom.hbmColor, bmp_bottom.bmHeight * bmp_bottom.bmWidthBytes, BottomBuffer);
			bb = BottomBuffer + vstep_b * (bmp_bottom.bmHeight - 1);
			vstep_b = -vstep_b;
		}
		if (bmp_top.bmBits)
			tb = TopBuffer = (LPBYTE)bmp_top.bmBits;
		else {
			TopBuffer = (LPBYTE)_alloca(bmp_top.bmHeight * bmp_top.bmWidthBytes);
			GetBitmapBits(iciTop.hbmColor, bmp_top.bmHeight * bmp_top.bmWidthBytes, TopBuffer);
			tb = TopBuffer + vstep_t * (bmp_top.bmHeight - 1);
			vstep_t = -vstep_t;
		}
		if (bmp_bottom_mask.bmBits)
			bmb = BottomMaskBuffer = (LPBYTE)bmp_bottom_mask.bmBits;
		else {
			BottomMaskBuffer = (LPBYTE)_alloca(bmp_bottom_mask.bmHeight * bmp_bottom_mask.bmWidthBytes);
			GetBitmapBits(iciBottom.hbmMask, bmp_bottom_mask.bmHeight * bmp_bottom_mask.bmWidthBytes, BottomMaskBuffer);
			bmb = BottomMaskBuffer + vstep_bm * (bmp_bottom_mask.bmHeight - 1);
			vstep_bm = -vstep_bm;
		}
		if (bmp_top_mask.bmBits)
			tmb = TopMaskBuffer = (LPBYTE)bmp_top_mask.bmBits;
		else {
			TopMaskBuffer = (LPBYTE)_alloca(bmp_top_mask.bmHeight * bmp_top_mask.bmWidthBytes);
			GetBitmapBits(iciTop.hbmMask, bmp_top_mask.bmHeight * bmp_top_mask.bmWidthBytes, TopMaskBuffer);
			tmb = TopMaskBuffer + vstep_tm * (bmp_top_mask.bmHeight - 1);
			vstep_tm = -vstep_tm;
		}
		{
			bool topHasAlpha = checkHasAlfa(TopBuffer, bmp_top.bmWidth, bmp_top.bmHeight);
			bool bottomHasAlpha = checkHasAlfa(BottomBuffer, bmp_bottom.bmWidth, bmp_bottom.bmHeight);
			bool topMaskUsed = !topHasAlpha && checkMaskUsed(TopMaskBuffer);
			bool bottomMaskUsed = !bottomHasAlpha && checkMaskUsed(BottomMaskBuffer);

			for (int y = 0; y < 16; y++) {
				for (int x = 0; x < 16; x++) {
					uint32_t bottom_d = ((LPDWORD)bb)[x];
					uint32_t top_d = ((LPDWORD)tb)[x];

					if (topMaskUsed) {
						if (GetMaskBit(tmb, x))
							top_d &= 0x00FFFFFF;
						else
							top_d |= 0xFF000000;
					}
					else if (!topHasAlpha)
						top_d |= 0xFF000000;

					if (bottomMaskUsed) {
						if (GetMaskBit(bmb, x))
							bottom_d &= 0x00FFFFFF;
						else
							bottom_d |= 0xFF000000;
					}
					else if (!bottomHasAlpha)
						bottom_d |= 0xFF000000;

					((LPDWORD)db)[x] = blend(bottom_d, top_d);
				}
				bb += vstep_b;
				tb += vstep_t;
				bmb += vstep_bm;
				tmb += vstep_tm;
				db += vstep_d;
			}
		}

		drawn = TRUE;
	}

	DeleteObject(iciBottom.hbmColor);
	DeleteObject(iciTop.hbmColor);
	DeleteObject(iciBottom.hbmMask);
	DeleteObject(iciTop.hbmMask);

	if (!drawn) {
		DrawIconEx(tempDC, 0, 0, hBottom, 16, 16, 0, nullptr, DI_NORMAL);
		DrawIconEx(tempDC, 0, 0, hTop, 16, 16, 0, nullptr, DI_NORMAL);
	}

	nMask = CreateBitmap(16, 16, 1, 1, p);
	tempDC2 = CreateCompatibleDC(nullptr);
	tempDC3 = CreateCompatibleDC(nullptr);
	hbm = CreateCompatibleBitmap(tempDC3, 16, 16);
	obmp = (HBITMAP)SelectObject(tempDC2, nMask);
	obmp2 = (HBITMAP)SelectObject(tempDC3, hbm);
	DrawIconEx(tempDC2, 0, 0, hBottom, 16, 16, 0, nullptr, DI_MASK);
	DrawIconEx(tempDC3, 0, 0, hTop, 16, 16, 0, nullptr, DI_MASK);
	BitBlt(tempDC2, 0, 0, 16, 16, tempDC3, 0, 0, SRCAND);

	GdiFlush();

	SelectObject(tempDC2, obmp);
	DeleteDC(tempDC2);

	SelectObject(tempDC3, obmp2);
	DeleteDC(tempDC3);

	SelectObject(tempDC, oImage);
	DeleteDC(tempDC);

	DeleteObject(hbm);

	iNew.fIcon = TRUE;
	iNew.hbmColor = nImage;
	iNew.hbmMask = nMask;
	res = CreateIconIndirect(&iNew);

	DeleteObject(nImage);
	DeleteObject(nMask);

	return res;
}

/////////////////////////////////////////////////////////////////////////////////////////
// CreateIconFromIndexes
// returns hIcon of joined icon by given indexes

static HICON CreateIconFromIndexes(KN_FP_MASK *base, KN_FP_MASK *overlay1, KN_FP_MASK *overlay2, KN_FP_MASK *overlay3, KN_FP_MASK *overlay4)
{
	HICON hIcon = nullptr;	// returned HICON
	HICON hTmp = nullptr;
	HICON icOverlay = nullptr;
	HICON icOverlay2 = nullptr;
	HICON icOverlay3 = nullptr;
	HICON icOverlay4 = nullptr;

	HICON icMain = IcoLib_GetIconByHandle(base->hIcolibItem);
	if (icMain) {
		icOverlay = (overlay1 == nullptr) ? nullptr : IcoLib_GetIconByHandle(overlay1->hIcolibItem);
		icOverlay2 = (overlay2 == nullptr) ? nullptr : IcoLib_GetIconByHandle(overlay2->hIcolibItem);
		icOverlay3 = (overlay3 == nullptr) ? nullptr : IcoLib_GetIconByHandle(overlay3->hIcolibItem);
		icOverlay4 = (overlay4 == nullptr) ? nullptr : IcoLib_GetIconByHandle(overlay4->hIcolibItem);

		hIcon = icMain;

		if (overlay1)
			hTmp = hIcon = CreateJoinedIcon(hIcon, icOverlay);

		if (overlay2) {
			hIcon = CreateJoinedIcon(hIcon, icOverlay2);
			if (hTmp) DestroyIcon(hTmp);
			hTmp = hIcon;
		}

		if (overlay3) {
			hIcon = CreateJoinedIcon(hIcon, icOverlay3);
			if (hTmp) DestroyIcon(hTmp);
			hTmp = hIcon;
		}

		if (overlay4) {
			hIcon = CreateJoinedIcon(hIcon, icOverlay4);
			if (hTmp) DestroyIcon(hTmp);
		}
	}

	if (hIcon && hIcon == icMain)
		hIcon = CopyIcon(icMain);

	IcoLib_ReleaseIcon(icMain);
	IcoLib_ReleaseIcon(icOverlay);
	IcoLib_ReleaseIcon(icOverlay2);
	IcoLib_ReleaseIcon(icOverlay3);
	IcoLib_ReleaseIcon(icOverlay4);
	return hIcon;
}

/////////////////////////////////////////////////////////////////////////////////////////
// GetIconsIndexes
// Retrieves Icons indexes by Mirver

static void GetIconsIndexes(LPWSTR wszMirVer, KN_FP_MASK *&base, KN_FP_MASK *&overlay1, KN_FP_MASK *&overlay2, KN_FP_MASK *&overlay3, KN_FP_MASK *&overlay4)
{
	base = overlay1 = overlay2 = overlay3 = overlay4 = nullptr;

	if (mir_wstrcmp(wszMirVer, L"?") == 0) {
		base = g_pUnknown;
		return;
	}

	LPWSTR wszMirVerUp = NEWWSTR_ALLOCA(wszMirVer);
	_wcsupr(wszMirVerUp);

	for (auto &it : g_arCommon) {
		if (it->hIcolibItem == nullptr)
			continue;

		if (!WildCompare(wszMirVerUp, it->szMaskUpper))
			continue;

		base = it;
		if (it->iIconIndex != IDI_NOTFOUND && it->iIconIndex != IDI_UNKNOWN && it->iIconIndex != IDI_UNDETECTED) {
			wchar_t destfile[MAX_PATH];
			wcsncpy_s(destfile, g_szSkinLib, _TRUNCATE);

			struct _stat64i32 stFileInfo;
			if (_wstat(destfile, &stFileInfo) == -1)
				base = g_pNotFound;
		}
		break;
	}

	if (base == nullptr)
		base = g_pUnknown;

	else if (!base->fNotUseOverlay) {
		for (auto &it : g_overlay1) {
			if (!WildCompare(wszMirVerUp, it->szMaskUpper))
				continue;

			struct _stat64i32 stFileInfo;
			if (_wstat(g_szSkinLib, &stFileInfo) != -1) {
				overlay1 = it;
				break;
			}
		}

		for (auto &it : g_overlay2) {
			if (WildCompare(wszMirVerUp, it->szMaskUpper)) {
				overlay2 = it;
				break;
			}
		}

		for (auto &it : g_overlay3) {
			if (WildCompare(wszMirVerUp, it->szMaskUpper)) {
				overlay3 = it;
				break;
			}
		}

		for (auto &it : g_overlay4) {
			if (it->hIcolibItem == nullptr)
				continue;

			if (WildCompare(wszMirVerUp, it->szMaskUpper)) {
				overlay4 = it;
				break;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// ServiceGetClientIcon
// MS_FP_GETCLIENTICONW service implementation.
// wParam - LPWSTR MirVer value to get client for.
// lParam - int noCopy - if wParam is equal to "1"	will return icon handler without copiing icon.
// ICON IS ALWAYS COPIED!!!

static INT_PTR ServiceGetClientIcon(WPARAM wParam, LPARAM)
{
	LPWSTR wszMirVer = (LPWSTR)wParam;			// MirVer value to get client for.
	if (wszMirVer == nullptr)
		return 0;

	KN_FP_MASK *base, *overlay1, *overlay2, *overlay3, *overlay4;
	GetIconsIndexes(wszMirVer, base, overlay1, overlay2, overlay3, overlay4);

	HICON hIcon = nullptr;			// returned HICON
	if (base != nullptr)
		hIcon = CreateIconFromIndexes(base, overlay1, overlay2, overlay3, overlay4);

	return (INT_PTR)hIcon;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ApplyFingerprintImage
// 1) Try to find appropriate mask
// 2) Register icon in extraimage list if not yet registered (EMPTY_EXTRA_ICON)
// 3) Set ExtraImage for contact

static void SetSrmmIcon(MCONTACT hContact, LPTSTR ptszMirver)
{
	if (mir_wstrlen(ptszMirver))
		Srmm_ModifyIcon(hContact, MODULENAME, 1, (HICON)ServiceGetClientIcon((WPARAM)ptszMirver, TRUE), ptszMirver);
	else
		Srmm_SetIconFlags(hContact, MODULENAME, 1, MBF_HIDDEN);
}

static int ApplyFingerprintImage(MCONTACT hContact, LPTSTR szMirVer)
{
	if (hContact == NULL)
		return 0;

	HANDLE hImage = INVALID_HANDLE_VALUE;
	if (szMirVer) {
		KN_FP_MASK *base, *overlay1, *overlay2, *overlay3, *overlay4;
		GetIconsIndexes(szMirVer, base, overlay1, overlay2, overlay3, overlay4);
		if (base) {
			// MAX: 256 + 64 + 64 + 64 + 64
			FOUNDINFO tmp = { base->idx, 0 };
			if (overlay1) tmp.iOverlay += ((overlay1->idx & 0xFF) << 18);
			if (overlay2) tmp.iOverlay += ((overlay2->idx & 0x3F) << 12);
			if (overlay3) tmp.iOverlay += ((overlay3->idx & 0x3F) << 6);
			if (overlay4) tmp.iOverlay += (overlay4->idx & 0x3F);

			auto *F = arFI.find(&tmp); // not found - then add
			if (F == nullptr) {
				F = new FOUNDINFO(tmp);
				HICON hIcon = CreateIconFromIndexes(base, overlay1, overlay2, overlay3, overlay4);
				if (hIcon != nullptr) {
					F->hRegisteredImage = ExtraIcon_AddIcon(hIcon);
					DestroyIcon(hIcon);
				}
				else F->hRegisteredImage = INVALID_HANDLE_VALUE;

				arFI.insert(F);
			}

			hImage = F->hRegisteredImage;
		}
	}

	ExtraIcon_SetIcon(hExtraIcon, hContact, hImage);

	if (arMonitoredWindows.getIndex((HANDLE)hContact) != -1)
		SetSrmmIcon(hContact, szMirVer);

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// WildCompare
// Compare 'name' string with 'mask' strings.
// Masks can contain '*' or '?' wild symbols
// Asterics '*' symbol covers 'empty' symbol too e.g WildCompare("Tst","T*st*"), returns TRUE
// In order to handle situation 'at least one any sybol' use "?*" combination:
// e.g WildCompare("Tst","T?*st*"), returns FALSE, but both WildCompare("Test","T?*st*") and
// WildCompare("Teeest","T?*st*") return TRUE.
//
// Function is case sensitive! so convert input or modify func to use _qtoupper()
//
// Mask can contain several submasks. In this case each submask (including first)
// should start from '|' e.g: "|first*submask|second*mask".
//
// Dec 25, 2006 by FYR:
// Added Exception to masks: the mask "|^mask3|mask2|mask1" means:
// if NOT according to mask 3 AND (mask1 OR mask2)
// EXCEPTION should be BEFORE main mask:
// 	IF Exception match - the comparing stops as FALSE
// 	IF Exception does not match - comparing continue
// 	IF Mask match - comparing stops as TRUE
// 	IF Mask does not not match comparing continue

BOOL __fastcall WildCompare(LPWSTR wszName, LPWSTR wszMask)
{
	if (wszMask == nullptr)
		return NULL;

	if (*wszMask != L'|')
		return wildcmpw(wszName, wszMask);

	size_t s = 1, e = 1;
	LPWSTR wszTemp = (LPWSTR)_alloca(mir_wstrlen(wszMask) * sizeof(wchar_t) + sizeof(wchar_t));
	BOOL bExcept;

	while (wszMask[e] != L'\0')
	{
		s = e;
		while (wszMask[e] != L'\0' && wszMask[e] != L'|') e++;

		// exception mask
		bExcept = (*(wszMask + s) == L'^');
		if (bExcept) s++;

		memcpy(wszTemp, wszMask + s, (e - s) * sizeof(wchar_t));
		wszTemp[e - s] = L'\0';

		if (wildcmpw(wszName, wszTemp))
			return !bExcept;

		if (wszMask[e] != L'\0')
			e++;
		else
			return FALSE;
	}
	return FALSE;
}

VOID ClearFI()
{
	arFI.destroy();
}

/////////////////////////////////////////////////////////////////////////////////////////
// ServiceGetClientDescrW
// MS_FP_GETCLIENTDESCRW service implementation.
// wParam - LPCWSTR MirVer value
// lParam - (NULL) unused
// returns LPCWSTR: client description (do not destroy) or NULL

static INT_PTR ServiceGetClientDescr(WPARAM wParam, LPARAM)
{
	LPWSTR wszMirVer = (LPWSTR)wParam;  // MirVer value to get client for.
	if (wszMirVer == nullptr)
		return 0;

	LPWSTR wszMirVerUp = NEWWSTR_ALLOCA(wszMirVer); _wcsupr(wszMirVerUp);
	if (mir_wstrcmp(wszMirVerUp, L"?") == 0)
		return (INT_PTR)g_pUnknown->szClientDescription;

	for (auto &it : g_arCommon)
		if (WildCompare(wszMirVerUp, it->szMaskUpper))
			return (INT_PTR)it->szClientDescription;

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////
// ServiceSameClientW
// MS_FP_SAMECLIENTSW service implementation.
// wParam - LPWSTR first MirVer value
// lParam - LPWSTR second MirVer value
// returns LPCWSTR: client description (do not destroy) if clients are same or NULL

static INT_PTR ServiceSameClients(WPARAM wParam, LPARAM lParam)
{
	if (!wParam || !lParam)
		return NULL; //one of its is not null

	INT_PTR res1 = ServiceGetClientDescr(wParam, 0);
	INT_PTR res2 = ServiceGetClientDescr(lParam, 0);
	return (res1 == res2 && res1 != 0) ? res1 : NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////
// OnExtraIconListRebuild
// Set all registered indexes in array to EMPTY_EXTRA_ICON (unregistered icon)

static int OnExtraIconListRebuild(WPARAM, LPARAM)
{
	ClearFI();
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// OnIconsChanged

static int OnIconsChanged(WPARAM, LPARAM)
{
	ClearFI();
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// OnExtraImageApply
// Try to get MirVer value from db for contact and if success calls ApplyFingerprintImage

int OnExtraImageApply(WPARAM hContact, LPARAM)
{
	if (hContact == NULL)
		return 0;

	ptrW tszMirver;
	char *szProto = Proto_GetBaseAccountName(hContact);
	if (szProto != nullptr)
		tszMirver = db_get_wsa(hContact, szProto, "MirVer");

	ApplyFingerprintImage(hContact, tszMirver);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// OnContactSettingChanged
// if contact settings changed apply new image or remove it

static int OnContactSettingChanged(WPARAM hContact, LPARAM lParam)
{
	if (hContact == NULL)
		return 0;

	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING*)lParam;
	if (cws && cws->szSetting && !strcmp(cws->szSetting, "MirVer")) {
		switch (cws->value.type) {
		case DBVT_UTF8:
			ApplyFingerprintImage(hContact, ptrW(mir_utf8decodeW(cws->value.pszVal)));
			break;
		case DBVT_ASCIIZ:
			ApplyFingerprintImage(hContact, _A2T(cws->value.pszVal));
			break;
		case DBVT_WCHAR:
			ApplyFingerprintImage(hContact, cws->value.pwszVal);
			break;
		default:
			ApplyFingerprintImage(hContact, nullptr);
		}
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// OnSrmmWindowEvent
// Monitors SRMM window's creation to draw a statusbar icon

static int OnSrmmWindowEvent(WPARAM uType, LPARAM lParam)
{
	if (!g_plugin.getByte("StatusBarIcon", 1))
		return 0;

	auto *pDlg = (CSrmmBaseDialog *)lParam;

	if (uType == MSG_WINDOW_EVT_OPEN) {
		ptrW ptszMirVer;
		char *szProto = Proto_GetBaseAccountName(pDlg->m_hContact);
		if (szProto != nullptr)
			ptszMirVer = db_get_wsa(pDlg->m_hContact, szProto, "MirVer");
		SetSrmmIcon(pDlg->m_hContact, ptszMirVer);
		arMonitoredWindows.insert((HANDLE)pDlg->m_hContact);
	}
	else if (uType == MSG_WINDOW_EVT_CLOSE)
		arMonitoredWindows.remove((HANDLE)pDlg->m_hContact);

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// OnModulesLoaded
// Hook necessary events here

static int OnExtraIconClick(WPARAM hContact, LPARAM, LPARAM)
{
	CallService(MS_USERINFO_SHOWDIALOG, hContact, NULL);
	return 0;
}

int OnModulesLoaded(WPARAM, LPARAM)
{
	g_LPCodePage = Langpack_GetDefaultCodePage();

	// Hook necessary events
	HookEvent(ME_SKIN_ICONSCHANGED, OnIconsChanged);
	HookEvent(ME_MSG_WINDOWEVENT, OnSrmmWindowEvent);

	HookEvent(ME_MC_DEFAULTTCHANGED, OnExtraImageApply);

	PathToAbsoluteW(DEFAULT_SKIN_FOLDER, g_szSkinLib);

	RegisterIcons();

	hExtraIcon = ExtraIcon_RegisterCallback("Client", LPGEN("Fingerprint"), "client_Miranda_unknown",
		OnExtraIconListRebuild, OnExtraImageApply, OnExtraIconClick);

	if (g_plugin.getByte("StatusBarIcon", 1)) {
		StatusIconData sid = {};
		sid.szModule = MODULENAME;
		sid.flags = MBF_HIDDEN;
		sid.dwId = 1;
		Srmm_AddIcon(&sid, &g_plugin);
	}

	return 0;
}

void InitFingerModule()
{
	HookEvent(ME_SYSTEM_MODULESLOADED, OnModulesLoaded);
	HookEvent(ME_OPT_INITIALISE, OnOptInitialise);
	HookEvent(ME_DB_CONTACT_SETTINGCHANGED, OnContactSettingChanged);

	CreateServiceFunction("Fingerprint/SameClients", ServiceSameClients);
	CreateServiceFunction("Fingerprint/GetClientDescr", ServiceGetClientDescr);
	CreateServiceFunction("Fingerprint/GetClientIcon", ServiceGetClientIcon);
}
