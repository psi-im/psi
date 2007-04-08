/*
 * globalaccelman_win.cpp - manager for global hotkeys
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

// TODO:
//  - don't invoke hotkey if there is a modal dialog?
//  - do multi-mapping, like the x11 version

#include "globalaccelman.h"

#include <QKeySequence>
#include <QWidget>
#include <Q3PtrList>

#include <windows.h>

// We'll start our hotkeys at offset 0x1000 in the Windows hotkey table (no real reason)
#define HOTKEY_START 0x1000

struct QT_VK_KEYMAP
{
	int key;
	UINT vk;
} qt_vk_table[] = {
	{ Qt::Key_Escape,     VK_ESCAPE },
	{ Qt::Key_Tab,        VK_TAB },
	{ Qt::Key_Backtab,    0 },
	{ Qt::Key_Backspace,  VK_BACK },
	{ Qt::Key_Return,     VK_RETURN },
	{ Qt::Key_Enter,      VK_RETURN },
	{ Qt::Key_Insert,     VK_INSERT },
	{ Qt::Key_Delete,     VK_DELETE },
	{ Qt::Key_Pause,      VK_PAUSE },
	{ Qt::Key_Print,      VK_PRINT },
	{ Qt::Key_SysReq,     0 },
	{ Qt::Key_Clear,      VK_CLEAR },
	{ Qt::Key_Home,       VK_HOME },
	{ Qt::Key_End,        VK_END },
	{ Qt::Key_Left,       VK_LEFT },
	{ Qt::Key_Up,         VK_UP },
	{ Qt::Key_Right,      VK_RIGHT },
	{ Qt::Key_Down,       VK_DOWN },
	{ Qt::Key_PageUp,      VK_PRIOR },
	{ Qt::Key_PageDown,       VK_NEXT },
	{ Qt::Key_Shift,      VK_SHIFT },
	{ Qt::Key_Control,    VK_CONTROL },
	{ Qt::Key_Meta,       VK_LWIN },
	{ Qt::Key_Alt,        VK_MENU },
	{ Qt::Key_CapsLock,   VK_CAPITAL },
	{ Qt::Key_NumLock,    VK_NUMLOCK },
	{ Qt::Key_ScrollLock, VK_SCROLL },
	{ Qt::Key_F1,         VK_F1 },
	{ Qt::Key_F2,         VK_F2 },
	{ Qt::Key_F3,         VK_F3 },
	{ Qt::Key_F4,         VK_F4 },
	{ Qt::Key_F5,         VK_F5 },
	{ Qt::Key_F6,         VK_F6 },
	{ Qt::Key_F7,         VK_F7 },
	{ Qt::Key_F8,         VK_F8 },
	{ Qt::Key_F9,         VK_F9 },
	{ Qt::Key_F10,        VK_F10 },
	{ Qt::Key_F11,        VK_F11 },
	{ Qt::Key_F12,        VK_F12 },
	{ Qt::Key_F13,        VK_F13 },
	{ Qt::Key_F14,        VK_F14 },
	{ Qt::Key_F15,        VK_F15 },
	{ Qt::Key_F16,        VK_F16 },
	{ Qt::Key_F17,        VK_F17 },
	{ Qt::Key_F18,        VK_F18 },
	{ Qt::Key_F19,        VK_F19 },
	{ Qt::Key_F20,        VK_F20 },
	{ Qt::Key_F21,        VK_F21 },
	{ Qt::Key_F22,        VK_F22 },
	{ Qt::Key_F23,        VK_F23 },
	{ Qt::Key_F24,        VK_F24 },
	{ Qt::Key_F25,        0 },
	{ Qt::Key_F26,        0 },
	{ Qt::Key_F27,        0 },
	{ Qt::Key_F28,        0 },
	{ Qt::Key_F29,        0 },
	{ Qt::Key_F30,        0 },
	{ Qt::Key_F31,        0 },
	{ Qt::Key_F32,        0 },
	{ Qt::Key_F33,        0 },
	{ Qt::Key_F34,        0 },
	{ Qt::Key_F35,        0 },
	{ Qt::Key_Super_L,    0 },
	{ Qt::Key_Super_R,    0 },
	{ Qt::Key_Menu,       0 },
	{ Qt::Key_Hyper_L,    0 },
	{ Qt::Key_Hyper_R,    0 },
	{ Qt::Key_Help,       0 },
	{ Qt::Key_Direction_L,0 },
	{ Qt::Key_Direction_R,0 },

	{ Qt::Key_unknown,    0 },
};

static bool convertKeySequence(const QKeySequence &ks, UINT *_mod, UINT *_key)
{
	int code = ks;

	UINT mod = 0;
	if(code & Qt::META)
		mod |= MOD_WIN;
	if(code & Qt::SHIFT)
		mod |= MOD_SHIFT;
	if(code & Qt::CTRL)
		mod |= MOD_CONTROL;
	if(code & Qt::ALT)
		mod |= MOD_ALT;

	UINT key = 0;
	code &= 0xffff;
	if(code >= 0x20 && code <= 0x7f)
		key = code;
	else {
		for(int n = 0; qt_vk_table[n].key != Qt::Key_unknown; ++n) {
			if(qt_vk_table[n].key == code) {
				key = qt_vk_table[n].vk;
				break;
			}
		}
		if(!key)
			return false;
	}

	if(_mod)
		*_mod = mod;
	if(_key)
		*_key = key;

	return true;
}

class WindowsHotKey
{
public:
	WindowsHotKey(HWND _w, int _win_key_id, int _id)
	{
		w = _w;
		win_key_id = _win_key_id;
		id = _id;
	}

	~WindowsHotKey()
	{
		UnregisterHotKey(w, win_key_id);
	}

	HWND w;
	int win_key_id; // Windows hotkey slot number
	int id;         // GlobalAccelManager unique ID for this hotkey
};

class GlobalAccelManager::Private
{
public:
	Private(GlobalAccelManager *par)
	{
		manager = par;
		id_at = 1;
		hidden = new HiddenWidget(this);
		list.setAutoDelete(true);
	}

	~Private()
	{
		delete hidden;
	}

	int getFreeSlot() const
	{
		Q3PtrListIterator<WindowsHotKey> it(list);
		// find first non-match
		int n = HOTKEY_START;
		for(WindowsHotKey *hk = 0; (hk = it.current()); ++it) {
			if(hk->win_key_id != n)
				break;
			++n;
		}
		return n;
	}

	int insertHotKey(int win_key_id)
	{
		WindowsHotKey *hk = new WindowsHotKey(hidden->winId(), win_key_id, id_at++);
		if(win_key_id >= (int)list.count())
			list.append(hk);
		else
			list.insert(win_key_id - HOTKEY_START, hk);
		return hk->id;
	}

	void hotkeyActivated(int win_key_id)
	{
		Q3PtrListIterator<WindowsHotKey> it(list);
		for(WindowsHotKey *hk = 0; (hk = it.current()); ++it) {
			if(hk->win_key_id == win_key_id)
				manager->triggerActivated(hk->id);
		}
	}

	class HiddenWidget : public QWidget
	{
	public:
		HiddenWidget(Private *_p)
		{
			p = _p;
		}

	protected:
		bool winEvent(MSG *m, long *result)
		{
			m->message &= 0xffff;
			if(m->message == WM_HOTKEY) {
				p->hotkeyActivated(m->wParam);
				return true;
			}
        		return QWidget::winEvent(m, result);
		}

	private:
		Private *p;
	};

	GlobalAccelManager *manager;
	int id_at;
	HiddenWidget *hidden;
	Q3PtrList<WindowsHotKey> list;
};

GlobalAccelManager::GlobalAccelManager()
{
	d = new Private(this);
}

GlobalAccelManager::~GlobalAccelManager()
{
	delete d;
}

int GlobalAccelManager::setAccel(const QKeySequence &ks)
{
	UINT mod, key;
	if(!convertKeySequence(ks, &mod, &key))
		return 0;

	int win_key_id = d->getFreeSlot();

	if(!RegisterHotKey(d->hidden->winId(), win_key_id, mod, key))
		return 0;

	return d->insertHotKey(win_key_id);
}

void GlobalAccelManager::removeAccel(int id)
{
	Q3PtrListIterator<WindowsHotKey> it(d->list);
	for(WindowsHotKey *hk = 0; (hk = it.current()); ++it) {
		if(hk->id == id) {
			d->list.removeRef(hk);
			return;
		}
	}
}
