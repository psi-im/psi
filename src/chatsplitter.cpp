/*
 * chatsplitter.cpp - QSplitter replacement that masquerades it
 * Copyright (C) 2007  Michail Pishchagin
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
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "chatsplitter.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QChildEvent>

#include "psioptions.h"

/**
 * Handy widget that masquerades itself as QSplitter, and could work
 * in both QSplitter mode, and QSplitter-less mode.
 */
ChatSplitter::ChatSplitter(QWidget* parent)
	: QWidget(parent)
	, splitterEnabled_(true)
	, splitter_(0)
	, layout_(0)
{
	connect(PsiOptions::instance(), SIGNAL(optionChanged(const QString&)), SLOT(optionsChanged()));
	optionsChanged();

	if (!layout_)
		updateLayout();
}

/**
 * Dummy function to fool Qt Designer-generated forms.
 */
void ChatSplitter::setOrientation(Qt::Orientation orientation)
{
	Q_ASSERT(orientation == Qt::Vertical);
	Q_UNUSED(orientation);
}

/**
 * Adds the given \a widget to the splitter's layout after all the
 * other items. Don't call this function if widget is already added
 * to the splitter's layout.
 */
void ChatSplitter::addWidget(QWidget* widget)
{
	Q_ASSERT(!children_.contains(widget));
	children_ << widget;
	connect(widget, SIGNAL(destroyed(QObject*)), SLOT(childDestroyed(QObject*)));
	updateChildLayout(widget);
}

/**
 * If splitter mode is enabled, the \a list is passed to the
 * actual splitter. Has no effect otherwise.
 */
void ChatSplitter::setSizes(const QList<int>& list)
{
	if (splitter_)
		splitter_->setSizes(list);
}

/**
 * Moves \a child either to the real QSplitter, or adds it to the
 * private layout.
 */
void ChatSplitter::updateChildLayout(QWidget* child)
{
	if (splitterEnabled() && splitter_) {
		splitter_->addWidget(child);
	}
	else {
		layout_->addWidget(child);
	}
}

/**
 * Removes destroyed widget from the splitter's child list.
 */
void ChatSplitter::childDestroyed(QObject* obj)
{
	Q_ASSERT(obj->isWidgetType());
	children_.removeAll(static_cast<QWidget*>(obj));
}

/**
 * If \a enable is true, the true QSplitter gets enabled, otherwise
 * all widgets are managed by QLayout.
 */
void ChatSplitter::setSplitterEnabled(bool enable)
{
	if (splitterEnabled_ == enable)
		return;

	splitterEnabled_ = enable;
	updateLayout();
}

/**
 * Invalidates layout and moves all child widgets to the proper position.
 */
void ChatSplitter::updateLayout()
{
	foreach(QWidget* child, children_)
		child->setParent(this);

	delete splitter_;
	delete layout_;
	splitter_ = new QSplitter(this);
	layout_ = new QVBoxLayout(this);
	layout_->setMargin(0);
	layout_->addWidget(splitter_);
	splitter_->setOrientation(Qt::Vertical);
	splitter_->setVisible(splitterEnabled());

	foreach(QWidget* child, children_)
		updateChildLayout(child);
}

/**
 * Updates layout according to current options.
 * FIXME: When PsiOptions::instance()->getOption("options.ui.chat.use-expanding-line-edit").toBool() finally makes it to PsiOptions, make this slot
 *        private.
 */
void ChatSplitter::optionsChanged()
{
	setSplitterEnabled(!PsiOptions::instance()->getOption("options.ui.chat.use-expanding-line-edit").toBool());
}
