/*
Copyright (c) 2015-24 Miranda NG team (https://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

/* HISTORY SYNC */

void CSkypeProto::OnGetServerHistory(MHttpResponse *response, AsyncHttpRequest *pRequest)
{
	JsonReply reply(response);
	if (reply.error())
		return;

	auto &root = reply.data();
	const JSONNode &metadata = root["_metadata"];

	int totalCount = metadata["totalCount"].as_int();
	std::string syncState = metadata["syncState"].as_string();

	bool markAllAsUnread = getBool("MarkMesUnread", true);
	bool bUseLocalTime = !bUseServerTime && pRequest->pUserInfo != 0;
	uint32_t lastMsgTime = 0;
	time_t iLocalTime = time(0);

	auto &conv = root["messages"];
	for (auto it = conv.rbegin(); it != conv.rend(); ++it) {
		auto &message = *it;
		CMStringA szMessageId = message["clientmessageid"] ? message["clientmessageid"].as_string().c_str() : message["skypeeditedid"].as_string().c_str();

		int iUserType;
		CMStringA szChatId = UrlToSkypeId(message["conversationLink"].as_mstring(), &iUserType);
		CMStringA szFrom = UrlToSkypeId(message["from"].as_mstring());

		MCONTACT hContact = FindContact(szChatId);
		std::string messageType = message["messagetype"].as_string();
		int emoteOffset = message["skypeemoteoffset"].as_int();

		time_t timestamp = IsoToUnixTime(message["composetime"].as_string());
		if (timestamp > getDword(hContact, "LastMsgTime", 0))
			setDword(hContact, "LastMsgTime", timestamp);

		CMStringW wszContent = message["content"].as_mstring();
		T2Utf szMsg(wszContent);
		if (messageType == "RichText/Contacts") {
			ProcessContactRecv(hContact, timestamp, szMsg, szMessageId);
			return;
		}

		uint32_t id = message["id"].as_int();
		if (id > lastMsgTime)
			lastMsgTime = id;

		if (bUseLocalTime)
			timestamp = iLocalTime;

		DB::EventInfo dbei(db_event_getById(m_szModuleName, szMessageId));

		dbei.flags = DBEF_UTF;
		if (!markAllAsUnread)
			dbei.flags |= DBEF_READ;
		if (IsMe(szFrom))
			dbei.flags |= DBEF_SENT;

		if (messageType == "Text" || messageType == "RichText") {
			CMStringW szMessage(messageType == "RichText" ? RemoveHtml(wszContent) : wszContent);
			dbei.eventType = (emoteOffset == 0) ? EVENTTYPE_MESSAGE : SKYPE_DB_EVENT_TYPE_ACTION;
		}
		else if (messageType == "Event/Call") {
			dbei.eventType = SKYPE_DB_EVENT_TYPE_CALL_INFO;
		}
		else if (messageType == "RichText/Files") {
			dbei.eventType = SKYPE_DB_EVENT_TYPE_FILETRANSFER_INFO;
		}
		else if (messageType == "RichText/UriObject") {
			dbei.eventType = SKYPE_DB_EVENT_TYPE_URIOBJ;
		}
		else if (messageType == "RichText/Media_Album") {
			// do nothing
		}
		else {
			dbei.eventType = SKYPE_DB_EVENT_TYPE_UNKNOWN;
		}

		dbei.szModule = m_szModuleName;
		dbei.timestamp = timestamp;
		dbei.cbBlob = (uint32_t)mir_strlen(szMsg);
		dbei.pBlob = szMsg;
		dbei.szId = szMessageId;
		if (iUserType == 19)
			dbei.szUserId = szFrom;

		if (dbei) {
			db_event_edit(dbei.getEvent(), &dbei, true);
			dbei.pBlob = nullptr;
		}
		else db_event_add(hContact, &dbei);
	}

	if (totalCount >= 99 || conv.size() >= 99) {
		CMStringA szUrl(pRequest->m_szUrl);
		int i1 = szUrl.Find("startTime=");
		int i2 = szUrl.Find("&", i1);
		if (i1 != -1 && i2 != -1) {
			i1 += 10;
			szUrl.Delete(i1, i2 - i1);

			char buf[100];
			itoa(lastMsgTime, buf, sizeof(buf));
			szUrl.Insert(i1, buf);

			PushRequest(new GetHistoryRequest(szUrl, pRequest->pUserInfo));
		}
	}
}

void CSkypeProto::ReadHistoryRest(const char *szUrl)
{
	auto *p = strstr(szUrl, g_plugin.szDefaultServer);
	if (p)
		PushRequest(new SyncHistoryFirstRequest(p+ g_plugin.szDefaultServer.GetLength()+3));
}

INT_PTR CSkypeProto::SvcLoadHistory(WPARAM hContact, LPARAM)
{
	PushRequest(new GetHistoryRequest(getId(hContact), 100, 0, false));
	return 0;
}

void CSkypeProto::OnSyncHistory(MHttpResponse *response, AsyncHttpRequest*)
{
	JsonReply reply(response);
	if (reply.error())
		return;

	auto &root = reply.data();
	const JSONNode &metadata = root["_metadata"];
	const JSONNode &conversations = root["conversations"].as_array();

	int totalCount = metadata["totalCount"].as_int();
	std::string syncState = metadata["syncState"].as_string();

	if (totalCount >= 99 || conversations.size() >= 99)
		ReadHistoryRest(syncState.c_str());

	for (auto &conversation : conversations) {
		const JSONNode &lastMessage = conversation["lastMessage"];
		if (!lastMessage)
			continue;

		int iUserType;
		std::string strConversationLink = lastMessage["conversationLink"].as_string();
		CMStringA szSkypename = UrlToSkypeId(strConversationLink.c_str(), &iUserType);
		if (iUserType == 8 || iUserType == 2) {
			time_t composeTime(IsoToUnixTime(lastMessage["composetime"].as_string()));

			MCONTACT hContact = FindContact(szSkypename);
			if (hContact != NULL) {
				uint32_t lastMsgTime = getDword(hContact, "LastMsgTime", 0);
				if (lastMsgTime && lastMsgTime < composeTime)
					PushRequest(new GetHistoryRequest(szSkypename, 100, lastMsgTime, true));
			}
		}
	}

	m_bHistorySynced = true;
}

//////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CSkypeProto::SvcEmptyHistory(WPARAM hContact, LPARAM flags)
{
	if (flags & CDF_DEL_HISTORY) {
		PushRequest(new EmptyHistoryRequest(getId(hContact)));
	}
	return 0;
}
