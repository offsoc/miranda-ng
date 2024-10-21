/*
Copyright (C) 2012-24 Miranda NG team (https://miranda-ng.org)

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

#define MenuExecService "/NSExecMenu"

void CTelegramProto::InitMenus()
{
	if (!HookProtoEvent(ME_NS_PREBUILDMENU, &CTelegramProto::OnPrebuildNSMenu))
		return;

	CreateProtoService(MenuExecService, &CTelegramProto::SvcExecMenu);

	CMStringA szServiceName(FORMAT, "%s%s", m_szModuleName, MenuExecService);
	CMenuItem mi(&g_plugin);
	mi.pszService = szServiceName;

	mi.position = NS_PROTO_MENU_POS;
	mi.hIcolibItem = g_plugin.getIconHandle(IDI_FORWARD);
	mi.name.a = LPGEN("Forward");
	hmiForward = Menu_AddNewStoryMenuItem(&mi, 1);

	mi.position++;
	mi.hIcolibItem = g_plugin.getIconHandle(IDI_REACTION);
	mi.name.a = LPGEN("Reaction");
	hmiReaction = Menu_AddNewStoryMenuItem(&mi, 2);
}

int CTelegramProto::OnPrebuildNSMenu(WPARAM hContact, LPARAM)
{
	if (!Proto_IsProtoOnContact(hContact, m_szModuleName)) {
		Menu_ShowItem(hmiForward, false);
		Menu_ShowItem(hmiReaction, false);
	}
	else {
		Menu_ShowItem(hmiForward, 0 == getByte(hContact, "Protected"));

		auto *pUser = FindUser(GetId(hContact));
		Menu_ShowItem(hmiReaction, pUser && pUser->pReactions);
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Dialog for message forwarding

class CForwardDlg : public CTelegramDlgBase
{
	CCtrlClc m_clist;
	std::vector<MEVENT> &m_ids;

	void FilterList(CCtrlClc *)
	{
		for (auto &hContact : Contacts())
			if (!Proto_IsProtoOnContact(hContact, m_proto->m_szModuleName))
				if (HANDLE hItem = m_clist.FindContact(hContact))
					m_clist.DeleteItem(hItem);
	}

	void ResetListOptions(CCtrlClc *)
	{
		m_clist.SetHideEmptyGroups(true);
		m_clist.SetHideOfflineRoot(true);
	}

public:
	CForwardDlg(CTelegramProto *ppro, std::vector<MEVENT> &ids) :
		CTelegramDlgBase(ppro, IDD_FORWARD),
		m_ids(ids),
		m_clist(this, IDC_CLIST)
	{
		m_clist.OnNewContact = 
			m_clist.OnListRebuilt = Callback(this, &CForwardDlg::FilterList);
		m_clist.OnOptionsChanged = Callback(this, &CForwardDlg::ResetListOptions);
	}

	bool OnInitDialog() override
	{
		SetWindowLongPtr(m_clist.GetHwnd(), GWL_STYLE,
			GetWindowLongPtr(m_clist.GetHwnd(), GWL_STYLE) | CLS_SHOWHIDDEN | CLS_HIDEOFFLINE | CLS_CHECKBOXES | CLS_HIDEEMPTYGROUPS | CLS_USEGROUPS | CLS_GREYALTERNATE | CLS_GROUPCHECKBOXES);
		m_clist.SendMsg(CLM_SETEXSTYLE, CLS_EX_DISABLEDRAGDROP | CLS_EX_TRACKSELECT, 0);
		ResetListOptions(&m_clist);
		FilterList(&m_clist);
		return true;
	}

	bool OnApply() override
	{
		auto *pMe = m_proto->FindUser(m_proto->GetId(db_event_getContact(m_ids[0])));
		if (pMe == nullptr)
			return false;

		TD::array<TD::int53> message_ids;
		for (auto &it : m_ids) {
			DB::EventInfo dbei(it, false);
			if (dbei && dbei.szId)
				message_ids.push_back(dbei2id(dbei));
		}
		std::sort(message_ids.begin(), message_ids.end());

		for (auto &hContact : m_proto->AccContacts()) {
			if (HANDLE hItem = m_clist.FindContact(hContact)) {
				if (!m_clist.GetCheck(hItem))
					continue;

				auto *pUser = m_proto->FindUser(m_proto->GetId(hContact));

				TD::array<TD::int53> ids = message_ids;
				m_proto->SendQuery(new TD::forwardMessages(pUser->chatId, 0, pMe->chatId, std::move(ids), 0, false, false));
			}
		}

		return true;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// Dialog for sending reaction

class CReactionsDlg : public CTelegramDlgBase
{
	MEVENT m_hEvent;
	TG_USER *m_pUser;
	CCtrlCombo cmbReactions;

public: 
	CReactionsDlg(CTelegramProto *ppro, MEVENT hEvent) :
		CTelegramDlgBase(ppro, IDD_REACTIONS),
		m_hEvent(hEvent),
		cmbReactions(this, IDC_REACTIONS)
	{
		m_pUser = ppro->FindUser(ppro->GetId(db_event_getContact(hEvent)));
	}

	bool OnInitDialog() override
	{
		for (auto &it : *m_pUser->pReactions)
			cmbReactions.AddString(Utf2T(it), (LPARAM)it);
		return true;
	}

	bool OnApply() override
	{
		DB::EventInfo dbei(m_hEvent, false);
		__int64 msgId = ::dbei2id(dbei);
		
		if (char *pszEmoji = (char *)cmbReactions.GetCurData()) {
			auto reaction = TD::make_object<TD::reactionTypeEmoji>(pszEmoji);
			m_proto->SendQuery(new TD::addMessageReaction(m_pUser->chatId, msgId, std::move(reaction), false, false));
		}
		return true;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// Module entry point

INT_PTR CTelegramProto::SvcExecMenu(WPARAM iCommand, LPARAM pHandle)
{
	MEVENT hCurrentEvent = NS_GetCurrent((HANDLE)pHandle);

	switch (iCommand) {
	case 1: // forward message
		{	std::vector<MEVENT> ids = NS_GetSelection(HANDLE(pHandle));
			if (!ids.empty())
				CForwardDlg(this, ids).DoModal();
		}
		break;

	case 2: // reactions
		if (hCurrentEvent != -1)
			CReactionsDlg(this, hCurrentEvent).DoModal();
		break;
	}
	return 0;
}
