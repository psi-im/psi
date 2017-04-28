#include "opt_theme.h"

#include "ui_opt_theme.h"
#include "psioptions.h"
#include "psithememodel.h"
#include "psithemeviewdelegate.h"
#include "psithememanager.h"
#include "psiiconset.h"

#include <QRadioButton>
#include <QToolButton>
#include <QDialog>

#define RADIO_PREFIX "rb_"
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
	themesModel = new PsiThemeModel(this);
	themesModel->setType(provider->type());
	d->themeView->setModel(themesModel);
	connect(d->themeView->selectionModel(),
		SIGNAL(currentChanged(QModelIndex, QModelIndex)),
		SIGNAL(dataChanged()));
	connect(d->themeView->selectionModel(),
		SIGNAL(currentChanged(QModelIndex,QModelIndex)),
		SLOT(itemChanged(QModelIndex,QModelIndex))); //changes radiobuttons state
	connect(themesModel,
		SIGNAL(rowsInserted(QModelIndex,int,int)),
		SLOT(modelRowsInserted(QModelIndex,int,int)));

	return w;
}

void OptionsTabAppearanceTheme::modelRowsInserted(const QModelIndex &parent, int first, int last)
{
	if (!parent.isValid() || !w) {
		Theme *theme = provider->current();
		OptAppearanceThemeUI *d = (OptAppearanceThemeUI *)w;
		const QSize buttonSize = QSize(21,21);
		if (theme) {
			for (int i = first; i <= last; i++) {
				const QModelIndex index = themesModel->index(i);
				const QString id = themesModel->data(index, PsiThemeModel::IdRole).toString();
				if (id == theme->id()) {
					d->themeView->setCurrentIndex(index);
				}
				const QString themeName = themesModel->data(index, PsiThemeModel::TitleRole).toString();
				bool enabled = (id == provider->current()->id());
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

				QRadioButton *status = new QRadioButton(d->themeView);
				status->setObjectName(RADIO_PREFIX + id);
				status->setChecked(enabled);
				status->setAutoExclusive(false);
				status->setText(themeName);
				connect(status, SIGNAL(toggled(bool)), this, SLOT(radioToggled(bool)));

				QWidget *itemWidget = new QWidget(d->themeView);

				QBoxLayout *box = new QBoxLayout(QBoxLayout::LeftToRight, d->themeView);
				box->addWidget(status);
				box->addStretch();
				box->addWidget(typeLabel);
				box->addWidget(screenshotButton);
				itemWidget->setLayout(box);

				d->themeView->setIndexWidget(index, itemWidget);
			}
			d->themeView->sortByColumn(0);
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

		const int row = themesModel->themeRow(getThemeId(btn->objectName()));
		const QModelIndex index = themesModel->index(row);
		const QString name_ = themesModel->data(index, PsiThemeModel::TitleRole).toString();
		const QPixmap scr = themesModel->data(index, PsiThemeModel::ScreenshotRole).value<QPixmap>();

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

void OptionsTabAppearanceTheme::radioToggled(bool toggled)
{
	if ( !w  || !sender()->inherits("QRadioButton"))
		return;

	OptAppearanceThemeUI *d = (OptAppearanceThemeUI *)w;
	QRadioButton *btn = static_cast<QRadioButton*>(sender());
	const QString btnObjectName = btn->objectName();
	if (btn && toggled) {
		const QModelIndex index = themesModel->index(themesModel->themeRow(getThemeId(btnObjectName)));
		if (index != d->themeView->currentIndex()) {
			d->themeView->setCurrentIndex(index);
		}
	}
	uncheckAll(btnObjectName);
}

void OptionsTabAppearanceTheme::uncheckAll(const QString &name_)
{
	if ( !w )
		return;

	OptAppearanceThemeUI *d = (OptAppearanceThemeUI *)w;
	const QList<QRadioButton*> buttons = d->themeView->findChildren<QRadioButton*>(QRegExp(RADIO_PREFIX"*"));
	if (!buttons.isEmpty()) {
		foreach (QRadioButton *b, buttons) {
			if (b->objectName() != name_) {
				b->setChecked(false);
			}
		}
	}
}

void OptionsTabAppearanceTheme::itemChanged(const QModelIndex &current, const QModelIndex &previous)
{
	if ( !w )
		return;

	Q_UNUSED(previous)
	OptAppearanceThemeUI *d = (OptAppearanceThemeUI *)w;
	const QString id = themesModel->data(current, PsiThemeModel::IdRole).toString();
	if (!id.isEmpty()) {
		QRadioButton *btn = d->themeView->findChild<QRadioButton*>(QString("%1%2").arg(RADIO_PREFIX).arg(id));
		if (btn) {
			uncheckAll(btn->objectName());
			btn->setChecked(!btn->isChecked());
		}
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
	const QString id = d->themeView->currentIndex().data(PsiThemeModel::IdRole).toString();
	if (!id.isEmpty()) {
		provider->setCurrentTheme(id);
	}
}

void OptionsTabAppearanceTheme::restoreOptions()
{
	if ( !w )
		return;

	OptAppearanceThemeUI *d = (OptAppearanceThemeUI *)w;

	Theme *theme = provider->current();
	d->themeView->setCurrentIndex(themesModel->index( themesModel->themeRow(theme?theme->id():"") ));
}
