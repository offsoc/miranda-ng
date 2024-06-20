#ifndef _STEAM_PROTO_H_
#define _STEAM_PROTO_H_

#define STEAM_SEARCH_BYID 1001
#define STEAM_SEARCH_BYNAME 1002

#define STEAM_PROTOCOL_VERSION 65580
#define STEAM_PROTOCOL_MASK 0x80000000

// Global settings for all accounts: hosts' list
#define STEAM_MODULE       "Steam"
#define DBKEY_HOSTS_COUNT  "HostsCount"
#define DBKEY_HOSTS_DATE   "HostsDate"

#define DBKEY_CLIENT_ID    "ClientID"
#define DBKEY_STEAM_ID     "SteamID"
#define DBKEY_ACCOUNT_NAME "Username"
#define DBKEY_MACHINE_ID   "MachineId"

struct SendAuthParam
{
	MCONTACT hContact;
	HANDLE hAuth;
};

struct STEAM_SEARCH_RESULT
{
	PROTOSEARCHRESULT psr;
	const JSONNode *data;
};

enum
{
	CMI_BLOCK,
	CMI_UNBLOCK,
	CMI_JOIN_GAME,
	CMI_MAX   // this item shall be the last one
};

typedef void (CSteamProto::*MsgCallback)(const uint8_t *pBuf, size_t cbLen);
typedef void (CSteamProto::*HttpCallback)(const HttpResponse &, void *);
typedef void (CSteamProto::*JsonCallback)(const JSONNode &, void *);

struct HttpRequest : public MTHttpRequest<CSteamProto>
{
	HttpRequest(int iRequestType, const char *pszUrl);

	MHttpRequest* Get();
};

struct ProtoRequest
{
   ProtoRequest(int64_t _1, MsgCallback _2) :
      id(_1),
      pCallback(_2)
   {}

   int64_t id;
   MsgCallback pCallback;
};

class CSteamProto : public PROTO<CSteamProto>
{
	friend class CSteamGuardDialog;
	friend class CSteamPasswordEditor;
	friend class CSteamOptionsMain;
	friend class CSteamOptionsBlockList;
	friend class PollRequest;
	friend class WebSocket<CSteamProto>;

	ptrW m_password;
	bool m_bTerminated;
	HWND m_hwndGuard;
	time_t m_idleTS;

	int64_t  GetId(const char *pszSetting);
	void     SetId(const char *pszSetting, int64_t id);

   int64_t  GetId(MCONTACT, const char *pszSetting);
   void     SetId(MCONTACT, const char *pszSetting, int64_t id);

	// polling
	CMStringA m_szRefreshToken, m_szAccessToken;
	ULONG hAuthProcess = 1;
	ULONG hMessageProcess = 1;
	mir_cs m_addContactLock;
	mir_cs m_setStatusLock;
	std::map<HANDLE, time_t> m_mpOutMessages;

	// connection
	WebSocket<CSteamProto> *m_ws;

	void __cdecl ServerThread(void *);
	bool ServerThreadStub(const char *szHost);

   mir_cs m_csRequests;
   OBJLIST<ProtoRequest> m_arRequests;

   void ProcessMulti(const uint8_t *buf, size_t cbLen);
   void ProcessMessage(const uint8_t *buf, size_t cbLen);

	void WSSend(EMsg msgType, const ProtobufCppMessage &msg);
	void WSSendHeader(EMsg msgType, const CMsgProtoBufHeader &hdr, const ProtobufCppMessage &msg);
	void WSSendService(const char *pszServiceName, const ProtobufCppMessage &msg, MsgCallback pCallback = 0);

	// requests
	bool SendRequest(HttpRequest *request);
	bool SendRequest(HttpRequest *request, HttpCallback callback, void *param = nullptr);
	bool SendRequest(HttpRequest *request, JsonCallback callback, void *param = nullptr);

	// login
	bool IsOnline();
	bool IsMe(const char *steamId);

	void Login();
	void LoginFailed();
	void Logout();

   void OnAuthorization(const uint8_t *buf, size_t cbLen);
	void OnGotRsaKey(const uint8_t *buf, size_t cbLen);
	void OnLoggedOn(const uint8_t *buf, size_t cbLen);
	void OnPollSession(const uint8_t *buf, size_t cbLen);

	void OnGotCaptcha(const HttpResponse &response, void *arg);

	void OnAuthorizationError(const JSONNode &root);

	void OnGotHosts(const JSONNode &root, void *);

	void DeleteAuthSettings();

	// contacts
	void SetAllContactStatuses(int status);
	void SetContactStatus(MCONTACT hContact, uint16_t status);

	MCONTACT GetContactFromAuthEvent(MEVENT hEvent);

	void UpdateContactDetails(MCONTACT hContact, const JSONNode &data);
	void UpdateContactRelationship(MCONTACT hContact, const JSONNode &data);
	void OnGotAppInfo(const JSONNode &root, void *arg);

	void ContactIsRemoved(MCONTACT hContact);
	void ContactIsFriend(MCONTACT hContact);
	void ContactIsBlocked(MCONTACT hContact);
	void ContactIsUnblocked(MCONTACT hContact);
	void ContactIsAskingAuth(MCONTACT hContact);

	MCONTACT GetContact(const char *steamId);
	MCONTACT AddContact(const char *steamId, const wchar_t *nick = nullptr, bool isTemporary = false);

	void OnGotFriendList(const JSONNode &root, void *);
	void OnGotBlockList(const JSONNode &root, void *);
	void OnGotUserSummaries(const JSONNode &root, void *);
	void OnGotAvatar(const HttpResponse &response, void *arg);

	void OnFriendAdded(const HttpResponse &response, void *arg);
	void OnFriendBlocked(const HttpResponse &response, void *arg);
	void OnFriendUnblocked(const HttpResponse &response, void *arg);
	void OnFriendRemoved(const HttpResponse &response, void *arg);

	void OnAuthRequested(const JSONNode &root, void *arg);

	void OnPendingApproved(const JSONNode &root, void *arg);
	void OnPendingIgnoreded(const JSONNode &root, void *arg);

	void OnSearchResults(const HttpResponse &response, void *arg);
	void OnSearchByNameStarted(const HttpResponse &response, void *arg);

	// messages
	int OnSendMessage(MCONTACT hContact, const char *message);
	void OnMessageSent(const HttpResponse &response, void *arg);
	int __cdecl OnPreCreateMessage(WPARAM, LPARAM lParam);

	// history
	void OnGotConversations(const JSONNode &root, void *arg);
	void OnGotHistoryMessages(const JSONNode &root, void *);

	// menus
	static int hChooserMenu;
	static HGENMENU contactMenuItems[CMI_MAX];

	INT_PTR __cdecl AuthRequestCommand(WPARAM, LPARAM);
	INT_PTR __cdecl AuthRevokeCommand(WPARAM, LPARAM);

	int __cdecl BlockCommand(WPARAM, LPARAM);
	int __cdecl UnblockCommand(WPARAM, LPARAM);
	int __cdecl JoinToGameCommand(WPARAM, LPARAM);

	INT_PTR __cdecl OpenBlockListCommand(WPARAM, LPARAM);

	static int PrebuildContactMenu(WPARAM wParam, LPARAM lParam);
	int OnPrebuildContactMenu(WPARAM wParam, LPARAM);

	void OnInitStatusMenu();

	// avatars
	wchar_t *GetAvatarFilePath(MCONTACT hContact);
	bool GetDbAvatarInfo(PROTO_AVATAR_INFORMATION &pai);
	void CheckAvatarChange(MCONTACT hContact, std::string avatarUrl);

	INT_PTR __cdecl GetAvatarInfo(WPARAM, LPARAM);
	INT_PTR __cdecl GetAvatarCaps(WPARAM, LPARAM);
	INT_PTR __cdecl GetMyAvatar(WPARAM, LPARAM);

	// xstatuses
	INT_PTR  __cdecl OnGetXStatusEx(WPARAM wParam, LPARAM lParam);
	INT_PTR  __cdecl OnGetXStatusIcon(WPARAM wParam, LPARAM lParam);
	INT_PTR  __cdecl OnRequestAdvStatusIconIdx(WPARAM wParam, LPARAM lParam);
	HICON GetXStatusIcon(int status, UINT flags);
	int GetContactXStatus(MCONTACT hContact);

	void __cdecl GetAwayMsgThread(void *arg);

	// events
	int __cdecl OnIdleChanged(WPARAM, LPARAM);
	int __cdecl OnOptionsInit(WPARAM wParam, LPARAM lParam);

	// utils
	static uint16_t SteamToMirandaStatus(PersonaState state);
	static PersonaState MirandaToSteamState(int status);
		
	static void ShowNotification(const wchar_t *message, int flags = 0, MCONTACT hContact = NULL);
	static void ShowNotification(const wchar_t *caption, const wchar_t *message, int flags = 0, MCONTACT hContact = NULL);

	INT_PTR __cdecl OnGetEventTextChatStates(WPARAM wParam, LPARAM lParam);

	// helpers
	inline int IdleSeconds()
	{
		// Based on idle time we report Steam server will mark us as online/away/snooze
		switch (m_iStatus) {
		case ID_STATUS_AWAY:
			return STEAM_API_IDLEOUT_AWAY;
		case ID_STATUS_NA:
			return STEAM_API_IDLEOUT_SNOOZE;
		default:
			return 0;
		}

		// ... or we can report real idle info
		// return m_idleTS ? time(0) - m_idleTS : 0;
	}

	inline const char *AccountIdToSteamId(long long accountId)
	{
		static char steamId[20];
		mir_snprintf(steamId, "%llu", accountId + 76561197960265728ll);
		return steamId;
	}

	inline const char *SteamIdToAccountId(long long steamId)
	{
		static char accountId[10];
		mir_snprintf(accountId, "%llu", steamId - 76561197960265728ll);
		return accountId;
	}

public:
	// constructor
	CSteamProto(const char *protoName, const wchar_t *userName);
	~CSteamProto();

	// options
	CMOption<wchar_t*> m_wszGroupName;   // default group for this account's contacts
	CMOption<wchar_t*> m_wszDeviceName;  // how do you see this account in the Device List

	// PROTO_INTERFACE
	MCONTACT AddToList(int flags, PROTOSEARCHRESULT *psr) override;
	MCONTACT AddToListByEvent(int flags, int iContact, MEVENT hDbEvent) override;

	int      Authorize(MEVENT hDbEvent) override;
	int      AuthRecv(MCONTACT, DB::EventInfo &dbei) override;
	int      AuthDeny(MEVENT hDbEvent, const wchar_t *szReason) override;
	int      AuthRequest(MCONTACT hContact, const wchar_t *szMessage) override;

	INT_PTR  GetCaps(int type, MCONTACT hContact = NULL) override;
	HANDLE   GetAwayMsg(MCONTACT hContact) override;

	HANDLE   SearchBasic(const wchar_t *id) override;
	HANDLE   SearchByName(const wchar_t *nick, const wchar_t *firstName, const wchar_t *lastName) override;

	int      SendMsg(MCONTACT hContact, MEVENT hReplyEvent, const char *msg) override;

	int      SetStatus(int iNewStatus) override;

	int      UserIsTyping(MCONTACT hContact, int type) override;

	bool     OnContactDeleted(MCONTACT, uint32_t flags) override;
	MWindow  OnCreateAccMgrUI(MWindow) override;
	void     OnModulesLoaded() override;

	// menus
	static void InitMenus();
};

struct CMPlugin : public ACCPROTOPLUGIN<CSteamProto>
{
	CMPlugin();

	int Load() override;
};

int OnReloadIcons(WPARAM wParam, LPARAM lParam);
void SetContactExtraIcon(MCONTACT hContact, int status);

MBinBuffer RsaEncrypt(const char *pszModulus, const char *exponent, const char *data);

#endif //_STEAM_PROTO_H_
