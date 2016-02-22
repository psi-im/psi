#include "opt_iconset.h"
#include "common.h"
#include "iconwidget.h"
#include "applicationinfo.h"
#include "psiiconset.h"
#include "psicon.h"
#include "psioptions.h"

#include <QDir>
#include <QCheckBox>
#include <QRadioButton>
#include <QComboBox>
#include <QLineEdit>
#include <QFileInfo>
#include <QApplication>
#include <QThread>
#include <QMutex>
#include <QEvent>
#include <QCursor>
#include <QPalette>
#include <QTabWidget>
#include <QFont>
#include <QTreeWidget>

#include "ui_opt_iconset_emo.h"
#include "ui_opt_iconset_mood.h"
#include "ui_opt_iconset_activity.h"
#include "ui_opt_iconset_client.h"
#include "ui_opt_iconset_system.h"
#include "ui_opt_iconset_roster.h"
#include "ui_opt_iconset_affiliation.h"
#include "ui_ui_isdetails.h"

class IconsetEmoUI : public QWidget, public Ui::IconsetEmo
{
public:
	IconsetEmoUI() : QWidget() { setupUi(this); }
};

class IconsetMoodUI : public QWidget, public Ui::IconsetMood
{
public:
	IconsetMoodUI() : QWidget() { setupUi(this); }
};

class IconsetClientUI : public QWidget, public Ui::IconsetClient
{
public:
	IconsetClientUI() : QWidget() { setupUi(this); }
};

class IconsetActivityUI : public QWidget, public Ui::IconsetActivity
{
public:
	IconsetActivityUI() : QWidget() { setupUi(this); }
};

class IconsetSystemUI : public QWidget, public Ui::IconsetSystem
{
public:
	IconsetSystemUI() : QWidget() { setupUi(this); }
};

class IconsetAffiliationUI : public QWidget, public Ui::IconsetAffiliation
{
  public:
    IconsetAffiliationUI() : QWidget() { setupUi(this); }
};

class IconsetRosterUI : public QWidget, public Ui::IconsetRoster
{
public:
	IconsetRosterUI() : QWidget() { setupUi(this); }
};

class IconsetDetailsDlg : public QDialog, public Ui::IconsetDetailsDlg
{
public:
	IconsetDetailsDlg(PsiCon *psicon, QWidget* parent, const char* name, bool modal)
		: QDialog(parent)
		, psi(psicon)
	{
		setAttribute(Qt::WA_DeleteOnClose);
		setupUi(this);

		psi->dialogRegister(this);

		QStringList bold_labels;
		bold_labels << "lb_name2";
		bold_labels << "lb_version2";
		bold_labels << "lb_date2";
		bold_labels << "lb_home2";
		bold_labels << "lb_desc2";
		bold_labels << "lb_authors";

		QList<QLabel *> labels = findChildren<QLabel *>();
		foreach (QLabel *l, labels) {
			if (bold_labels.contains(l->objectName())) {
				QFont font = l->font();
				font.setBold(true);
				l->setFont(font);
			}
		}

		setObjectName(name);
		setModal(modal);
	}

	~IconsetDetailsDlg()
	{
		psi->dialogUnregister(this);
	}

	void setIconset( const Iconset &is ) {
		setWindowTitle(windowTitle().arg(is.name()));

		lb_name->setText(is.name());
		lb_version->setText(is.version());

		if ( is.description().isEmpty() ) {
			lb_desc->hide();
			lb_desc2->hide();
		}
		else
			lb_desc->setText(is.description());

		if ( !is.homeUrl().isEmpty() ) {
			lb_home->setTitle(is.homeUrl());
			lb_home->setUrl(is.homeUrl());
		}
		else {
			lb_home->hide();
			lb_home2->hide();
		}

		if ( is.creation().isEmpty() ) {
			lb_date->hide();
			lb_date2->hide();
		}
		else {
			QDate date = QDate::fromString(is.creation(), Qt::ISODate);
			lb_date->setText(date.toString(Qt::LocalDate));
		}

		if ( is.authors().count() == 0 ) {
			ptv_authors->hide();
			lb_authors->hide();
		}
		else {
			QString authors;
			QStringList::ConstIterator it = is.authors().begin();
			for ( ; it != is.authors().end(); ++it) {
				if ( !authors.isEmpty() )
					authors += "<br><br>";
				authors += *it;
			}
			ptv_authors->setText( "<qt><nobr>" + authors + "</nobr></qt>" );
		}

		isd_iconset->setIconset(is);

		resize(sizeHint());
	}

private:
	PsiCon *psi;
};

static void isDetails(const Iconset &is, QWidget *parent, PsiCon *psi)
{
	IconsetDetailsDlg *isd = new IconsetDetailsDlg(psi, parent, "IconsetDetailsDlg", false);
	isd->setIconset(is);
	isd->show();
}

static int countIconsets(QString addDir, QStringList excludeList)
{
	int count = 0;

	foreach (const QString &dataDir,  ApplicationInfo::dataDirs()) {
		QDir dir (dataDir + "/iconsets" + addDir);

		foreach (const QFileInfo &fi,
				 dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries))
		{
			QString iconsetId = fi.absoluteFilePath().section('/', -2);
			if ( excludeList.contains(iconsetId) || !Iconset::isSourceAllowed(fi))
				continue;

			count++;
			excludeList << iconsetId;
		}
	}

	return count;
}

//----------------------------------------------------------------------------
// IconsetLoadEvent
//----------------------------------------------------------------------------

class IconsetLoadEvent : public QEvent
{
public:
	IconsetLoadEvent(IconsetLoadThread *par, Iconset *i)
		: QEvent(QEvent::User)
		, p(par)
		, is(i)
	{
	}

	IconsetLoadThread *thread() const { return p; }

	// if iconset() is '0' then it means that iconset wasn't loaded successfully
	Iconset *iconset() const { return is; }

private:
	IconsetLoadThread *p;
	Iconset *is;
};

//----------------------------------------------------------------------------
// IconsetFinishEvent
//----------------------------------------------------------------------------

class IconsetFinishEvent : public QEvent
{
public:
	IconsetFinishEvent()
		: QEvent( (QEvent::Type)(QEvent::User + 1) )
	{
	}
};


//----------------------------------------------------------------------------
// IconsetLoadThreadDestroyEvent
//----------------------------------------------------------------------------

class IconsetLoadThreadDestroyEvent : public QEvent
{
public:
	IconsetLoadThreadDestroyEvent(QThread *t)
		: QEvent( (QEvent::Type)(QEvent::User + 2) )
		, thread(t)
	{
	}

	~IconsetLoadThreadDestroyEvent()
	{
		thread->wait();
		delete thread;
	}

private:
	QThread *thread;
};

//----------------------------------------------------------------------------
// IconsetLoadThread
//----------------------------------------------------------------------------

class IconsetLoadThread : public QThread
{
public:
	IconsetLoadThread(QObject *parent, QString addPath);
	void excludeIconsets(QStringList);

	bool cancelled;

protected:
	void run();
	void postEvent(QEvent *);

private:
	QObject *parent;
	QString addPath;
	QStringList excludeList;
};

IconsetLoadThread::IconsetLoadThread(QObject *p, QString path)
	: cancelled(false)
	, parent(p)
	, addPath(path)
{
}

void IconsetLoadThread::excludeIconsets(QStringList l)
{
	excludeList += l;
}

static QMutex threadCancelled, threadMutex;

void IconsetLoadThread::postEvent(QEvent *e)
{
	threadCancelled.lock();
	bool cancel = cancelled;
	threadCancelled.unlock();

	if ( cancel ) {
		delete e;
		return;
	}

	QApplication::postEvent(parent, e);
}

void IconsetLoadThread::run()
{
	threadMutex.lock();
	QStringList dirs = ApplicationInfo::dataDirs();
	threadMutex.unlock();

	QStringList failedList;
	foreach (const QString &dataDir, dirs) {
		QDir dir(dataDir + "/iconsets" + addPath);
		foreach (const QFileInfo &iconsetFI,
				 dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries))
		{
			if (!Iconset::isSourceAllowed(iconsetFI))
				continue;

			threadCancelled.lock();
			bool cancel = cancelled;
			threadCancelled.unlock();

			if ( cancel )
				goto getout;

			QString iconsetId = iconsetFI.absoluteFilePath().section('/', -2);
			if (excludeList.contains(iconsetId))
				continue;

			Iconset *is = new Iconset;

			if ( is->load (iconsetFI.absoluteFilePath()) ) {
				failedList.removeOne(is->id());
				excludeList << is->id();

				// don't forget to delete iconset in ::event()!
				postEvent(new IconsetLoadEvent(this, is));
			}
			else {
				delete is;
				failedList << iconsetId;
			}
		}
	}

	for(int i = 0; i < failedList.size(); i++) {
		postEvent(new IconsetLoadEvent(this, 0));
	}

getout:
	postEvent(new IconsetFinishEvent());
	QApplication::postEvent(qApp, new IconsetLoadThreadDestroyEvent(this)); // self destruct
}

//----------------------------------------------------------------------------
// OptionsTabIconsetSystem
//----------------------------------------------------------------------------

OptionsTabIconsetSystem::OptionsTabIconsetSystem(QObject *parent)
	: OptionsTab(parent, "iconset_system", "", tr("System Icons"), tr("Select the system iconset"))
	, w(0)
	, thread(0)
{
}

OptionsTabIconsetSystem::~OptionsTabIconsetSystem()
{
	cancelThread();
}

QWidget *OptionsTabIconsetSystem::widget()
{
	if ( w )
		return 0;

	w = new IconsetSystemUI;
	IconsetSystemUI *d = (IconsetSystemUI *)w;

	connect(d->pb_sysDetails, SIGNAL(clicked()), SLOT(previewIconset()));

	// TODO: add QWhatsThis

	return w;
}

void OptionsTabIconsetSystem::applyOptions()
{
	if ( !w || thread )
		return;

	IconsetSystemUI *d = (IconsetSystemUI *)w;
	const Iconset *is = d->iss_system->iconset();
	if ( is ) {
		QFileInfo fi( is->fileName() );
		PsiOptions::instance()->setOption("options.iconsets.system", fi.fileName());
	}
}

void OptionsTabIconsetSystem::restoreOptions()
{
	if ( !w || thread )
		return;

	IconsetSystemUI *d = (IconsetSystemUI *)w;
	d->iss_system->clear();
	QStringList loaded;

	d->setCursor(Qt::WaitCursor);

	d->iss_system->setEnabled(false);
	d->pb_sysDetails->setEnabled(false);

	d->progress->show();
	d->progress->setValue( 0 );

	numIconsets = countIconsets("/system", loaded);
	iconsetsLoaded = 0;

	cancelThread();

	thread = new IconsetLoadThread(this, "/system");
	thread->start();
}

bool OptionsTabIconsetSystem::event(QEvent *e)
{
	IconsetSystemUI *d = (IconsetSystemUI *)w;
	if ( e->type() == QEvent::User ) { // iconset load event
		IconsetLoadEvent *le = (IconsetLoadEvent *)e;

		if ( thread != le->thread() )
			return false;

		if ( !numIconsets )
			numIconsets = 1;
		d->progress->setValue( (int)((float)100 * ++iconsetsLoaded / numIconsets) );

		Iconset *i = le->iconset();
		if ( i ) {
			PsiIconset::instance()->stripFirstAnimFrame(i);
			d->iss_system->insert(*i);

			QFileInfo fi( i->fileName() );
			if ( fi.fileName() == PsiOptions::instance()->getOption("options.iconsets.system").toString() )
				d->iss_system->setItemSelected(d->iss_system->lastItem(), true);

			delete i;
		}

		return true;
	}
	else if ( e->type() == QEvent::User + 1 ) { // finish event
		d->iss_system->setEnabled(true);
		d->pb_sysDetails->setEnabled(true);

		connect(d->iss_system, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SIGNAL(dataChanged()));

		d->progress->hide();

		d->unsetCursor();
		thread = 0;

		return true;
	}

	return false;
}

void OptionsTabIconsetSystem::previewIconset()
{
	IconsetSystemUI *d = (IconsetSystemUI *)w;
	const Iconset *is = d->iss_system->iconset();
	if (is) {
		isDetails(*is, parentWidget->parentWidget(), psi);
	}
}

void OptionsTabIconsetSystem::setData(PsiCon *psicon, QWidget *p)
{
	psi = psicon;
	parentWidget = p;
}

void OptionsTabIconsetSystem::cancelThread()
{
	if ( thread ) {
		threadCancelled.lock();
		thread->cancelled = true;
		threadCancelled.unlock();

		thread = 0;
	}
}

//----------------------------------------------------------------------------
// OptionsTabIconsetEmoticons
//----------------------------------------------------------------------------

OptionsTabIconsetEmoticons::OptionsTabIconsetEmoticons(QObject *parent)
	: OptionsTab(parent, "iconset_emoticons", "", tr("Emoticons"), tr("Select your emoticon iconsets"))
	, w(0)
	, thread(0)
{
}

OptionsTabIconsetEmoticons::~OptionsTabIconsetEmoticons()
{
	cancelThread();
}

QWidget *OptionsTabIconsetEmoticons::widget()
{
	if ( w )
		return 0;

	w = new IconsetEmoUI;
	IconsetEmoUI *d = (IconsetEmoUI *)w;

	connect(d->pb_emoUp, SIGNAL(clicked()), d->iss_emoticons, SLOT(moveItemUp()));
	connect(d->pb_emoUp, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->pb_emoDown, SIGNAL(clicked()), d->iss_emoticons, SLOT(moveItemDown()));
	connect(d->pb_emoDown, SIGNAL(clicked()), SIGNAL(dataChanged()));
	connect(d->pb_emoDetails, SIGNAL(clicked()), SLOT(previewIconset()));

	d->ck_useEmoticons->setWhatsThis(
		tr("<P>Emoticons are short sequences of characters that are used to convey an emotion or idea.</P>"
		"<P>Enable this option if you want Psi to replace common emoticons with a graphical image.</P>"
		"<P>For example, <B>:-)</B> would be replaced by <icon name=\"psi/smile\"></P>"));

	// TODO: add QWhatsThis

	return w;
}

void OptionsTabIconsetEmoticons::applyOptions()
{
	if ( !w || thread )
		return;

	IconsetEmoUI *d = (IconsetEmoUI *)w;
	PsiOptions::instance()->setOption("options.ui.emoticons.use-emoticons", d->ck_useEmoticons->isChecked());

	QStringList list;
	for (int row = 0; row < d->iss_emoticons->count(); row++) {
		IconWidgetItem *item = (IconWidgetItem *)d->iss_emoticons->item(row);

		if ( d->iss_emoticons->isItemSelected(item) ) {
			const Iconset *is = item->iconset();
			if ( is ) {
				QFileInfo fi( is->fileName() );
				list << fi.fileName();
			}
		}
	}
	PsiOptions::instance()->setOption("options.iconsets.emoticons", list);
}

void OptionsTabIconsetEmoticons::restoreOptions()
{
	if ( !w || thread )
		return;

	IconsetEmoUI *d = (IconsetEmoUI *)w;
	d->ck_useEmoticons->setChecked( PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool() );

	// fill in the iconset view
	d->iss_emoticons->clear();

	{
		foreach(Iconset* is, PsiIconset::instance()->emoticons) {
			d->iss_emoticons->insert(*is);
			d->iss_emoticons->setItemSelected(d->iss_emoticons->lastItem(), true);
		}
	}


	{
		QStringList loaded;
		{
			foreach(Iconset* tmp, PsiIconset::instance()->emoticons) {
				loaded << tmp->id();
			}
		}

		d->setCursor(Qt::WaitCursor);

		d->ck_useEmoticons->setEnabled(false);
		d->groupBox9->setEnabled(false);

		d->progress->show();
		d->progress->setValue( 0 );

		numIconsets = countIconsets("/emoticons", loaded);
		iconsetsLoaded = 0;

		cancelThread();

		thread = new IconsetLoadThread(this, "/emoticons");
		thread->excludeIconsets(loaded);
		thread->start();
	}
}

bool OptionsTabIconsetEmoticons::event(QEvent *e)
{
	IconsetEmoUI *d = (IconsetEmoUI *)w;
	if ( e->type() == QEvent::User ) { // iconset load event
		IconsetLoadEvent *le = (IconsetLoadEvent *)e;

		if ( thread != le->thread() )
			return false;

		if ( !numIconsets )
			numIconsets = 1;
		d->progress->setValue( (int)((float)100 * ++iconsetsLoaded / numIconsets) );

		Iconset *is = le->iconset();
		if ( is ) {
			PsiIconset::removeAnimation(is);
			d->iss_emoticons->insert(*is);
			delete is;
		}

		return true;
	}
	else if ( e->type() == QEvent::User + 1 ) { // finish event
		d->ck_useEmoticons->setEnabled(true);
		d->groupBox9->setEnabled(true);

		connect(d->iss_emoticons, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SIGNAL(dataChanged()));

		bool checked = d->ck_useEmoticons->isChecked();
		d->ck_useEmoticons->setChecked( true );
		d->ck_useEmoticons->setChecked( checked );

		d->progress->hide();

		d->unsetCursor();
		thread = 0;

		return true;
	}

	return false;
}

void OptionsTabIconsetEmoticons::previewIconset()
{
	IconsetEmoUI *d = (IconsetEmoUI *)w;
	const Iconset *is = d->iss_emoticons->iconset();
	if (is) {
		isDetails(*is, parentWidget->parentWidget(), psi);
	}
}

void OptionsTabIconsetEmoticons::setData(PsiCon *psicon, QWidget *p)
{
	psi = psicon;
	parentWidget = p;
}

void OptionsTabIconsetEmoticons::cancelThread()
{
	if ( thread ) {
		threadCancelled.lock();
		thread->cancelled = true;
		threadCancelled.unlock();

		thread = 0;
	}
}


//----------------------------------------------------------------------------
// OptionsTabIconsetMoods
//----------------------------------------------------------------------------

OptionsTabIconsetMoods::OptionsTabIconsetMoods(QObject *parent)
	: OptionsTab(parent, "iconset_moods", "", tr("Moods"), tr("Select your mood iconset"))
	, w(0)
	, thread(0)
{
}

OptionsTabIconsetMoods::~OptionsTabIconsetMoods()
{
	cancelThread();
}

QWidget *OptionsTabIconsetMoods::widget()
{
	if ( w )
		return 0;

	w = new IconsetMoodUI;
	IconsetMoodUI *d = (IconsetMoodUI *)w;

	connect(d->pb_moodDetails, SIGNAL(clicked()), SLOT(previewIconset()));

	return w;
}

void OptionsTabIconsetMoods::applyOptions()
{
	if ( !w || thread )
		return;

	IconsetMoodUI *d = (IconsetMoodUI *)w;

	const Iconset *is = d->iss_moods->iconset();
	if ( is ) {
		QFileInfo fi( is->fileName() );
		PsiOptions::instance()->setOption("options.iconsets.moods", fi.fileName());
	}
}

void OptionsTabIconsetMoods::restoreOptions()
{
	if ( !w || thread )
		return;

	IconsetMoodUI *d = (IconsetMoodUI *)w;

	d->iss_moods->clear();
	QStringList loaded;

	d->setCursor(Qt::WaitCursor);

	QPalette customPal = d->palette();
	customPal.setCurrentColorGroup(QPalette::Inactive);
	d->iss_moods->setEnabled(false);
	d->iss_moods->setPalette(customPal);

	d->pb_moodDetails->setEnabled(false);
	d->pb_moodDetails->setPalette(customPal);

	d->progress->show();
	d->progress->setValue( 0 );

	numIconsets = countIconsets("/moods", loaded);
	iconsetsLoaded = 0;

	cancelThread();

	thread = new IconsetLoadThread(this, "/moods");
	thread->start();
}

bool OptionsTabIconsetMoods::event(QEvent *e)
{
	IconsetMoodUI *d = (IconsetMoodUI *)w;
	if ( e->type() == QEvent::User ) { // iconset load event
		IconsetLoadEvent *le = (IconsetLoadEvent *)e;

		if ( thread != le->thread() )
			return false;

		if ( !numIconsets )
			numIconsets = 1;
		d->progress->setValue( (int)((float)100 * ++iconsetsLoaded / numIconsets) );

		Iconset *i = le->iconset();
		if ( i ) {
			PsiIconset::instance()->stripFirstAnimFrame(i);
			d->iss_moods->insert(*i);

			QFileInfo fi( i->fileName() );
			if ( fi.fileName() == PsiOptions::instance()->getOption("options.iconsets.moods").toString() )
				d->iss_moods->setItemSelected(d->iss_moods->lastItem(), true);

			delete i;
		}

		return true;
	}
	else if ( e->type() == QEvent::User + 1 ) { // finish event
		d->iss_moods->setEnabled(true);
		d->iss_moods->setPalette(QPalette());

		d->pb_moodDetails->setEnabled(true);
		d->pb_moodDetails->setPalette(QPalette());

		connect(d->iss_moods, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SIGNAL(dataChanged()));

		d->progress->hide();

		d->unsetCursor();
		thread = 0;

		return true;
	}

	return false;
}

void OptionsTabIconsetMoods::previewIconset()
{
	IconsetMoodUI *d = (IconsetMoodUI *)w;
	const Iconset *is = d->iss_moods->iconset();
	if (is) {
		isDetails(*is, parentWidget->parentWidget(), psi);
	}
}

void OptionsTabIconsetMoods::setData(PsiCon *psicon, QWidget *p)
{
	psi = psicon;
	parentWidget = p;
}

void OptionsTabIconsetMoods::cancelThread()
{
	if ( thread ) {
		threadCancelled.lock();
		thread->cancelled = true;
		threadCancelled.unlock();

		thread = 0;
	}
}
//----------------------------------------------------------------------------
// OptionsTabIconsetActivity
//----------------------------------------------------------------------------

OptionsTabIconsetActivity::OptionsTabIconsetActivity(QObject *parent)
	: OptionsTab(parent, "iconset_activities", "", tr("Activity"), tr("Select your activity iconset"))
	, w(0)
	, thread(0)
{
}

OptionsTabIconsetActivity::~OptionsTabIconsetActivity()
{
	cancelThread();
}

QWidget *OptionsTabIconsetActivity::widget()
{
	if ( w )
		return 0;

	w = new IconsetActivityUI;
	IconsetActivityUI *d = (IconsetActivityUI *)w;

	connect(d->pb_activityDetails, SIGNAL(clicked()), SLOT(previewIconset()));

	return w;
}

void OptionsTabIconsetActivity::applyOptions()
{
	if ( !w || thread )
		return;

	IconsetActivityUI *d = (IconsetActivityUI *)w;

	const Iconset *is = d->iss_activity->iconset();
	if ( is ) {
		QFileInfo fi( is->fileName() );
		PsiOptions::instance()->setOption("options.iconsets.activities", fi.fileName());
	}
}

void OptionsTabIconsetActivity::restoreOptions()
{
	if ( !w || thread )
		return;

	IconsetActivityUI *d = (IconsetActivityUI *)w;

	d->iss_activity->clear();
	QStringList loaded;

	d->setCursor(Qt::WaitCursor);

	QPalette customPal = d->palette();
	customPal.setCurrentColorGroup(QPalette::Inactive);
	d->iss_activity->setEnabled(false);
	d->iss_activity->setPalette(customPal);

	d->pb_activityDetails->setEnabled(false);
	d->pb_activityDetails->setPalette(customPal);

	d->progress->show();
	d->progress->setValue( 0 );

	numIconsets = countIconsets("/activities", loaded);
	iconsetsLoaded = 0;

	cancelThread();

	thread = new IconsetLoadThread(this, "/activities");
	thread->start();
}

bool OptionsTabIconsetActivity::event(QEvent *e)
{
	IconsetActivityUI *d = (IconsetActivityUI *)w;
	if ( e->type() == QEvent::User ) { // iconset load event
		IconsetLoadEvent *le = (IconsetLoadEvent *)e;

		if ( thread != le->thread() )
			return false;

		if ( !numIconsets )
			numIconsets = 1;
		d->progress->setValue( (int)((float)100 * ++iconsetsLoaded / numIconsets) );

		Iconset *i = le->iconset();
		if ( i ) {
			PsiIconset::instance()->stripFirstAnimFrame(i);
			d->iss_activity->insert(*i);

			QFileInfo fi( i->fileName() );
			if ( fi.fileName() == PsiOptions::instance()->getOption("options.iconsets.activities").toString() )
				d->iss_activity->setItemSelected(d->iss_activity->lastItem(), true);

			delete i;
		}

		return true;
	}
	else if ( e->type() == QEvent::User + 1 ) { // finish event
		d->iss_activity->setEnabled(true);
		d->iss_activity->setPalette(QPalette());

		d->pb_activityDetails->setEnabled(true);
		d->pb_activityDetails->setPalette(QPalette());

		connect(d->iss_activity, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SIGNAL(dataChanged()));

		d->progress->hide();

		d->unsetCursor();
		thread = 0;

		return true;
	}

	return false;
}

void OptionsTabIconsetActivity::previewIconset()
{
	IconsetActivityUI *d = (IconsetActivityUI *)w;
	const Iconset *is = d->iss_activity->iconset();
	if (is) {
		isDetails(*is, parentWidget->parentWidget(), psi);
	}
}

void OptionsTabIconsetActivity::setData(PsiCon *psicon, QWidget *p)
{
	psi = psicon;
	parentWidget = p;
}

void OptionsTabIconsetActivity::cancelThread()
{
	if ( thread ) {
		threadCancelled.lock();
		thread->cancelled = true;
		threadCancelled.unlock();

		thread = 0;
	}
}
//----------------------------------------------------------------------------
// OptionsTabIconsetAffiliations
//----------------------------------------------------------------------------

OptionsTabIconsetAffiliations::OptionsTabIconsetAffiliations(QObject *parent)
	: OptionsTab(parent, "iconset_affiliations", "", tr("Affiliations"), tr("Select your affiliations iconset"))
	, w(0)
	, thread(0)
{
}

OptionsTabIconsetAffiliations::~OptionsTabIconsetAffiliations()
{
	cancelThread();
}

QWidget *OptionsTabIconsetAffiliations::widget()
{
	if ( w )
		return 0;

	w = new IconsetAffiliationUI;
	IconsetAffiliationUI *d = (IconsetAffiliationUI *)w;

	connect(d->pb_affiliationDetails, SIGNAL(clicked()), SLOT(previewIconset()));

	return w;
}

void OptionsTabIconsetAffiliations::applyOptions()
{
	if ( !w || thread )
		return;

	IconsetAffiliationUI *d = (IconsetAffiliationUI *)w;

	const Iconset *is = d->iss_affiliation->iconset();
	if ( is ) {
		QFileInfo fi( is->fileName() );
		PsiOptions::instance()->setOption("options.iconsets.affiliations", fi.fileName());
	}
}

void OptionsTabIconsetAffiliations::restoreOptions()
{
	if ( !w || thread )
		return;

	IconsetAffiliationUI *d = (IconsetAffiliationUI *)w;

	d->iss_affiliation->clear();
	QStringList loaded;

	d->setCursor(Qt::WaitCursor);

	QPalette customPal = d->palette();
	customPal.setCurrentColorGroup(QPalette::Inactive);
	d->iss_affiliation->setEnabled(false);
	d->iss_affiliation->setPalette(customPal);

	d->pb_affiliationDetails->setEnabled(false);
	d->pb_affiliationDetails->setPalette(customPal);

	d->progress->show();
	d->progress->setValue( 0 );

	numIconsets = countIconsets("/affiliations", loaded);
	iconsetsLoaded = 0;

	cancelThread();

	thread = new IconsetLoadThread(this, "/affiliations");
	thread->start();
}

bool OptionsTabIconsetAffiliations::event(QEvent *e)
{
	IconsetAffiliationUI *d = (IconsetAffiliationUI *)w;
	if ( e->type() == QEvent::User ) { // iconset load event
		IconsetLoadEvent *le = (IconsetLoadEvent *)e;

		if ( thread != le->thread() )
			return false;

		if ( !numIconsets )
			numIconsets = 1;
		d->progress->setValue( (int)((float)100 * ++iconsetsLoaded / numIconsets) );

		Iconset *i = le->iconset();
		if ( i ) {
			PsiIconset::instance()->stripFirstAnimFrame(i);
			d->iss_affiliation->insert(*i);

			QFileInfo fi( i->fileName() );
			if ( fi.fileName() == PsiOptions::instance()->getOption("options.iconsets.affiliations").toString() )
				d->iss_affiliation->setItemSelected(d->iss_affiliation->lastItem(), true);

			delete i;
		}

		return true;
	}
	else if ( e->type() == QEvent::User + 1 ) { // finish event
		d->iss_affiliation->setEnabled(true);
		d->iss_affiliation->setPalette(QPalette());

		d->pb_affiliationDetails->setEnabled(true);
		d->pb_affiliationDetails->setPalette(QPalette());

		connect(d->iss_affiliation, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SIGNAL(dataChanged()));

		d->progress->hide();

		d->unsetCursor();
		thread = 0;

		return true;
	}

	return false;
}

void OptionsTabIconsetAffiliations::previewIconset()
{
	IconsetAffiliationUI *d = (IconsetAffiliationUI *)w;
	const Iconset *is = d->iss_affiliation->iconset();
	if (is) {
		isDetails(*is, parentWidget->parentWidget(), psi);
	}
}

void OptionsTabIconsetAffiliations::setData(PsiCon *psicon, QWidget *p)
{
	psi = psicon;
	parentWidget = p;
}

void OptionsTabIconsetAffiliations::cancelThread()
{
	if ( thread ) {
		threadCancelled.lock();
		thread->cancelled = true;
		threadCancelled.unlock();

		thread = 0;
	}
}

//----------------------------------------------------------------------------
// OptionsTabIconsetClients
//----------------------------------------------------------------------------

OptionsTabIconsetClients::OptionsTabIconsetClients(QObject *parent)
	: OptionsTab(parent, "iconset_clients", "", tr("Clients"), tr("Select your clients iconset"))
	, w(0)
	, thread(0)
{
}

OptionsTabIconsetClients::~OptionsTabIconsetClients()
{
	cancelThread();
}

QWidget *OptionsTabIconsetClients::widget()
{
	if ( w )
		return 0;

	w = new IconsetClientUI;
	IconsetClientUI *d = (IconsetClientUI *)w;

	connect(d->pb_clientDetails, SIGNAL(clicked()), SLOT(previewIconset()));

	return w;
}

void OptionsTabIconsetClients::applyOptions()
{
	if ( !w || thread )
		return;

	IconsetClientUI *d = (IconsetClientUI *)w;

	const Iconset *is = d->iss_clients->iconset();
	if ( is ) {
		QFileInfo fi( is->fileName() );
		PsiOptions::instance()->setOption("options.iconsets.clients", fi.fileName());
	}
}

void OptionsTabIconsetClients::restoreOptions()
{
	if ( !w || thread )
		return;

	IconsetClientUI *d = (IconsetClientUI *)w;

	d->iss_clients->clear();
	QStringList loaded;

	d->setCursor(Qt::WaitCursor);

	QPalette customPal = d->palette();
	customPal.setCurrentColorGroup(QPalette::Inactive);
	d->iss_clients->setEnabled(false);
	d->iss_clients->setPalette(customPal);

	d->pb_clientDetails->setEnabled(false);
	d->pb_clientDetails->setPalette(customPal);

	d->progress->show();
	d->progress->setValue( 0 );

	numIconsets = countIconsets("/clients", loaded);
	iconsetsLoaded = 0;

	cancelThread();

	thread = new IconsetLoadThread(this, "/clients");
	thread->start();
}

bool OptionsTabIconsetClients::event(QEvent *e)
{
	IconsetClientUI *d = (IconsetClientUI *)w;
	if ( e->type() == QEvent::User ) { // iconset load event
		IconsetLoadEvent *le = (IconsetLoadEvent *)e;

		if ( thread != le->thread() )
			return false;

		if ( !numIconsets )
			numIconsets = 1;
		d->progress->setValue( (int)((float)100 * ++iconsetsLoaded / numIconsets) );

		Iconset *i = le->iconset();
		if ( i ) {
			PsiIconset::instance()->stripFirstAnimFrame(i);
			d->iss_clients->insert(*i);

			QFileInfo fi( i->fileName() );
			if ( fi.fileName() == PsiOptions::instance()->getOption("options.iconsets.clients").toString() )
				d->iss_clients->setItemSelected(d->iss_clients->lastItem(), true);

			delete i;
		}

		return true;
	}
	else if ( e->type() == QEvent::User + 1 ) { // finish event
		d->iss_clients->setEnabled(true);
		d->iss_clients->setPalette(QPalette());

		d->pb_clientDetails->setEnabled(true);
		d->pb_clientDetails->setPalette(QPalette());

		connect(d->iss_clients, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SIGNAL(dataChanged()));

		d->progress->hide();

		d->unsetCursor();
		thread = 0;

		return true;
	}

	return false;
}

void OptionsTabIconsetClients::previewIconset()
{
	IconsetClientUI *d = (IconsetClientUI *)w;
	const Iconset *is = d->iss_clients->iconset();
	if (is) {
		isDetails(*is, parentWidget->parentWidget(), psi);
	}
}

void OptionsTabIconsetClients::setData(PsiCon *psicon, QWidget *p)
{
	psi = psicon;
	parentWidget = p;
}

void OptionsTabIconsetClients::cancelThread()
{
	if ( thread ) {
		threadCancelled.lock();
		thread->cancelled = true;
		threadCancelled.unlock();

		thread = 0;
	}
}

//----------------------------------------------------------------------------
// OptionsTabIconsetRoster
//----------------------------------------------------------------------------

OptionsTabIconsetRoster::OptionsTabIconsetRoster(QObject *parent)
	: OptionsTab(parent, "iconset_roster", "", tr("Roster Icons"), tr("Select iconsets for your roster"))
	, w(0)
	, thread(0)
{
}

OptionsTabIconsetRoster::~OptionsTabIconsetRoster()
{
	cancelThread();
}

void OptionsTabIconsetRoster::addService(const QString& id, const QString& name)
{
	IconsetRosterUI*d = (IconsetRosterUI*)w;
	QTreeWidgetItem* item = new QTreeWidgetItem(d->tw_isServices, QStringList(QString(name)));
	item->setData(0, ServiceRole, QVariant(QString(id)));
	d->tw_isServices->addTopLevelItem(item);
}

QWidget *OptionsTabIconsetRoster::widget()
{
	if ( w )
		return 0;

	w = new IconsetRosterUI;
	IconsetRosterUI *d = (IconsetRosterUI *)w;

	// Initialize transport iconsets
	addService("aim", "AIM");
	addService("muc", "Chat (MUC)");
	addService("gadugadu", "Gadu-Gadu");
	addService("icq", "ICQ");
	addService("irc", "IRC");
	addService("disk", "Jabber Disk");
	addService("mrim", "Mail.Ru-IM");
	addService("msn", "MSN");
	addService("rss", "RSS");
	addService("sms", "SMS");
	addService("smtp", "SMTP");
	addService("transport", tr("Transport"));
	addService("vkontakte", tr("vk.com"));
	addService("weather", tr("Weather"));
	addService("xmpp", "XMPP");
	addService("yahoo", "Yahoo!");

	d->tw_isServices->resizeColumnToContents(0);

	connect(d->pb_defRosterDetails, SIGNAL(clicked()), SLOT(defaultDetails()));

	connect(d->iss_servicesRoster, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), SLOT(isServices_iconsetSelected(QListWidgetItem *, QListWidgetItem *)));
	connect(d->tw_isServices, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), SLOT(isServices_selectionChanged(QTreeWidgetItem *)));
	connect(d->pb_servicesDetails, SIGNAL(clicked()), SLOT(servicesDetails()));
	isServices_selectionChanged(0);

	connect(d->iss_customRoster, SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)), SLOT(isCustom_iconsetSelected(QListWidgetItem *, QListWidgetItem *)));
	connect(d->tw_customRoster, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), SLOT(isCustom_selectionChanged(QTreeWidgetItem *)));
	connect(d->tw_customRoster, SIGNAL(itemClicked(QTreeWidgetItem *, int )), SLOT(isCustom_selectionChanged(QTreeWidgetItem *)));

	connect(d->pb_customRosterDetails, SIGNAL(clicked()), SLOT(customDetails()));
	connect(d->le_customRoster, SIGNAL(textChanged(const QString &)), SLOT(isCustom_textChanged()));
	connect(d->pb_customRosterAdd, SIGNAL(clicked()), SLOT(isCustom_add()));
	connect(d->pb_customRosterDelete, SIGNAL(clicked()), SLOT(isCustom_delete()));
	isCustom_selectionChanged(0);

	d->ck_useTransportIconsForContacts->setWhatsThis(
		tr("Toggles use of transport icons to the contacts, that use that transports."));

	// TODO: add QWhatsThis

	return w;
}

void OptionsTabIconsetRoster::applyOptions()
{
	if (!w || thread)
		return;

	IconsetRosterUI *d = (IconsetRosterUI *)w;
	PsiOptions::instance()->setOption("options.ui.contactlist.use-transport-icons", d->ck_useTransportIconsForContacts->isChecked());

	// roster - default
	{
		const Iconset *is = d->iss_defRoster->iconset();
		if (is) {
			QFileInfo fi(is->fileName());
			PsiOptions::instance()->setOption("options.iconsets.status", fi.fileName());
		}
	}

	// roster - services
	{
		QString baseServicesPath = "options.iconsets.service-status";
		PsiOptions::instance()->removeOption(baseServicesPath, true);

		QTreeWidgetItemIterator it(d->tw_isServices);
		while (*it) {
			QString path = PsiOptions::instance()->mapPut(baseServicesPath, (*it)->data(0, ServiceRole).toString());
			PsiOptions::instance()->setOption(path + ".iconset", (*it)->data(0, IconsetRole).toString());
			++it;
		}
	}

	// roster - custom
	{
		QString baseCustomStatusPath = "options.iconsets.custom-status";
		PsiOptions::instance()->removeOption(baseCustomStatusPath, true);

		int index = 0;
		QTreeWidgetItemIterator it(d->tw_customRoster);
		while (*it) {
			QString path = baseCustomStatusPath + ".a" + QString::number(index++);
			PsiOptions::instance()->setOption(path + ".regexp", (*it)->data(0, RegexpRole).toString());
			PsiOptions::instance()->setOption(path + ".iconset", (*it)->data(0, IconsetRole).toString());
			++it;
		}
	}
}

void OptionsTabIconsetRoster::restoreOptions()
{
	if ( !w || thread )
		return;

	IconsetRosterUI *d = (IconsetRosterUI *)w;
	d->ck_useTransportIconsForContacts->setChecked( PsiOptions::instance()->getOption("options.ui.contactlist.use-transport-icons").toBool() );

	d->iss_defRoster->clear();
	d->iss_servicesRoster->clear();
	d->iss_customRoster->clear();

	QStringList loaded;

	d->setCursor(Qt::WaitCursor);

	QPalette customPal = d->palette();
	// customPal.setDisabled(customPal.inactive());
	d->tabWidget3->setEnabled(false);
	d->tabWidget3->setPalette(customPal);

	d->progress->show();
	d->progress->setValue( 0 );

	numIconsets = countIconsets("/roster", loaded);
	iconsetsLoaded = 0;

	cancelThread();

	thread = new IconsetLoadThread(this, "/roster");
	thread->start();
}

bool OptionsTabIconsetRoster::event(QEvent *e)
{
	IconsetRosterUI *d = (IconsetRosterUI *)w;
	if ( e->type() == QEvent::User ) { // iconset load event
		IconsetLoadEvent *le = (IconsetLoadEvent *)e;

		if ( thread != le->thread() )
			return false;

		if ( !numIconsets )
			numIconsets = 1;
		d->progress->setValue( (int)((float)100 * ++iconsetsLoaded / numIconsets) );

		Iconset *i = le->iconset();
		if ( i ) {
			PsiIconset::instance()->stripFirstAnimFrame(i);
			QFileInfo fi( i->fileName() );

			// roster - default
			d->iss_defRoster->insert(*i);
			if ( fi.fileName() == PsiOptions::instance()->getOption("options.iconsets.status"))
				d->iss_defRoster->setItemSelected(d->iss_defRoster->lastItem(), true);

			// roster - service
			d->iss_servicesRoster->insert(*i);

			// roster - custom
			d->iss_customRoster->insert(*i);

			delete i;
		}

		return true;
	}
	else if ( e->type() == QEvent::User + 1 ) { // finish event
		// roster - service
		{
			// fill the QTreeWidget
			QTreeWidgetItemIterator it(d->tw_isServices);
			while (*it) {
				QTreeWidgetItem *i = *it;
				if (!i->data(0, ServiceRole).toString().isEmpty()) {
					Iconset *iss = PsiIconset::instance()->roster[
					                   PsiOptions::instance()->getOption(
					                       PsiOptions::instance()->mapLookup(
					                           "options.iconsets.service-status",
					                           i->data(0, ServiceRole).toString())+".iconset").toString()];
					if (iss) {
						i->setText(1, iss->name());
						QFileInfo fi(iss->fileName());
						i->setData(0, IconsetRole, fi.fileName());
					}
				}
				++it;
			}
		}

		// roster - custom
		{
			// Then, fill the QListView
			QTreeWidgetItem *last = 0;
			QStringList customicons = PsiOptions::instance()->getChildOptionNames("options.iconsets.custom-status", true, true);
			foreach(QString base, customicons) {
				QString regexp = PsiOptions::instance()->getOption(base + ".regexp").toString();
				QString icoset = PsiOptions::instance()->getOption(base + ".iconset").toString();
				QTreeWidgetItem *item = new QTreeWidgetItem(d->tw_customRoster, last);
				last = item;

				item->setText(0, clipCustomText(regexp));
				item->setData(0, RegexpRole, regexp);

				Iconset *iss = PsiIconset::instance()->roster[icoset];
				if ( iss ) {
					item->setText(1, iss->name());
					QFileInfo fi ( iss->fileName() );
					item->setData(0, IconsetRole, fi.fileName());
				}
			}
			d->tw_customRoster->resizeColumnToContents(0);
		}

		d->tabWidget3->setEnabled(true);
		// d->tabWidget3->setPalette(QPalette());

		connect(d->iss_defRoster, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SIGNAL(dataChanged()));
		connect(d->iss_servicesRoster, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SIGNAL(dataChanged()));
		connect(d->iss_customRoster, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SIGNAL(dataChanged()));

		d->progress->hide();

		d->unsetCursor();
		thread = 0;

		return true;
	}

	return false;
}

void OptionsTabIconsetRoster::setData(PsiCon *psicon, QWidget *p)
{
	psi = psicon;
	parentWidget = p;
}

void OptionsTabIconsetRoster::defaultDetails()
{
	IconsetRosterUI *d = (IconsetRosterUI *)w;
	const Iconset *is = d->iss_defRoster->iconset();
	if (is) {
		isDetails(*is, parentWidget->parentWidget(), psi);
	}
}

void OptionsTabIconsetRoster::servicesDetails()
{
	IconsetRosterUI *d = (IconsetRosterUI *)w;
	const Iconset *is = d->iss_servicesRoster->iconset();
	if (is) {
		isDetails(*is, parentWidget->parentWidget(), psi);
	}
}

void OptionsTabIconsetRoster::customDetails()
{
	IconsetRosterUI *d = (IconsetRosterUI *)w;
	const Iconset *is = d->iss_customRoster->iconset();
	if (is) {
		isDetails(*is, parentWidget->parentWidget(), psi);
	}
}

//------------------------------------------------------------

void OptionsTabIconsetRoster::isServices_iconsetSelected(QListWidgetItem *item, QListWidgetItem *)
{
	if ( !item )
		return;

	IconsetRosterUI *d = (IconsetRosterUI *)w;
	QTreeWidgetItem *it = d->tw_isServices->currentItem();
	if ( !it )
		return;

	const Iconset *is = ((IconWidgetItem *)item)->iconset();
	if ( !is )
		return;

	it->setText(1, is->name());
	QFileInfo fi ( is->fileName() );
	it->setData(0, IconsetRole, fi.fileName());
}

void OptionsTabIconsetRoster::isServices_selectionChanged(QTreeWidgetItem *it)
{
	IconsetRosterUI *d = (IconsetRosterUI *)w;
	d->iss_servicesRoster->setEnabled( it != 0 );
	d->pb_servicesDetails->setEnabled( it != 0 );
	d->iss_servicesRoster->clearSelection();
	if ( !it )
		return;

	QString name = it->data(0, IconsetRole).toString();
	if (name.isEmpty())
		return;

	emit noDirty(true);
	for (int row = 0; row < d->iss_servicesRoster->count(); row++) {
		IconWidgetItem *item = (IconWidgetItem *)d->iss_servicesRoster->item(row);
		const Iconset *is = item->iconset();
		if ( is ) {
			QFileInfo fi ( is->fileName() );
			if ( fi.fileName() == name ) {
				emit noDirty(true);
				d->iss_servicesRoster->setItemSelected(item, true);
				d->iss_servicesRoster->scrollToItem(item);
				emit noDirty(false);
				break;
			}
		}
	}
	qApp->processEvents();
	emit noDirty(false);
}

//------------------------------------------------------------

void OptionsTabIconsetRoster::isCustom_iconsetSelected(QListWidgetItem *item, QListWidgetItem *)
{
	if ( !item )
		return;

	IconsetRosterUI *d = (IconsetRosterUI *)w;
	QTreeWidgetItem *it = d->tw_customRoster->currentItem();
	if ( !it )
		return;

	const Iconset *is = ((IconWidgetItem *)item)->iconset();
	if ( !is )
		return;

	it->setText(1, is->name());
	QFileInfo fi ( is->fileName() );
	it->setData(0, IconsetRole, fi.fileName());
}

void OptionsTabIconsetRoster::isCustom_selectionChanged(QTreeWidgetItem *it)
{
	IconsetRosterUI *d = (IconsetRosterUI *)w;
	d->iss_customRoster->setEnabled( it != 0 );
	d->pb_customRosterDetails->setEnabled( it != 0 );
	//d->pb_customRosterAdd->setEnabled( it != 0 );
	d->pb_customRosterDelete->setEnabled( it != 0 );
	d->le_customRoster->setEnabled( it != 0 );
	d->iss_customRoster->clearSelection();
	if ( !it )
		return;

	QString name = it->data(0, IconsetRole).toString();
	if (name.isEmpty())
		return;

	emit noDirty(true);
	d->le_customRoster->setText(it->data(0, RegexpRole).toString());

	for (int row = 0; row < d->iss_customRoster->count(); row++) {
		IconWidgetItem *item = (IconWidgetItem *)d->iss_customRoster->item(row);
		const Iconset *is = item->iconset();
		if ( is ) {
			QFileInfo fi ( is->fileName() );
			if ( fi.fileName() == name ) {
				d->iss_customRoster->setItemSelected(item, true);
				d->iss_customRoster->scrollToItem(item);
				break;
			}
		}
	}
	qApp->processEvents();
	emit noDirty(false);
}

void OptionsTabIconsetRoster::isCustom_textChanged()
{
	IconsetRosterUI *d = (IconsetRosterUI *)w;
	QTreeWidgetItem *item = d->tw_customRoster->currentItem();
	if ( !item )
		return;

	item->setText(0, clipCustomText(d->le_customRoster->text()));
	item->setData(0, RegexpRole, d->le_customRoster->text());
}

void OptionsTabIconsetRoster::isCustom_add()
{
	IconsetRosterUI *d = (IconsetRosterUI *)w;

	QString def;
	const Iconset *iconset = d->iss_defRoster->iconset();
	if ( iconset ) {
		QFileInfo fi( iconset->fileName() );
		def = fi.fileName();
	}
	else
		qWarning("OptionsTabIconsetRoster::isCustom_add(): 'def' is null!");

	QTreeWidgetItem *item = new QTreeWidgetItem(d->tw_customRoster);
	const Iconset *i = PsiIconset::instance()->roster[def];
	if ( i ) {
		item->setText(1, i->name());

		QFileInfo fi(i->fileName());
		item->setData(0, IconsetRole, fi.fileName());
	}

	d->tw_customRoster->setCurrentItem(item);
	emit dataChanged();

	d->le_customRoster->setFocus();
}

void OptionsTabIconsetRoster::isCustom_delete()
{
	IconsetRosterUI *d = (IconsetRosterUI *)w;
	QTreeWidgetItem *item = d->tw_customRoster->currentItem();
	if ( !item )
		return;

	delete item;
	isCustom_selectionChanged(0);
	d->tw_customRoster->clearSelection();
	emit dataChanged();
}

QString OptionsTabIconsetRoster::clipCustomText(QString s)
{
	if ( s.length() > 10 ) {
		s = s.left(9);
		s += "...";
	}

	return s;
}

void OptionsTabIconsetRoster::cancelThread()
{
	if ( thread ) {
		threadCancelled.lock();
		thread->cancelled = true;
		threadCancelled.unlock();

		thread = 0;
	}
}
