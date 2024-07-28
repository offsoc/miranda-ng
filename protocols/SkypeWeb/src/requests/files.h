#pragma once

struct ASMObjectCreateRequest : public AsyncHttpRequest
{
	ASMObjectCreateRequest(CSkypeProto *ppro, CFileUploadParam *fup) :
		AsyncHttpRequest(REQUEST_POST, HOST_OTHER, "https://api.asm.skype.com/v1/objects", &CSkypeProto::OnASMObjectCreated)
	{
		flags &= (~NLHRF_DUMPASTEXT);
		pUserInfo = fup;
		
		AddHeader("Authorization", CMStringA(FORMAT, "skype_token %s", ppro->m_szApiToken.get()));
		AddHeader("Content-Type", "application/json");
		AddHeader("X-Client-Version", "0/0.0.0.0");

		CMStringA szContact(ppro->getId(fup->hContact));
		T2Utf uszFileName(fup->tszFileName);
		const char *szFileName = strrchr(uszFileName.get() + 1, '\\');

		JSONNode node, jPermissions, jPermission(JSON_ARRAY);
		jPermissions.set_name("permissions");
		jPermission.set_name(szContact.c_str());
		jPermission << CHAR_PARAM("", "read");
		jPermissions << jPermission;
		node << CHAR_PARAM("type", "sharing/file") << CHAR_PARAM("filename", szFileName) << jPermissions;
		m_szParam = node.write().c_str();
	}
};

struct ASMObjectUploadRequest : public AsyncHttpRequest
{
	ASMObjectUploadRequest(CSkypeProto *ppro, const char *szObject, const uint8_t *data, int size, CFileUploadParam *fup) :
		AsyncHttpRequest(REQUEST_PUT, HOST_OTHER, 0, &CSkypeProto::OnASMObjectUploaded)
	{
		m_szUrl.AppendFormat("https://api.asm.skype.com/v1/objects/%s/content/original", szObject);
		pUserInfo = fup;

		AddHeader("Authorization", CMStringA(FORMAT, "skype_token %s", ppro->m_szApiToken.get()));
		AddHeader("Content-Type", "application/octet-stream");

		m_szParam.Truncate(size);
		memcpy(m_szParam.GetBuffer(), data, size);
	}
};

struct SendFileRequest : public AsyncHttpRequest
{
	SendFileRequest(const char *username, time_t timestamp, const char *message, const char *messageType, const char *asmRef) :
		AsyncHttpRequest(REQUEST_POST, HOST_DEFAULT, 0, &CSkypeProto::OnMessageSent)
	{
		m_szUrl.AppendFormat("/users/ME/conversations/%s/messages", mir_urlEncode(username).c_str());

		JSONNode node, ref(JSON_ARRAY);
		ref.set_name("amsreferences"); ref.push_back(JSONNode("", asmRef));

		node << INT64_PARAM("clientmessageid", timestamp) << CHAR_PARAM("messagetype", messageType)
			<< CHAR_PARAM("contenttype", "text") << CHAR_PARAM("content", message) << ref;
		m_szParam = node.write().c_str();
	}
};
