/*
IRC plugin for Miranda IM

Copyright (C) 2003-05 Jurgen Persson
Copyright (C) 2007-09 George Hazan

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "stdafx.h"
#include "version.h"

#include <m_genmenu.h>

static volatile LONG g_msgid = 1;

static int CompareSessions(const CDccSession* p1, const CDccSession* p2)
{
	return INT_PTR(p1->di->hContact) - INT_PTR(p2->di->hContact);
}

CIrcProto::CIrcProto(const char* szModuleName, const wchar_t* tszUserName) :
	PROTO<CIrcProto>(szModuleName, tszUserName),
	m_dcc_chats(10, CompareSessions),
	m_dcc_xfers(10, CompareSessions),
	m_ignoreItems(10),
	vUserhostReasons(10),
	vWhoInProgress(10)
{
	CreateProtoService(PS_GETMYAWAYMSG, &CIrcProto::GetMyAwayMsg);

	CreateProtoService(PS_JOINCHAT, &CIrcProto::OnJoinChat);
	CreateProtoService(PS_LEAVECHAT, &CIrcProto::OnLeaveChat);

	CreateProtoService(IRC_JOINCHANNEL, &CIrcProto::OnJoinMenuCommand);
	CreateProtoService(IRC_CHANGENICK, &CIrcProto::OnChangeNickMenuCommand);
	CreateProtoService(IRC_SHOWLIST, &CIrcProto::OnShowListMenuCommand);
	CreateProtoService(IRC_SHOWSERVER, &CIrcProto::OnShowServerMenuCommand);
	CreateProtoService(IRC_UM_CHANSETTINGS, &CIrcProto::OnMenuChanSettings);
	CreateProtoService(IRC_UM_WHOIS, &CIrcProto::OnMenuWhois);
	CreateProtoService(IRC_UM_DISCONNECT, &CIrcProto::OnMenuDisconnect);
	CreateProtoService(IRC_UM_IGNORE, &CIrcProto::OnMenuIgnore);

	CreateProtoService("/DblClickEvent", &CIrcProto::OnDoubleclicked);

	HookProtoEvent(ME_DB_CONTACT_SETTINGCHANGED, &CIrcProto::OnDbSettingChanged);
	HookProtoEvent(ME_OPT_INITIALISE, &CIrcProto::OnInitOptionsPages);

	codepage = CP_ACP;

	InitPrefs();
	CheckUpdate();

	CList_SetAllOffline(true);

	// group chats
	GCREGISTER gcr = {};
	gcr.dwFlags = GC_CHANMGR;
	gcr.ptszDispName = m_tszUserName;
	gcr.pszModule = m_szModuleName;
	Chat_Register(&gcr);

	HookProtoEvent(ME_GC_EVENT, &CIrcProto::GCEventHook);
	HookProtoEvent(ME_GC_BUILDMENU, &CIrcProto::GCMenuHook);

	IRC_MAP_ENTRY("PING", OnIrc_PING)
	IRC_MAP_ENTRY("JOIN", OnIrc_JOIN)
	IRC_MAP_ENTRY("QUIT", OnIrc_QUIT)
	IRC_MAP_ENTRY("KICK", OnIrc_KICK)
	IRC_MAP_ENTRY("MODE", OnIrc_MODE)
	IRC_MAP_ENTRY("NICK", OnIrc_NICK)
	IRC_MAP_ENTRY("PART", OnIrc_PART)
	IRC_MAP_ENTRY("PRIVMSG", OnIrc_PRIVMSG)
	IRC_MAP_ENTRY("TOPIC", OnIrc_TOPIC)
	IRC_MAP_ENTRY("NOTICE", OnIrc_NOTICE)
	IRC_MAP_ENTRY("PING", OnIrc_PINGPONG)
	IRC_MAP_ENTRY("PONG", OnIrc_PINGPONG)
	IRC_MAP_ENTRY("CAP", OnIrc_CAP)
	IRC_MAP_ENTRY("AUTHENTICATE", OnIrc_AUTHENTICATE)
	IRC_MAP_ENTRY("INVITE", OnIrc_INVITE)
	IRC_MAP_ENTRY("ERROR", OnIrc_ERROR)
	IRC_MAP_ENTRY("001", OnIrc_WELCOME)
	IRC_MAP_ENTRY("002", OnIrc_YOURHOST)
	IRC_MAP_ENTRY("005", OnIrc_SUPPORT)
	IRC_MAP_ENTRY("223", OnIrc_WHOIS_OTHER)			//CodePage info
	IRC_MAP_ENTRY("254", OnIrc_NOOFCHANNELS)
	IRC_MAP_ENTRY("263", OnIrc_TRYAGAIN)
	IRC_MAP_ENTRY("264", OnIrc_WHOIS_OTHER)			//Encryption info (SSL connect)
	IRC_MAP_ENTRY("301", OnIrc_WHOIS_AWAY)
	IRC_MAP_ENTRY("302", OnIrc_USERHOST_REPLY)
	IRC_MAP_ENTRY("305", OnIrc_BACKFROMAWAY)
	IRC_MAP_ENTRY("306", OnIrc_SETAWAY)
	IRC_MAP_ENTRY("307", OnIrc_WHOIS_AUTH)
	IRC_MAP_ENTRY("310", OnIrc_WHOIS_OTHER)
	IRC_MAP_ENTRY("311", OnIrc_WHOIS_NAME)
	IRC_MAP_ENTRY("312", OnIrc_WHOIS_SERVER)
	IRC_MAP_ENTRY("313", OnIrc_WHOIS_OTHER)
	IRC_MAP_ENTRY("315", OnIrc_WHO_END)
	IRC_MAP_ENTRY("317", OnIrc_WHOIS_IDLE)
	IRC_MAP_ENTRY("318", OnIrc_WHOIS_END)
	IRC_MAP_ENTRY("319", OnIrc_WHOIS_CHANNELS)
	IRC_MAP_ENTRY("320", OnIrc_WHOIS_AUTH)
	IRC_MAP_ENTRY("321", OnIrc_LISTSTART)
	IRC_MAP_ENTRY("322", OnIrc_LIST)
	IRC_MAP_ENTRY("323", OnIrc_LISTEND)
	IRC_MAP_ENTRY("324", OnIrc_MODEQUERY)
	IRC_MAP_ENTRY("330", OnIrc_WHOIS_AUTH)
	IRC_MAP_ENTRY("332", OnIrc_INITIALTOPIC)
	IRC_MAP_ENTRY("333", OnIrc_INITIALTOPICNAME)
	IRC_MAP_ENTRY("352", OnIrc_WHO_REPLY)
	IRC_MAP_ENTRY("353", OnIrc_NAMES)
	IRC_MAP_ENTRY("366", OnIrc_ENDNAMES)
	IRC_MAP_ENTRY("367", OnIrc_BANLIST)
	IRC_MAP_ENTRY("368", OnIrc_BANLISTEND)
	IRC_MAP_ENTRY("346", OnIrc_BANLIST)
	IRC_MAP_ENTRY("347", OnIrc_BANLISTEND)
	IRC_MAP_ENTRY("348", OnIrc_BANLIST)
	IRC_MAP_ENTRY("349", OnIrc_BANLISTEND)
	IRC_MAP_ENTRY("371", OnIrc_WHOIS_OTHER)
	IRC_MAP_ENTRY("376", OnIrc_ENDMOTD)
	IRC_MAP_ENTRY("401", OnIrc_WHOIS_NO_USER)
	IRC_MAP_ENTRY("403", OnIrc_JOINERROR)
	IRC_MAP_ENTRY("416", OnIrc_WHOTOOLONG)
	IRC_MAP_ENTRY("421", OnIrc_UNKNOWN)
	IRC_MAP_ENTRY("422", OnIrc_ENDMOTD)
	IRC_MAP_ENTRY("433", OnIrc_NICK_ERR)
	IRC_MAP_ENTRY("471", OnIrc_JOINERROR)
	IRC_MAP_ENTRY("473", OnIrc_JOINERROR)
	IRC_MAP_ENTRY("474", OnIrc_JOINERROR)
	IRC_MAP_ENTRY("475", OnIrc_JOINERROR)
	IRC_MAP_ENTRY("671", OnIrc_WHOIS_OTHER)			//Encryption info (SSL connect)
	IRC_MAP_ENTRY("903", OnIrc_AUTH_OK)
	IRC_MAP_ENTRY("904", OnIrc_AUTH_FAIL)
	IRC_MAP_ENTRY("905", OnIrc_AUTH_FAIL)
	IRC_MAP_ENTRY("906", OnIrc_AUTH_FAIL)
}

CIrcProto::~CIrcProto()
{
	if (con) {
		Netlib_CloseHandle(con);
		con = nullptr;
	}

	Netlib_CloseHandle(hNetlibDCC); hNetlibDCC = nullptr;

	mir_free(m_alias);

	KillChatTimer(OnlineNotifTimer);
	KillChatTimer(OnlineNotifTimer3);
}

////////////////////////////////////////////////////////////////////////////////////////
// OnModulesLoaded - performs hook registration

void CIrcProto::OnModulesLoaded()
{
	wchar_t name[128];
	mir_snwprintf(name, TranslateT("%s (server)"), m_tszUserName);

	db_unset(0, m_szModuleName, "JTemp");

	NETLIBUSER nlu = {};
	nlu.flags = NUF_OUTGOING | NUF_INCOMING | NUF_HTTPCONNS | NUF_UNICODE;
	nlu.szSettingsModule = m_szModuleName;
	nlu.szDescriptiveName.w = name;
	m_hNetlibUser = Netlib_RegisterUser(&nlu);

	nlu.flags = NUF_OUTGOING | NUF_INCOMING | NUF_HTTPCONNS | NUF_UNICODE;
	char szTemp2[256];
	mir_snprintf(szTemp2, "%s DCC", m_szModuleName);
	nlu.szSettingsModule = szTemp2;
	mir_snwprintf(name, TranslateT("%s (client-to-client)"), m_tszUserName);
	nlu.szDescriptiveName.w = name;
	hNetlibDCC = Netlib_RegisterUser(&nlu);

	m_pServer = Chat_NewSession(GCW_SERVER, m_szModuleName, SERVERWINDOW, m_tszUserName);

	if (m_useServer && !m_hideServerWindow)
		Chat_Control(SERVERWINDOW, WINDOW_VISIBLE);
	else
		Chat_Control(SERVERWINDOW, WINDOW_HIDDEN);

	ptrA szNetwork(getStringA("Network"));
	if (szNetwork) {
		CMStringA szSetting(FORMAT, "PERFORM:%s", szNetwork.get());
		szSetting.MakeUpper();

		CMStringW wszValue(getMStringW(szSetting));
		if (!wszValue.IsEmpty()) {
			setWString("PERFORM:EVENT: CONNECT", wszValue);
			delSetting(szSetting);
		}
		delSetting("Network");
	}

	InitIgnore();

	HookProtoEvent(ME_USERINFO_INITIALISE, &CIrcProto::OnInitUserInfo);
	
	if (m_nick[0]) {
		wchar_t szBuf[40];
		if (mir_wstrlen(m_alternativeNick) == 0) {
			mir_snwprintf(szBuf, L"%s%u", m_nick, rand() % 9999);
			setWString("AlernativeNick", szBuf);
			mir_wstrncpy(m_alternativeNick, szBuf, 30);
		}

		if (mir_wstrlen(m_name) == 0) {
			mir_snwprintf(szBuf, L"Miranda%u", rand() % 9999);
			setWString("Name", szBuf);
			mir_wstrncpy(m_name, szBuf, 200);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// AddToList - adds a contact to the contact list

MCONTACT CIrcProto::AddToList(int, PROTOSEARCHRESULT* psr)
{
	if (m_iStatus == ID_STATUS_OFFLINE || m_iStatus == ID_STATUS_CONNECTING)
		return 0;

	wchar_t *id = psr->id.w ? psr->id.w : psr->nick.w;
	id = psr->flags & PSR_UNICODE ? mir_wstrdup((wchar_t*)id) : mir_a2u((char*)id);

	CONTACT user = { id, nullptr, nullptr, true, false, false };
	MCONTACT hContact = CList_AddContact(&user, true, false);

	if (hContact) {
		DBVARIANT dbv1;
		CMStringW S = L"S";

		if (getByte(hContact, "AdvancedMode", 0) == 0) {
			S += user.name;
			DoUserhostWithReason(1, S, true, user.name);
		}
		else {
			if (!getWString(hContact, "UWildcard", &dbv1)) {
				S += dbv1.pwszVal;
				DoUserhostWithReason(2, S, true, dbv1.pwszVal);
				db_free(&dbv1);
			}
			else {
				S += user.name;
				DoUserhostWithReason(2, S, true, user.name);
			}
		}
		if (getByte("MirVerAutoRequest", 1))
			PostIrcMessage(L"/PRIVMSG %s \001VERSION\001", user.name);
	}

	mir_free(id);
	return hContact;
}

////////////////////////////////////////////////////////////////////////////////////////
// AuthAllow - processes the successful authorization

int CIrcProto::Authorize(MEVENT)
{
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// AuthDeny - handles the unsuccessful authorization

int CIrcProto::AuthDeny(MEVENT, const wchar_t*)
{
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// FileAllow - starts a file transfer

HANDLE CIrcProto::FileAllow(MCONTACT, HANDLE hTransfer, const wchar_t* szPath)
{
	DCCINFO* di = (DCCINFO*)hTransfer;

	if (!IsConnected()) {
		delete di;
		return (HANDLE)szPath;
	}

	di->sPath = szPath;
	di->sFileAndPath = di->sPath + di->sFile;

	CDccSession *dcc = new CDccSession(this, di);
	AddDCCSession(di, dcc);
	dcc->Connect();
	return di;
}

////////////////////////////////////////////////////////////////////////////////////////
// FileCancel - cancels a file transfer

int CIrcProto::FileCancel(MCONTACT, HANDLE hTransfer)
{
	DCCINFO* di = (DCCINFO*)hTransfer;

	CDccSession *dcc = FindDCCSession(di);
	if (dcc) {
		InterlockedExchange(&dcc->dwWhatNeedsDoing, (long)FILERESUME_CANCEL);
		SetEvent(dcc->hEvent);
		dcc->Disconnect();
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// FileDeny - denies a file transfer

int CIrcProto::FileDeny(MCONTACT, HANDLE hTransfer, const wchar_t*)
{
	DCCINFO* di = (DCCINFO*)hTransfer;
	delete di;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// FileResume - processes file renaming etc

int CIrcProto::FileResume(HANDLE hTransfer, int action, const wchar_t *szFilename)
{
	DCCINFO* di = (DCCINFO*)hTransfer;

	long i = action;
	CDccSession *dcc = FindDCCSession(di);
	if (dcc) {
		InterlockedExchange(&dcc->dwWhatNeedsDoing, i);
		if (action == FILERESUME_RENAME) {
			wchar_t* szTemp = wcsdup(szFilename);
			InterlockedExchangePointer((PVOID*)&dcc->NewFileName, szTemp);
		}

		if (action == FILERESUME_RESUME) {
			unsigned __int64 dwPos = 0;

			struct _stati64 statbuf;
			if (_wstat64(di->sFileAndPath, &statbuf) == 0 && (statbuf.st_mode & _S_IFDIR) == 0)
				dwPos = statbuf.st_size;

			CMStringW sFileWithQuotes = di->sFile;

			// if spaces in the filename surround with quotes
			if (sFileWithQuotes.Find(' ', 0) != -1) {
				sFileWithQuotes.Insert(0, L"\"");
				sFileWithQuotes.Insert(sFileWithQuotes.GetLength(), L"\"");
			}

			if (di->bReverse)
				PostIrcMessage(L"/PRIVMSG %s \001DCC RESUME %s 0 %I64u %s\001", di->sContactName.c_str(), sFileWithQuotes.c_str(), dwPos, dcc->di->sToken.c_str());
			else
				PostIrcMessage(L"/PRIVMSG %s \001DCC RESUME %s %u %I64u\001", di->sContactName.c_str(), sFileWithQuotes.c_str(), di->iPort, dwPos);

			return 0;
		}

		SetEvent(dcc->hEvent);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// GetCaps - return protocol capabilities bits

INT_PTR CIrcProto::GetCaps(int type, MCONTACT)
{
	switch (type) {
	case PFLAGNUM_1:
		return PF1_BASICSEARCH | PF1_MODEMSG | PF1_FILE | PF1_CHAT | PF1_CANRENAMEFILE | PF1_PEER2PEER | PF1_IM;

	case PFLAGNUM_2:
		return PF2_ONLINE | PF2_SHORTAWAY;

	case PFLAGNUM_3:
		return PF2_SHORTAWAY;

	case PFLAGNUM_4:
		return PF4_NOAUTHDENYREASON | PF4_NOCUSTOMAUTH | PF4_SERVERFORMATTING;

	case PFLAG_UNIQUEIDTEXT:
		return (INT_PTR)TranslateT("Nickname");

	case PFLAG_MAXLENOFMESSAGE:
		return 400;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// SearchBasic - searches the contact by JID

struct AckBasicSearchParam
{
	wchar_t buf[50];
};

void __cdecl CIrcProto::AckBasicSearch(void *arg)
{
	Thread_SetName("IRC: AckBasicSearch");

	AckBasicSearchParam *param = (AckBasicSearchParam*)arg;
	PROTOSEARCHRESULT psr = { sizeof(psr) };
	psr.flags = PSR_UNICODE;
	psr.id.w = psr.nick.w = param->buf;
	ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_DATA, (HANDLE)1, (LPARAM)& psr);
	ProtoBroadcastAck(NULL, ACKTYPE_SEARCH, ACKRESULT_SUCCESS, (HANDLE)1, 0);
	delete param;
}

HANDLE CIrcProto::SearchBasic(const wchar_t* szId)
{
	if (szId) {
		if (m_iStatus != ID_STATUS_OFFLINE && m_iStatus != ID_STATUS_CONNECTING && !IsChannel(szId)) {
			AckBasicSearchParam* param = new AckBasicSearchParam;
			mir_wstrncpy(param->buf, szId, 50);
			ForkThread(&CIrcProto::AckBasicSearch, param);
			return (HANDLE)1;
		}
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////
// SendFile - sends a file

HANDLE CIrcProto::SendFile(MCONTACT hContact, const wchar_t*, wchar_t** ppszFiles)
{
	DCCINFO* dci = nullptr;
	int iPort = 0;
	int index = 0;
	unsigned __int64 size = 0;

	// do not send to channels :-P
	if (isChatRoom(hContact))
		return nullptr;

	// stop if it is an active type filetransfer and the user's IP is not known
	unsigned long ulAdr = 0;
	if (m_manualHost)
		ulAdr = ConvertIPToInteger(m_mySpecifiedHostIP);
	else
		ulAdr = ConvertIPToInteger(m_IPFromServer ? m_myHost : m_myLocalHost);

	if (!m_DCCPassive && !ulAdr) {
		DoEvent(GC_EVENT_INFORMATION, nullptr, m_info.sNick, TranslateT("DCC ERROR: Unable to automatically resolve external IP"), nullptr, nullptr, NULL, true, false);
		return nullptr;
	}

	if (ppszFiles[index]) {

		//get file size
		while (ppszFiles[index]) {
			struct _stati64 statbuf;
			if (_wstat64(ppszFiles[index], &statbuf) == 0 && (statbuf.st_mode & _S_IFDIR) == 0) {
				size = statbuf.st_size;
				break;
			}
			index++;
		}

		if (size == 0) {
			DoEvent(GC_EVENT_INFORMATION, nullptr, m_info.sNick, TranslateT("DCC ERROR: No valid files specified"), nullptr, nullptr, NULL, true, false);
			return nullptr;
		}

		DBVARIANT dbv;
		if (!getWString(hContact, "Nick", &dbv)) {
			// set up a basic DCCINFO struct and pass it to a DCC object
			dci = new DCCINFO;
			dci->sFileAndPath = ppszFiles[index];

			int i = dci->sFileAndPath.ReverseFind('\\');
			if (i != -1) {
				dci->sPath = dci->sFileAndPath.Mid(0, i + 1);
				dci->sFile = dci->sFileAndPath.Mid(i + 1);
			}

			CMStringW sFileWithQuotes = dci->sFile;

			// if spaces in the filename surround witrh quotes
			if (sFileWithQuotes.Find(' ', 0) != -1) {
				sFileWithQuotes.Insert(0, L"\"");
				sFileWithQuotes.Insert(sFileWithQuotes.GetLength(), L"\"");
			}

			dci->hContact = hContact;
			dci->sContactName = dbv.pwszVal;
			dci->iType = DCC_SEND;
			dci->bReverse = m_DCCPassive ? true : false;
			dci->bSender = true;
			dci->dwSize = size;

			// create new dcc object
			CDccSession *dcc = new CDccSession(this, dci);

			// keep track of all objects created
			AddDCCSession(dci, dcc);

			// need to make sure that %'s are doubled to avoid having chat interpret as color codes
			CMStringW sFileCorrect = dci->sFile;

			// is it an reverse filetransfer (receiver acts as server)
			if (dci->bReverse) {
				wchar_t szTemp[256];
				PostIrcMessage(L"/CTCP %s DCC SEND %s 200 0 %I64u %u",
					dci->sContactName.c_str(), sFileWithQuotes.c_str(), dci->dwSize, dcc->iToken);

				mir_snwprintf(szTemp,
					TranslateT("DCC reversed file transfer request sent to %s [%s]"),
					dci->sContactName.c_str(), sFileCorrect.c_str());
				DoEvent(GC_EVENT_INFORMATION, nullptr, m_info.sNick, szTemp, nullptr, nullptr, NULL, true, false);

				if (m_sendNotice) {
					mir_snwprintf(szTemp,
						L"/NOTICE %s I am sending the file '\002%s\002' (%I64u kB) to you, please accept it. [Reverse transfer]",
						dci->sContactName.c_str(), sFileCorrect.c_str(), dci->dwSize / 1024);
					PostIrcMessage(szTemp);
				}
			}
			else { // ... normal filetransfer.
				iPort = dcc->Connect();
				if (iPort) {
					wchar_t szTemp[256];
					PostIrcMessage(L"/CTCP %s DCC SEND %s %u %u %I64u",
						dci->sContactName.c_str(), sFileWithQuotes.c_str(), ulAdr, iPort, dci->dwSize);

					mir_snwprintf(szTemp,
						TranslateT("DCC file transfer request sent to %s [%s]"),
						dci->sContactName.c_str(), sFileCorrect.c_str());
					DoEvent(GC_EVENT_INFORMATION, nullptr, m_info.sNick, szTemp, nullptr, nullptr, NULL, true, false);

					if (m_sendNotice) {
						mir_snwprintf(szTemp,
							L"/NOTICE %s I am sending the file '\002%s\002' (%I64u kB) to you, please accept it. [IP: %s]",
							dci->sContactName.c_str(), sFileCorrect.c_str(), dci->dwSize / 1024, (wchar_t*)_A2T(ConvertIntegerToIP(ulAdr)));
						PostIrcMessage(szTemp);
					}
				}
				else DoEvent(GC_EVENT_INFORMATION, nullptr, m_info.sNick.c_str(),
					TranslateT("DCC ERROR: Unable to bind local port"), nullptr, nullptr, NULL, true, false);
			}

			// fix for sending multiple files
			index++;
			while (ppszFiles[index]) {
				if (_waccess(ppszFiles[index], 0) == 0) {
					PostIrcMessage(L"/DCC SEND %s %S", dci->sContactName.c_str(), ppszFiles[index]);
				}
				index++;
			}

			db_free(&dbv);
		}
	}

	if (dci)
		return dci;
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////
// SendMessage - sends a message

int CIrcProto::SendMsg(MCONTACT hContact, MEVENT, const char* pszSrc)
{
	uint8_t bDcc = getByte(hContact, "DCC", 0);
	uint16_t wStatus = getWord(hContact, "Status", ID_STATUS_OFFLINE);
	if (bDcc && wStatus != ID_STATUS_ONLINE) {
		ProtoBroadcastAsync(hContact, ACKTYPE_MESSAGE, ACKRESULT_FAILED, 0, (LPARAM)TranslateT("The dcc chat connection is not active"));
		return 0;
	}
	
	if (!bDcc && (m_iStatus == ID_STATUS_OFFLINE || m_iStatus == ID_STATUS_CONNECTING)) {
		ProtoBroadcastAsync(hContact, ACKTYPE_MESSAGE, ACKRESULT_FAILED, 0, (LPARAM)TranslateT("The protocol is not online"));
		return 0;
	}

	wchar_t *result;
	mir_utf8decode(NEWSTR_ALLOCA(pszSrc), &result);
	PostIrcMessageWnd(nullptr, hContact, result);
	mir_free(result);

	int seq = InterlockedIncrement(&g_msgid);
	ProtoBroadcastAsync(hContact, ACKTYPE_MESSAGE, ACKRESULT_SUCCESS, (HANDLE)seq);
	return seq;
}

////////////////////////////////////////////////////////////////////////////////////////
// SetStatus - sets the protocol status

int CIrcProto::SetStatus(int iNewStatus)
{
	return SetStatusInternal(iNewStatus, false);
}

int CIrcProto::SetStatusInternal(int iNewStatus, bool bIsInternal)
{
	if (iNewStatus != ID_STATUS_OFFLINE && (!m_nick[0] || !m_userID[0] || !m_name[0])) {
		Clist_TrayNotifyW(m_szModuleName, TranslateT("IRC error"), TranslateT("Connection cannot be established! You have not completed all necessary fields (Nickname, User ID and Full name)."), NIIF_ERROR, 15000);
		return 0;
	}

	if (!bIsInternal)
		m_iDesiredStatus = iNewStatus;

	if ((iNewStatus == ID_STATUS_ONLINE || iNewStatus == ID_STATUS_AWAY || iNewStatus == ID_STATUS_FREECHAT) && !IsConnected()) //go from offline to online
	{
		if (!m_bConnectThreadRunning)
			ConnectToServer();
	}
	else if ((iNewStatus == ID_STATUS_ONLINE || iNewStatus == ID_STATUS_FREECHAT) && IsConnected() && m_iStatus == ID_STATUS_AWAY) //go to online while connected
	{
		m_statusMessage = L"";
		PostIrcMessage(L"/AWAY");
		return 0;
	}
	else if (iNewStatus == ID_STATUS_OFFLINE && IsConnected()) //go from online/away to offline
		DisconnectFromServer();
	else if (iNewStatus == ID_STATUS_OFFLINE && !IsConnected()) //offline to offline
	{
		return 0;
	}
	else if (iNewStatus == ID_STATUS_AWAY && IsConnected()) //go to away while connected
	{
		PostIrcMessage(L"/AWAY %s", m_statusMessage.Mid(0, 450).c_str());
		return 0;
	}
	else if (iNewStatus == ID_STATUS_ONLINE && IsConnected()) //already online
		return 0;
	else
		SetStatusInternal(ID_STATUS_AWAY, true);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// GetAwayMsg - returns a contact's away message

HANDLE CIrcProto::GetAwayMsg(MCONTACT hContact)
{
	WhoisAwayReply = L"";
	DBVARIANT dbv;

	// bypass chat contacts.
	if (!isChatRoom(hContact)) {
		if (hContact && !getWString(hContact, "Nick", &dbv)) {
			int i = getWord(hContact, "Status", ID_STATUS_OFFLINE);
			if (i != ID_STATUS_AWAY) {
				db_free(&dbv);
				return nullptr;
			}
			CMStringW S = L"WHOIS ";
			S += dbv.pwszVal;
			if (IsConnected())
				SendIrcMessage(S, false);
			db_free(&dbv);
		}
	}

	return (HANDLE)1;
}

////////////////////////////////////////////////////////////////////////////////////////
// SetAwayMsg - sets the away status message

int CIrcProto::SetAwayMsg(int status, const wchar_t* msg)
{
	switch (status) {
	case ID_STATUS_ONLINE:     case ID_STATUS_INVISIBLE:   case ID_STATUS_FREECHAT:
	case ID_STATUS_CONNECTING: case ID_STATUS_OFFLINE:
		break;

	default:
		CMStringW newStatus = msg;
		newStatus.Replace(L"\r\n", L" ");
		if (m_statusMessage.IsEmpty() || msg == nullptr || m_statusMessage != newStatus) {
			if (msg == nullptr || *msg == 0)
				m_statusMessage = STR_AWAYMESSAGE;
			else
				m_statusMessage = newStatus;

			if (m_iStatus == ID_STATUS_AWAY)
				PostIrcMessage(L"/AWAY %s", m_statusMessage.Mid(0, 450).c_str());
		}
	}

	return 0;
}
