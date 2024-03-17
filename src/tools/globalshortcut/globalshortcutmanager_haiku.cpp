/*
 * globalshortcutmanager_haiku.cpp - Haiku implementation of global shortcuts by Vitaly (Diger)
 * Based on X11 implementation of global shortcuts
 * Copyright (C) 2003-2006  Justin Karneges, Maciej Niedzielski
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "globalshortcutmanager.h"
#include "globalshortcuttrigger.h"

#include <InterfaceDefs.h>
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QWidget>

class HKeyTrigger {
public:
    virtual ~HKeyTrigger() { }
    virtual void activate()                 = 0;
    virtual bool isAccepted(int qkey) const = 0;
};

class HKeyTriggerManager : public QObject {
public:
    static HKeyTriggerManager *instance()
    {
        if (!instance_)
            instance_ = new HKeyTriggerManager();
        return instance_;
    }

    void addTrigger(HKeyTrigger *trigger) { triggers_ << trigger; }

    void removeTrigger(HKeyTrigger *trigger) { triggers_.removeAll(trigger); }

    struct Qt_HK_Keygroup {
        char num;
        int  sym[3];
    };

protected:
    // reimplemented
    bool eventFilter(QObject *o, QEvent *e)
    {
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent *k    = static_cast<QKeyEvent *>(e);
            int        qkey = k->key();
            if (k->modifiers() & Qt::ShiftModifier)
                qkey |= Qt::SHIFT;
            if (k->modifiers() & Qt::ControlModifier)
                qkey |= Qt::CTRL;
            if (k->modifiers() & Qt::AltModifier)
                qkey |= Qt::ALT;
            if (k->modifiers() & Qt::MetaModifier)
                qkey |= Qt::META;

            foreach (HKeyTrigger *trigger, triggers_) {
                if (trigger->isAccepted(qkey)) {
                    trigger->activate();
                    return true;
                }
            }
        }

        return QObject::eventFilter(o, e);
    }

private:
    HKeyTriggerManager() : QObject(QCoreApplication::instance())
    {
        QCoreApplication::instance()->installEventFilter(this);
    }

    static HKeyTriggerManager *instance_;
    QList<HKeyTrigger *>       triggers_;

private:
    struct Qt_HK_Keymap {
        int            key;
        Qt_HK_Keygroup hk;
    };

    static Qt_HK_Keymap qt_hk_table[];
    static long         alt_mask;
    static long         meta_mask;
    static long         super_mask;
    static long         hyper_mask;
    static long         numlock_mask;
    static bool         haveMods;

    // adapted from qapplication_x11.cpp
    static void ensureModifiers()
    {
        if (haveMods)
            return;
    }

public:
    static bool convertKeySequence(const QKeySequence &ks, unsigned int *_mod, Qt_HK_Keygroup *_kg)
    {
        int code = ks[0];
        ensureModifiers();

        unsigned int mod = 0;
        /*
                if (code & Qt::META)
                        mod |= meta_mask;
                if (code & Qt::SHIFT)
                        mod |= ShiftMask;
                if (code & Qt::CTRL)
                        mod |= ControlMask;
                if (code & Qt::ALT)
                        mod |= alt_mask;
                */
        Qt_HK_Keygroup kg;
        kg.num    = 0;
        kg.sym[0] = 0;
        code &= ~Qt::KeyboardModifierMask;

        bool found = false;
        for (int n = 0; qt_hk_table[n].key != Qt::Key_unknown; ++n) {
            if (qt_hk_table[n].key == code) {
                kg    = qt_hk_table[n].hk;
                found = true;
                break;
            }
        }

        if (!found) {
            // try latin1
            if (code >= 0x20 && code <= 0x7f) {
                kg.num    = 1;
                kg.sym[0] = code;
            }
        }

        if (!kg.num)
            return false;

        if (_mod)
            *_mod = mod;
        if (_kg)
            *_kg = kg;

        return true;
    }

    static QList<long> ignModifiersList()
    {
        QList<long> ret;
        /*
                if (numlock_mask) {
                        ret << 0 << LockMask << numlock_mask << (LockMask | numlock_mask);
                }
                else {
                        ret << 0 << LockMask;
                }
                */
        return ret;
    }
};

HKeyTriggerManager *HKeyTriggerManager::instance_ = NULL;
class GlobalShortcutManager::KeyTrigger::Impl : public HKeyTrigger {
private:
    KeyTrigger *trigger_;
    int         qkey_;

    struct GrabbedKey {
        int  code;
        uint mod;
    };
    QList<GrabbedKey> grabbedKeys_;

    static bool failed;

public:
    /**
     * Constructor registers the hotkey.
     */
    Impl(GlobalShortcutManager::KeyTrigger *t, const QKeySequence &ks) : trigger_(t), qkey_(ks[0])
    {
        HKeyTriggerManager::instance()->addTrigger(this);

        HKeyTriggerManager::Qt_HK_Keygroup kg;
        unsigned int                       mod;
    }

    /**
     * Destructor unregisters the hotkey.
     */
    ~Impl() { HKeyTriggerManager::instance()->removeTrigger(this); }

    void activate() { emit trigger_->triggered(); }

    bool isAccepted(int qkey) const { return qkey_ == qkey; }
};

bool GlobalShortcutManager::KeyTrigger::Impl::failed;
long HKeyTriggerManager::alt_mask     = 0;
long HKeyTriggerManager::meta_mask    = 0;
long HKeyTriggerManager::super_mask   = 0;
long HKeyTriggerManager::hyper_mask   = 0;
long HKeyTriggerManager::numlock_mask = 0;
bool HKeyTriggerManager::haveMods     = false;

HKeyTriggerManager::Qt_HK_Keymap HKeyTriggerManager::qt_hk_table[] = {
    { Qt::Key_Escape, { 1, { B_ESCAPE } } },
    { Qt::Key_Tab, { 1, { B_TAB } } },
    { Qt::Key_Backtab, { 0, { 0 } } },
    { Qt::Key_Backspace, { 1, { B_BACKSPACE } } },
    { Qt::Key_Return, { 1, { B_RETURN } } },
    { Qt::Key_Enter, { 1, { B_ENTER } } },
    { Qt::Key_Insert, { 1, { B_INSERT } } },
    { Qt::Key_Delete, { 1, { B_DELETE } } },
    { Qt::Key_Pause, { 1, { B_PAUSE_KEY } } },
    { Qt::Key_Print, { 1, { B_PRINT_KEY } } },
    { Qt::Key_SysReq, { 0, { 0 } } },
    //{ Qt::Key_Clear,       B_CLEAR_KEY},
    { Qt::Key_Home, { 1, { B_HOME } } },
    { Qt::Key_End, { 1, { B_END } } },
    { Qt::Key_Left, { 1, { B_LEFT_ARROW } } },
    { Qt::Key_Up, { 1, { B_UP_ARROW } } },
    { Qt::Key_Right, { 1, { B_RIGHT_ARROW } } },
    { Qt::Key_Down, { 1, { B_DOWN_ARROW } } },
    { Qt::Key_PageUp, { 1, { B_PAGE_UP } } },
    { Qt::Key_PageDown, { 1, { B_PAGE_DOWN } } },
    { Qt::Key_Shift, { 1, { B_SHIFT_KEY } } },
    { Qt::Key_Control, { 1, { B_CONTROL_KEY } } },
    { Qt::Key_Meta, { 1, { B_LEFT_OPTION_KEY } } },
    { Qt::Key_Alt, { 1, { B_MENU_KEY } } },
    { Qt::Key_CapsLock, { 1, { B_CAPS_LOCK } } },
    { Qt::Key_NumLock, { 1, { B_NUM_LOCK } } },
    { Qt::Key_ScrollLock, { 1, { B_SCROLL_KEY } } },
    { Qt::Key_F1, { 1, { B_F1_KEY } } },
    { Qt::Key_F2, { 1, { B_F2_KEY } } },
    { Qt::Key_F3, { 1, { B_F3_KEY } } },
    { Qt::Key_F4, { 1, { B_F4_KEY } } },
    { Qt::Key_F5, { 1, { B_F5_KEY } } },
    { Qt::Key_F6, { 1, { B_F6_KEY } } },
    { Qt::Key_F7, { 1, { B_F7_KEY } } },
    { Qt::Key_F8, { 1, { B_F8_KEY } } },
    { Qt::Key_F9, { 1, { B_F9_KEY } } },
    { Qt::Key_F10, { 1, { B_F10_KEY } } },
    { Qt::Key_F11, { 1, { B_F11_KEY } } },
    { Qt::Key_F12, { 1, { B_F12_KEY } } },
    { Qt::Key_F13, { 0, { 0 } } },
    { Qt::Key_F14, { 0, { 0 } } },
    { Qt::Key_F15, { 0, { 0 } } },
    { Qt::Key_F16, { 0, { 0 } } },
    { Qt::Key_F17, { 0, { 0 } } },
    { Qt::Key_F18, { 0, { 0 } } },
    { Qt::Key_F19, { 0, { 0 } } },
    { Qt::Key_F20, { 0, { 0 } } },
    { Qt::Key_F21, { 0, { 0 } } },
    { Qt::Key_F22, { 0, { 0 } } },
    { Qt::Key_F23, { 0, { 0 } } },
    { Qt::Key_F24, { 0, { 0 } } },
    { Qt::Key_F25, { 0, { 0 } } },
    { Qt::Key_F26, { 0, { 0 } } },
    { Qt::Key_F27, { 0, { 0 } } },
    { Qt::Key_F28, { 0, { 0 } } },
    { Qt::Key_F29, { 0, { 0 } } },
    { Qt::Key_F30, { 0, { 0 } } },
    { Qt::Key_F31, { 0, { 0 } } },
    { Qt::Key_F32, { 0, { 0 } } },
    { Qt::Key_F33, { 0, { 0 } } },
    { Qt::Key_F34, { 0, { 0 } } },
    { Qt::Key_F35, { 0, { 0 } } },
    { Qt::Key_Super_L, { 0, { 0 } } },
    { Qt::Key_Super_R, { 0, { 0 } } },
    { Qt::Key_Menu, { 0, { 0 } } },
    { Qt::Key_Hyper_L, { 0, { 0 } } },
    { Qt::Key_Hyper_R, { 0, { 0 } } },
    { Qt::Key_Help, { 0, { 0 } } },
    { Qt::Key_Direction_L, { 0, { 0 } } },
    { Qt::Key_Direction_R, { 0, { 0 } } },

    { Qt::Key_unknown, { 0, { 0 } } },
};
GlobalShortcutManager::KeyTrigger::KeyTrigger(const QKeySequence &key) { d = new Impl(this, key); }

GlobalShortcutManager::KeyTrigger::~KeyTrigger()
{
    delete d;
    d = 0;
}
