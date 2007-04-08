/*
 * globalaccelman_x11.cpp - manager for global hotkeys
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

#include "globalaccelman.h"

#include <qkeysequence.h>
#include <qwidget.h>
#include <qapplication.h>
//Added by qt3to4:
#include <QKeyEvent>
#include <QEvent>
#include <Q3PtrList>
#include <QX11Info>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#ifdef KeyPress
// defined by X11 headers
const int XKeyPress = KeyPress;
const int XKeyRelease = KeyRelease;
#undef KeyPress
#endif

struct QT_XK_KEYGROUP
{
	char num;
	int sym[3];
};

struct QT_XK_KEYMAP
{
	int key;
	QT_XK_KEYGROUP xk;
} qt_xk_table[] = {
	{ Qt::Key_Escape,     {1, { XK_Escape }}},
	{ Qt::Key_Tab,        {2, { XK_Tab, XK_KP_Tab }}},
	{ Qt::Key_Backtab,    {1, { XK_ISO_Left_Tab }}},
	{ Qt::Key_Backspace,  {1, { XK_BackSpace }}},
	{ Qt::Key_Return,     {1, { XK_Return }}},
	{ Qt::Key_Enter,      {1, { XK_KP_Enter }}},
	{ Qt::Key_Insert,     {2, { XK_Insert, XK_KP_Insert }}},
	{ Qt::Key_Delete,     {3, { XK_Delete, XK_KP_Delete, XK_Clear }}},
	{ Qt::Key_Pause,      {1, { XK_Pause }}},
	{ Qt::Key_Print,      {1, { XK_Print }}},
	{ Qt::Key_SysReq,     {1, { XK_Sys_Req }}},
	{ Qt::Key_Clear,      {1, { XK_KP_Begin }}},
	{ Qt::Key_Home,       {2, { XK_Home, XK_KP_Home }}},
	{ Qt::Key_End,        {2, { XK_End, XK_KP_End }}},
	{ Qt::Key_Left,       {2, { XK_Left, XK_KP_Left }}},
	{ Qt::Key_Up,         {2, { XK_Up, XK_KP_Up }}},
	{ Qt::Key_Right,      {2, { XK_Right, XK_KP_Right }}},
	{ Qt::Key_Down,       {2, { XK_Down, XK_KP_Down }}},
	{ Qt::Key_PageUp,      {2, { XK_Prior, XK_KP_Prior }}},
	{ Qt::Key_PageDown,       {2, { XK_Next, XK_KP_Next }}},
	{ Qt::Key_Shift,      {3, { XK_Shift_L, XK_Shift_R, XK_Shift_Lock  }}},
	{ Qt::Key_Control,    {2, { XK_Control_L, XK_Control_R }}},
	{ Qt::Key_Meta,       {2, { XK_Meta_L, XK_Meta_R }}},
	{ Qt::Key_Alt,        {2, { XK_Alt_L, XK_Alt_R }}},
	{ Qt::Key_CapsLock,   {1, { XK_Caps_Lock }}},
	{ Qt::Key_NumLock,    {1, { XK_Num_Lock }}},
	{ Qt::Key_ScrollLock, {1, { XK_Scroll_Lock }}},
	{ Qt::Key_Space,      {2, { XK_space, XK_KP_Space }}},
	{ Qt::Key_Equal,      {2, { XK_equal, XK_KP_Equal }}},
	{ Qt::Key_Asterisk,   {2, { XK_asterisk, XK_KP_Multiply }}},
	{ Qt::Key_Plus,       {2, { XK_plus, XK_KP_Add }}},
	{ Qt::Key_Comma,      {2, { XK_comma, XK_KP_Separator }}},
	{ Qt::Key_Minus,      {2, { XK_minus, XK_KP_Subtract }}},
	{ Qt::Key_Period,     {2, { XK_period, XK_KP_Decimal }}},
	{ Qt::Key_Slash,      {2, { XK_slash, XK_KP_Divide }}},
	{ Qt::Key_F1,         {1, { XK_F1 }}},
	{ Qt::Key_F2,         {1, { XK_F2 }}},
	{ Qt::Key_F3,         {1, { XK_F3 }}},
	{ Qt::Key_F4,         {1, { XK_F4 }}},
	{ Qt::Key_F5,         {1, { XK_F5 }}},
	{ Qt::Key_F6,         {1, { XK_F6 }}},
	{ Qt::Key_F7,         {1, { XK_F7 }}},
	{ Qt::Key_F8,         {1, { XK_F8 }}},
	{ Qt::Key_F9,         {1, { XK_F9 }}},
	{ Qt::Key_F10,        {1, { XK_F10 }}},
	{ Qt::Key_F11,        {1, { XK_F11 }}},
	{ Qt::Key_F12,        {1, { XK_F12 }}},
	{ Qt::Key_F13,        {1, { XK_F13 }}},
	{ Qt::Key_F14,        {1, { XK_F14 }}},
	{ Qt::Key_F15,        {1, { XK_F15 }}},
	{ Qt::Key_F16,        {1, { XK_F16 }}},
	{ Qt::Key_F17,        {1, { XK_F17 }}},
	{ Qt::Key_F18,        {1, { XK_F18 }}},
	{ Qt::Key_F19,        {1, { XK_F19 }}},
	{ Qt::Key_F20,        {1, { XK_F20 }}},
	{ Qt::Key_F21,        {1, { XK_F21 }}},
	{ Qt::Key_F22,        {1, { XK_F22 }}},
	{ Qt::Key_F23,        {1, { XK_F23 }}},
	{ Qt::Key_F24,        {1, { XK_F24 }}},
	{ Qt::Key_F25,        {1, { XK_F25 }}},
	{ Qt::Key_F26,        {1, { XK_F26 }}},
	{ Qt::Key_F27,        {1, { XK_F27 }}},
	{ Qt::Key_F28,        {1, { XK_F28 }}},
	{ Qt::Key_F29,        {1, { XK_F29 }}},
	{ Qt::Key_F30,        {1, { XK_F30 }}},
	{ Qt::Key_F31,        {1, { XK_F31 }}},
	{ Qt::Key_F32,        {1, { XK_F32 }}},
	{ Qt::Key_F33,        {1, { XK_F33 }}},
	{ Qt::Key_F34,        {1, { XK_F34 }}},
	{ Qt::Key_F35,        {1, { XK_F35 }}},
	{ Qt::Key_Super_L,    {1, { XK_Super_L }}},
	{ Qt::Key_Super_R,    {1, { XK_Super_R }}},
	{ Qt::Key_Menu,       {1, { XK_Menu }}},
	{ Qt::Key_Hyper_L,    {1, { XK_Hyper_L }}},
	{ Qt::Key_Hyper_R,    {1, { XK_Hyper_R }}},
	{ Qt::Key_Help,       {1, { XK_Help }}},
	{ Qt::Key_Direction_L,{0, { 0 }}},
	{ Qt::Key_Direction_R,{0, { 0 }}},

	{ Qt::Key_unknown,    {0, { 0 }}},
};

static long alt_mask = 0;
static long meta_mask = 0;
static bool haveMods = false;

// adapted from qapplication_x11.cpp
static void ensureModifiers()
{
	if(haveMods)
		return;

	Display *appDpy = QX11Info::display();
	XModifierKeymap *map = XGetModifierMapping(appDpy);
	if(map) {
		int i, maskIndex = 0, mapIndex = 0;
		for (maskIndex = 0; maskIndex < 8; maskIndex++) {
			for (i = 0; i < map->max_keypermod; i++) {
				if (map->modifiermap[mapIndex]) {
					KeySym sym = XKeycodeToKeysym(appDpy, map->modifiermap[mapIndex], 0);
					if ( alt_mask == 0 && ( sym == XK_Alt_L || sym == XK_Alt_R ) ) {
						alt_mask = 1 << maskIndex;
					}
					if ( meta_mask == 0 && (sym == XK_Meta_L || sym == XK_Meta_R ) ) {
						meta_mask = 1 << maskIndex;
					}
				}
				mapIndex++;
			}
		}

		XFreeModifiermap(map);
	}
	else {
		// assume defaults
		alt_mask = Mod1Mask;
		meta_mask = Mod4Mask;
	}

	haveMods = true;
}

static bool convertKeySequence(const QKeySequence &ks, unsigned int *_mod, QT_XK_KEYGROUP *_kg)
{
	int code = ks;
	ensureModifiers();

	unsigned int mod = 0;
	if(code & Qt::META)
		mod |= meta_mask;
	if(code & Qt::SHIFT)
		mod |= ShiftMask;
	if(code & Qt::CTRL)
		mod |= ControlMask;
	if(code & Qt::ALT)
		mod |= alt_mask;

	QT_XK_KEYGROUP kg;
	kg.num = 0;
	code &= 0xffff;
	// see if it is in our table
	bool found = false;
	for(int n = 0; qt_xk_table[n].key != Qt::Key_unknown; ++n) {
		if(qt_xk_table[n].key == code) {
			kg = qt_xk_table[n].xk;
			found = true;
			break;
		}
	}
	// not found?
	if(!found) {
		// try latin1
		if(code >= 0x20 && code <= 0x7f) {
			kg.num = 1;
			kg.sym[0] = code;
		}
	}
	if(!kg.num)
		return false;

	if(_mod)
		*_mod = mod;
	if(_kg)
		*_kg = kg;

	return true;
}

class X11HotKey
{
public:
	X11HotKey() {}
	~X11HotKey()
	{
		XUngrabKey(dsp, code, mod, grab);
	}

	static bool failed;
	static int XGrabErrorHandler(Display *, XErrorEvent *)
	{
		failed = true;
        	return 0;
	}

	static X11HotKey * bind(int keysym, unsigned int mod)
	{
		Display *dsp = QX11Info::display();
		Window grab = QX11Info::appRootWindow();
		int code = XKeysymToKeycode(dsp, keysym);

		// don't grab keys with empty code (because it means just the modifier key)
		if ( keysym && !code )
			return 0;

		failed = false;
		XErrorHandler savedErrorHandler = XSetErrorHandler(XGrabErrorHandler);
		XGrabKey(dsp, code, mod, grab, False, GrabModeAsync, GrabModeAsync);
		XSync(dsp, False);
		XSetErrorHandler(savedErrorHandler);
		if(failed)
			return 0;
		X11HotKey *k = new X11HotKey;
		k->dsp = dsp;
		k->grab = grab;
		k->code = code;
		k->mod = mod;
		return k;
	}

	Display *dsp;     // X11 display
	Window grab;      // Window (root window, in our case)
	int code;         // Keycode
	unsigned int mod; // modifiers (shift/alt/etc)
};

bool X11HotKey::failed;

class X11HotKeyGroup
{
public:
	X11HotKeyGroup()
	{
		list.setAutoDelete(true);
	}

	~X11HotKeyGroup()
	{
	}

	static X11HotKeyGroup * bind(const QT_XK_KEYGROUP &kg, unsigned int mod, int qkey, int id)
	{
		Q3PtrList<X11HotKey> list;
		for(int n = 0; n < kg.num; ++n) {
			X11HotKey *k = X11HotKey::bind(kg.sym[n], mod);
			if(k)
				list.append(k);
		}
		if(list.isEmpty())
			return 0;
		X11HotKeyGroup *h = new X11HotKeyGroup;
		h->list = list;
		h->qkey = qkey;
		h->id = id;
		return h;
	}

	Q3PtrList<X11HotKey> list; // Associated X11HotKeys
	int qkey;                 // Qt keycode (with modifiers)
	int id;                   // GlobalAccelManager unique ID for this hotkey
};

class GlobalAccelManager::Private
{
public:
	Private(GlobalAccelManager *_man)
	{
		man = _man;
		id_at = 1;
		list.setAutoDelete(true);
		f = new Filter(this);
		qApp->installEventFilter(f);
	}

	~Private()
	{
		qApp->removeEventFilter(f);
		delete f;
	}

	bool isThisYours(int qkey)
	{
		Q3PtrListIterator<X11HotKeyGroup> it(list);
		for(X11HotKeyGroup *k; (k = it.current()); ++it) {
			if(k->qkey == qkey) {
				man->triggerActivated(k->id);
				return true;
			}
		}
		return false;
	}

	class Filter : public QObject
	{
	public:
		Filter(Private *_p)
		{
			p = _p;
		}

	protected:
		bool eventFilter(QObject *o, QEvent *e)
		{
			if(e->type() == QEvent::KeyPress) {
				QKeyEvent *k = (QKeyEvent *)e;
				int qkey = k->key();
				if(k->modifiers() & Qt::ShiftModifier)
					qkey |= Qt::SHIFT;
				if(k->modifiers() & Qt::ControlModifier)
					qkey |= Qt::CTRL;
				if(k->modifiers() & Qt::AltModifier)
					qkey |= Qt::ALT;
				if(k->modifiers() & Qt::MetaModifier)
					qkey |= Qt::META;

				if(p->isThisYours(qkey))
					return true;
			}
			return QObject::eventFilter(o, e);
		}

	private:
		Private *p;
	};

	GlobalAccelManager *man;
	int id_at;
	Filter *f;
	Q3PtrList<X11HotKeyGroup> list;
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
	QT_XK_KEYGROUP kg;
	unsigned int mod;
	if(!convertKeySequence(ks, &mod, &kg))
		return 0;

	X11HotKeyGroup *h = X11HotKeyGroup::bind(kg, mod, ks, d->id_at++);
	if(!h)
		return 0;
	d->list.append(h);

	return h->id;
}

void GlobalAccelManager::removeAccel(int id)
{
	Q3PtrListIterator<X11HotKeyGroup> it(d->list);
	for(X11HotKeyGroup *hk = 0; (hk = it.current()); ++it) {
		if(hk->id == id) {
			d->list.removeRef(hk);
			return;
		}
	}
}
