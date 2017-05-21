/*
 * opt_theme.cpp
 * Copyright (C) 2010-2017  Sergey Ilinykh, Vitaly Tonkacheyev
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

#include "opt_theme.h"

#include "ui_opt_theme.h"
#include "psioptions.h"
#include "psithememodel.h"
#include "psithememanager.h"
#include "psiiconset.h"

#include <QToolButton>
#include <QDialog>
#include <QSortFilterProxyModel>
#include <QTimer>

#define SCREEN_PREFIX "scb_"

class OptAppearanceThemeUI : public QWidget, public Ui::OptAppearanceTheme
{
public:
	OptAppearanceThemeUI() : QWidget() { setupUi(this); }
};



OptionsTabAppearanceThemes::OptionsTabAppearanceThemes(QObject *parent)
	: MetaOptionsTab(parent, "themes", "", tr("Themes"), tr("Configure themes"))
{
	foreach (PsiThemeProvider *provider,
			 PsiThemeManager::instance()->registeredProviders()) {
		addTab( new OptionsTabAppearanceTheme(this, provider) );
	}
}





//----------------------------------------------------------------------------
// OptionsTabAppearanceTheme
//----------------------------------------------------------------------------

OptionsTabAppearanceTheme::OptionsTabAppearanceTheme(QObject *parent,
						     PsiThemeProvider *provider_)
	: OptionsTab(parent, provider_->type(), "",
				 provider_->optionsName(),
				 provider_->optionsDescription())
	, w(0)
	, provider(provider_)
{

}

OptionsTabAppearanceTheme::~OptionsTabAppearanceTheme()
{
}

QWidget *OptionsTabAppearanceTheme::widget()
{
	if ( w )
		return 0;

	w = new OptAppearanceThemeUI();
	OptAppearanceThemeUI *d = (OptAppearanceThemeUI *)w;

	unsortedModel = new PsiThemeModel(this);

	themesModel = new QSortFilterProxyModel(this);
	themesModel->setSourceModel(unsortedModel);
	themesModel->setSortCaseSensitivity(Qt::CaseInsensitive);

	d->themeView->setModel(themesModel);
	d->themeView->sortByColumn(0, Qt::AscendingOrder);

	connect(d->themeView->selectionModel(),
		SIGNAL(currentChanged(QModelIndex, QModelIndex)),
		SLOT(themeSelected(QModelIndex, QModelIndex)));

	connect(themesModel,
		SIGNAL(rowsInserted(QModelIndex,int,int)),
		SLOT(modelRowsInserted(QModelIndex,int,int)));

	QTimer::singleShot(0, this, SLOT(startLoading()));

	return w;
}

void OptionsTabAppearanceTheme::startLoading()
{
	unsortedModel->setType(provider->type());
}

void OptionsTabAppearanceTheme::themeSelected(const QModelIndex &current, const QModelIndex &previous)
{
	Q_UNUSED(current);
	if (!previous.isValid()) {
		return; // Psi won't start if it's impossible to load any theme. So we always have previous.
	}
	emit dataChanged();
}

void OptionsTabAppearanceTheme::modelRowsInserted(const QModelIndex &parent, int first, int last)
{
	if (!parent.isValid() || !w) {
		OptAppearanceThemeUI *d = (OptAppearanceThemeUI *)w;
		//const QSize buttonSize = QSize(21,21);
		for (int i = first; i <= last; i++) {
			const QModelIndex index = themesModel->index(i, 0);
			const QString id = themesModel->data(index, PsiThemeModel::IdRole).toString();
			if (themesModel->data(index, PsiThemeModel::IsCurrent).toBool()) {
				d->themeView->setCurrentIndex(index);
			}
#if 0
			const QString themeName = themesModel->data(index, PsiThemeModel::TitleRole).toString();
			bool isPsi = id.startsWith("psi");
			const QPixmap client = isPsi ? IconsetFactory::iconPixmap("clients/psi")
							 : IconsetFactory::iconPixmap("clients/adium");
			const QString clientName = isPsi ? tr("Psi Theme") : tr("Adium Theme");
			QToolButton *screenshotButton = new QToolButton(d->themeView);
			screenshotButton->setIcon(QIcon(IconsetFactory::iconPixmap("psi/eye")));
			screenshotButton->resize(buttonSize);
			screenshotButton->setObjectName(SCREEN_PREFIX + id);
			screenshotButton->setToolTip(tr("Show theme screenshot"));
			connect(screenshotButton, SIGNAL(clicked()), this, SLOT(showThemeScreenshot()));

			QLabel *typeLabel = new QLabel(d->themeView);
			typeLabel->setPixmap(client);
			typeLabel->setToolTip(clientName);

			QLabel *nameLabel = new QLabel(themeName, d->themeView);

			QWidget *itemWidget = new QWidget(d->themeView);

			QBoxLayout *box = new QBoxLayout(QBoxLayout::LeftToRight, d->themeView);
			box->addWidget(typeLabel);
			box->addWidget(nameLabel);
			box->addStretch();
			box->addWidget(screenshotButton);
			itemWidget->setLayout(box);
			//itemWidget->setAutoFillBackground(true); // from recommendation of indexWidget but does not work as expected

			d->themeView->setIndexWidget(index, itemWidget);
#endif
		}
	}
}

void OptionsTabAppearanceTheme::showThemeScreenshot()
{
	if ( !w || !sender()->inherits("QToolButton") )
		return;
	OptAppearanceThemeUI *d = (OptAppearanceThemeUI *)w;
	QToolButton *btn = static_cast<QToolButton*>(sender());
	if ( btn ) {
		if ( screenshotDialog )
			delete(screenshotDialog);

		const QSize minSize(300, 100);
		screenshotDialog = new QDialog(d);
		screenshotDialog->setMinimumSize(minSize);

		const int row = unsortedModel->themeRow(getThemeId(btn->objectName()));
		const QModelIndex index = unsortedModel->index(row, 0);
		const QString name_ = unsortedModel->data(index, PsiThemeModel::TitleRole).toString();
		const QPixmap scr = unsortedModel->data(index, PsiThemeModel::HasPreviewRole).value<QPixmap>();

		screenshotDialog->setWindowTitle(tr("%1 Screenshot").arg(name_));
		screenshotDialog->setWindowIcon(QIcon(IconsetFactory::iconPixmap("psi/logo_128")));

		QBoxLayout *box = new QBoxLayout(QBoxLayout::LeftToRight, screenshotDialog);
		QLabel *image = new QLabel(screenshotDialog);
		if (!scr.isNull()) {
			image->setPixmap(scr);
		}
		else {
			image->setText(tr("No Image"));
		}
		box->addWidget(image);

		screenshotDialog->setAttribute(Qt::WA_DeleteOnClose);
		screenshotDialog->show();
	}
}

QString OptionsTabAppearanceTheme::getThemeId(const QString &objName) const
{
	const int index = objName.indexOf("_", 0);
	return (index >0 ? objName.right(objName.length() - index - 1) : QString());
}

void OptionsTabAppearanceTheme::applyOptions()
{
	if ( !w )
		return;

	OptAppearanceThemeUI *d = (OptAppearanceThemeUI *)w;
	themesModel->setData(d->themeView->currentIndex(), true, PsiThemeModel::IsCurrent);
}

void OptionsTabAppearanceTheme::restoreOptions()
{
#if 0
	if ( !w )
		return;

	OptAppearanceThemeUI *d = (OptAppearanceThemeUI *)w;
#endif
}
