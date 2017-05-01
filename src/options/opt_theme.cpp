#include "opt_theme.h"

#include "ui_opt_theme.h"
#include "psioptions.h"
#include "psithememodel.h"
#include "psithememanager.h"
#include "psiiconset.h"

#include <QSortFilterProxyModel>
#include <QTimer>
#include <QBuffer>

#define LABEL_PREFIX "tl_"

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
	themesModel->setSortRole(PsiThemeModel::TitleRole);

	d->themeView->setModel(themesModel);
	d->themeView->setSortingEnabled(true);

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
	Q_UNUSED(current)
	if (!previous.isValid()) {
		return; // Psi won't start if it's impossible to load any theme. So we always have previous.
	}
	emit dataChanged();
}

void OptionsTabAppearanceTheme::modelRowsInserted(const QModelIndex &parent, int first, int last)
{
	if (!parent.isValid() || !w) {
		OptAppearanceThemeUI *d = (OptAppearanceThemeUI *)w;
		for (int i = first; i <= last; i++) {
			const QModelIndex index = themesModel->index(i, 0);
			const QString id = themesModel->data(index, PsiThemeModel::IdRole).toString();
			if (themesModel->data(index, PsiThemeModel::IsCurrent).toBool()) {
				d->themeView->setCurrentIndex(index);
			}
			const QString themeName = themesModel->data(index, PsiThemeModel::TitleRole).toString();
			bool isPsi = id.startsWith("psi");
			QPixmap client = isPsi ? IconsetFactory::iconPixmap("clients/psi")
					       : IconsetFactory::iconPixmap("clients/adium");
			const QString clientName = isPsi ? tr("Psi Theme") : tr("Adium Theme");

			QLabel *typeLabel = new QLabel(d->themeView);
			typeLabel->setObjectName(LABEL_PREFIX + id);
			QString itemContents(themeName);
			if (!client.isNull()) {
				itemContents =QString("<table width='100%' border='0'><tr>"
						      "<td align='center' width='22'><img src='data:image/png;base64, %0'>"
						      "</td><td align='left'>%1</td>"
						      "</tr></table>").arg(getPixmapBytes(client)).arg(themeName);
			}
			typeLabel->setText(itemContents);
			QString html(clientName);
			QPixmap scr = unsortedModel->data(index, PsiThemeModel::ScreenshotRole).value<QPixmap>();
			if (!scr.isNull()) {
				const QString desc = tr("Screenshot of %1 theme").arg(themeName);
				html = QString("<table border='0'><tr><td align='center'>"
					       "<b>%0</b>"
					       "</td></tr><tr><td><img src='data:image/png;base64, %1'></td>"
					       "</tr></table>").arg(desc).arg(getPixmapBytes(scr));
			}
			typeLabel->setToolTip(html);
			d->themeView->setIndexWidget(index, typeLabel);
		}
		themesModel->sort(0);
	}
}

QString OptionsTabAppearanceTheme::getPixmapBytes(const QPixmap &image) const
{
	QByteArray imageData;
	QBuffer imageBuffer(&imageData);
	imageBuffer.open(QIODevice::WriteOnly);
	image.save(&imageBuffer, "PNG");
	return QString(imageData.toBase64());

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
