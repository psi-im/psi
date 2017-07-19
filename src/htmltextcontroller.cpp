/*
 * htmltextcontroller.cpp
 * Copyright (C) 2010  Evgeny Khryukin
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
#include "htmltextcontroller.h"
#include <QMenu>
#include <QFontDialog>
#include <QColorDialog>

class HTMLTextMenu : public QMenu
{
	Q_OBJECT
public:
	HTMLTextMenu(QWidget* parent = 0): QMenu(parent)
	{
		actBold = addAction(tr("Bold"));
		actBold->setCheckable(true);
		actBold->setData(QVariant(HTMLTextController::StateBold));

		actItalic = addAction(tr("Italic"));
		actItalic->setCheckable(true);
		actItalic->setData(QVariant(HTMLTextController::StateItalic));

		actUnderline = addAction(tr("Underline"));
		actUnderline->setCheckable(true);
		actUnderline->setData(QVariant(HTMLTextController::StateUnderline));

		actStrikeOut = addAction(tr("Strike Out"));
		actStrikeOut->setCheckable(true);
		actStrikeOut->setData(QVariant(HTMLTextController::StateStrikeOut));

		actTextColor = addAction(tr("Text Color"));
		actTextColor->setCheckable(true);
		actTextColor->setData(QVariant(HTMLTextController::StateTextColorChanged));

		actBackgroundColor = addAction(tr("Background Color"));
		actBackgroundColor->setCheckable(true);
		actBackgroundColor->setData(QVariant(HTMLTextController::StateBackgroundColorChanged));

		actText = addAction(tr("Font"));
		actText->setCheckable(true);
		actText->setData(QVariant(HTMLTextController::StateTextStyleChanged));

		addSeparator();

		actReset = addAction(tr("Reset"));
		actReset->setData(QVariant(HTMLTextController::StateNone));
	};

	void setMenuState(QList<HTMLTextController::TextEditState> list)
	{
		foreach(HTMLTextController::TextEditState state, list) {
			switch(state) {
			case HTMLTextController::StateBold:
				actBold->setChecked(true);
				break;
			case HTMLTextController::StateItalic:
				actItalic->setChecked(true);
				break;
			case HTMLTextController::StateUnderline:
				actUnderline->setChecked(true);
				break;
			case HTMLTextController::StateStrikeOut:
				actStrikeOut->setChecked(true);
				break;
			case HTMLTextController::StateTextStyleChanged:
				//actText->setChecked(true);
				break;
			case HTMLTextController::StateTextColorChanged:
				actTextColor->setChecked(true);
				break;
			case HTMLTextController::StateBackgroundColorChanged:
				actBackgroundColor->setChecked(true);
				break;
			default:
				break;
			}
		}
	};

private:
	QAction *actBold;
	QAction *actUnderline;
	QAction *actItalic;
	QAction *actStrikeOut;
	QAction *actText;
	QAction *actTextColor;
	QAction *actBackgroundColor;
	QAction *actReset;

};

//----------------------------------------------------------------------------
// HTMLTextController
//----------------------------------------------------------------------------
HTMLTextController::HTMLTextController(QTextEdit *parent)
	: QObject()
	, te_(parent)
{
	font_ = te_->font();
	currentFont_ = font_;
	background_ = te_->currentCharFormat().background();
	currentBackground_ = background_;
	foreground_ = te_->currentCharFormat().foreground();
	currentForeground_ = foreground_;
}

void HTMLTextController::addState(TextEditState state)
{
	QTextCharFormat tcf;
	switch(state) {
	case StateBold:
		tcf.setFontWeight(QFont::Bold);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateItalic:
		tcf.setFontItalic(true);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateUnderline:
		tcf.setFontUnderline(true);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateStrikeOut:
		tcf.setFontStrikeOut(true);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateTextStyleChanged:
		tcf.setFont(currentFont_);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateTextColorChanged:
		tcf.setForeground(currentForeground_);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateBackgroundColorChanged:
		tcf.setBackground(currentBackground_);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateNone:
		tcf.setFontWeight(QFont::Normal);
		tcf.setFontItalic(false);
		tcf.setFontUnderline(false);
		tcf.setFontStrikeOut(false);
		tcf.setFont(font_);
		tcf.setBackground(background_);
		tcf.setForeground(foreground_);
		te_->mergeCurrentCharFormat(tcf);
		break;

	default:
		break;
	}
}

void HTMLTextController::removeState(TextEditState state)
{
	QTextCharFormat tcf;
	switch(state) {
	case StateBold:
		tcf.setFontWeight(QFont::Normal);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateItalic:
		tcf.setFontItalic(false);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateUnderline:
		tcf.setFontUnderline(false);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateStrikeOut:
		tcf.setFontStrikeOut(false);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateTextStyleChanged:
		tcf.setFont(font_);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateTextColorChanged:
		tcf.setForeground(foreground_);
		te_->mergeCurrentCharFormat(tcf);
		break;
	case StateBackgroundColorChanged:
		tcf.setBackground(background_);
		te_->mergeCurrentCharFormat(tcf);
		break;
	default:
		break;
	}
}

QList<HTMLTextController::TextEditState>  HTMLTextController::state()
{
	QList<TextEditState> list;
	const QTextCharFormat tcf = te_->textCursor().charFormat();
	if(tcf.fontWeight() == QFont::Bold)
		list.push_back(StateBold);
	if(tcf.fontItalic())
		list.push_back(StateItalic);
	if(tcf.fontUnderline())
		list.push_back(StateUnderline);
	if(tcf.fontStrikeOut())
		list.push_back(StateStrikeOut);
	if(tcf.font() != font_)
		list.push_back(StateTextStyleChanged);
	if(tcf.background() != background_)
		list.push_back(StateBackgroundColorChanged);
	if(tcf.foreground() != foreground_)
		list.push_back(StateTextColorChanged);

	return list;
}

void HTMLTextController::doMenu()
{
	HTMLTextMenu *menu = new HTMLTextMenu();
	menu->setMenuState(state());
	if(!cssString_.isEmpty())
		menu->setStyleSheet(cssString_);
	QAction *act = menu->exec(QCursor::pos());
	if(!act) {
		delete menu;
		return;
	}
	int data = act->data().toInt();
	if(data == StateBold) {
		if(act->isChecked())
			addState(StateBold);
		else
			removeState(StateBold);
	}
	if(data == StateItalic) {
		if(act->isChecked())
			addState(StateItalic);
		else
			removeState(StateItalic);
	}
	else if(data == StateUnderline) {
		if(act->isChecked())
			addState(StateUnderline);
		else
			removeState(StateUnderline);
	}
	else if(data == StateStrikeOut) {
		if(act->isChecked())
			addState(StateStrikeOut);
		else
			removeState(StateStrikeOut);
	}
	else if(data == StateTextStyleChanged) {
		if(act->isChecked()) {
			bool ok;
			QFont tmpFont = QFontDialog::getFont(&ok, font_);
			if(ok) {
				currentFont_ = tmpFont;
				addState(StateTextStyleChanged);
			}
		}
		else {
			currentFont_ = font_;
			removeState(StateTextStyleChanged);
		}
	}
	else if(data == StateTextColorChanged) {
		if(act->isChecked()) {
			QColor color = QColorDialog::getColor(currentForeground_.color());
			if(color.isValid()) {
				currentForeground_ = QBrush(color);
				addState(StateTextColorChanged);
			}
		}
		else {
			currentForeground_ = foreground_;
			removeState(StateTextColorChanged);
		}
	}
	else if(data == StateBackgroundColorChanged) {
		if(act->isChecked()) {
			QColor color = QColorDialog::getColor(currentBackground_.color());
			if(color.isValid()) {
				currentBackground_ = QBrush(color);
				addState(StateBackgroundColorChanged);
			}
		}
		else {
			currentBackground_ = background_;
			removeState(StateBackgroundColorChanged);
		}
	}
	else if(data == StateNone)
		addState(StateNone);

	delete menu;
	te_->window()->activateWindow();
	te_->setFocus(Qt::MouseFocusReason);

}

void HTMLTextController::setFont(const QFont &f)
{
	font_ = f;
}

#include "htmltextcontroller.moc"
