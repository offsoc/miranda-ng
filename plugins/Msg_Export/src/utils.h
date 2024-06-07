
//This file is part of Msg_Export a Miranda IM plugin
//Copyright (C)2002 Kennet Nielsen ( http://sourceforge.net/projects/msg-export/ )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#ifndef MSG_EXP_UTILS_H
#define MSG_EXP_UTILS_H

enum ENDialogAction
{
	eDAPromptUser,
	eDAAutomatic,
	eDANothing
};

extern ENDialogAction g_enRenameAction;
extern ENDialogAction g_enDeleteAction;

extern int nMaxLineWidth;
extern wstring g_sExportDir;
extern wstring g_sDefaultFile;
extern wstring g_sTimeFormat;
extern map<wstring, string::size_type, less<wstring> > clFileTo1ColWidth;

extern bool g_bUseJson;

extern DATABASELINK *g_pDriver;

void LogLastError(const wchar_t *pszError);
void DisplayErrorDialog(const wchar_t *pszError, wstring &sFilePath);

bool bIsExportEnabled(MCONTACT hContact);
HANDLE openCreateFile(const wstring &sFilePath);
bool bExportEvent(MCONTACT hContact, MEVENT hDbEvent, HANDLE hFile, const wstring &sFilePath, bool bAppendOnly);

int nExportEvent(WPARAM wparam, LPARAM lparam);
int nContactDeleted(WPARAM wparam, LPARAM lparam);

wchar_t* GetMyOwnNick(MCONTACT hContact);

wstring __inline _DBGetStringW(MCONTACT hContact, const char *szModule, const char *szSetting, const wchar_t *pszError);

void UpdateFileToColWidth();

wstring GetFilePathFromUser(MCONTACT hContact);

void ReplaceDefines(MCONTACT hContact, wstring &sTarget);
void ReplaceTimeVariables(wstring &sRet);

bool bIsUtf8Header(uint8_t *pucByteOrder);

#endif
