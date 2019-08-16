#include "groupchattopicdlg.h"

#include "groupchatdlg.h"
#include "iconset.h"
#include "psioptions.h"
#include "shortcutmanager.h"
#include "ui_groupchattopicaddlang.h"
#include "ui_groupchattopicdlg.h"

#include <QPlainTextEdit>

GroupchatTopicDlg::GroupchatTopicDlg(GCMainDlg *parent) :
    QDialog(parent),
    m_ui(new Ui::GroupchatTopicDlg),
    m_addLangUi(new Ui::GroupChatTopicAddLangDlg)
{
    m_ui->setupUi(this);
    QKeySequence sendKey = ShortcutManager::instance()->shortcut("chat.send");
    if (sendKey == QKeySequence(Qt::Key_Enter) || sendKey == QKeySequence(Qt::Key_Return)) {
        sendKey = QKeySequence(Qt::CTRL + Qt::Key_Return);
    }
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setShortcut(sendKey);

    auto cw = new QToolButton();
    cw->setIcon(IconsetFactory::icon("psi/add").icon());
    m_ui->twLang->setCornerWidget(cw);
    QObject::connect(m_ui->twLang, &QTabWidget::tabCloseRequested, this, [=](int index) {
        m_ui->twLang->widget(index)->deleteLater();
        m_ui->twLang->removeTab(index);
    });
    QObject::connect(cw, &QToolButton::clicked, this, [=](bool checked) {
        Q_UNUSED(checked);
        if (!addLangDlg) {
            addLangDlg = new QDialog(this);
            m_addLangUi->setupUi(addLangDlg);
            addLangDlg->setWindowIcon(QIcon(IconsetFactory::iconPixmap("psi/logo_128")));
            addLangDlg->setAttribute(Qt::WA_DeleteOnClose);

            m_addLangUi->cmbLang->addItem(tr("Any Language"), 0);
            QMap<QString,QLocale::Language> langs;
            for (auto const &loc : QLocale::matchingLocales(
                     QLocale::AnyLanguage,
                     QLocale::AnyScript,
                     QLocale::AnyCountry))
            {
                if (loc != QLocale::c()) {
                    langs.insert(QLocale::languageToString(loc.language()), loc.language());
                }
            }
            for (auto lang: langs) {
                LanguageManager::LangId id;
                id.language = lang;
                m_addLangUi->cmbLang->addItem(LanguageManager::languageName(id), lang);
            }

            populateCountryAndScript();

            addLangDlg->adjustSize();
            addLangDlg->move(cw->mapToGlobal(QPoint(cw->width() - addLangDlg->width(), cw->height())));
            addLangDlg->show();
            QObject::connect(addLangDlg,  &QDialog::accepted, this, [=]() {
                LanguageManager::LangId id;
                id.language = m_addLangUi->cmbLang->currentData().toInt();
                id.script = m_addLangUi->cmbScript->currentData().toInt();
                id.country = m_addLangUi->cmbCountry->currentData().toInt();
                bool found = false;
                for (int i = 0; i < m_ui->twLang->count(); i++) {
                    QPlainTextEdit *edit = static_cast<QPlainTextEdit *>(m_ui->twLang->widget(i));
                    LanguageManager::LangId tabId = edit->property("langId").value<LanguageManager::LangId>();
                    if (id == tabId) {
                        m_ui->twLang->setCurrentIndex(i);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    addLanguage(id);
                }
            });

            QObject::connect(m_addLangUi->cmbLang, static_cast<void(QComboBox::*)(int index)>(&QComboBox::currentIndexChanged), this, [=](int index) {
                Q_UNUSED(index)
                populateCountryAndScript();
            });
        } else {
            addLangDlg->setFocus();
        }
    });
}

GroupchatTopicDlg::~GroupchatTopicDlg()
{
    delete m_ui;
    delete m_addLangUi;
}

QMap<LanguageManager::LangId, QString> GroupchatTopicDlg::subjectMap() const
{
    QMap<LanguageManager::LangId, QString> ret;
    for (int i = 0; i < m_ui->twLang->count(); i++) {
        QPlainTextEdit *edit = static_cast<QPlainTextEdit *>(m_ui->twLang->widget(i));
        LanguageManager::LangId id = edit->property("langId").value<LanguageManager::LangId>();
        QString text = edit->toPlainText();
        ret.insert(id, text);
    }
    return ret;
}

void GroupchatTopicDlg::setSubjectMap(const QMap<LanguageManager::LangId, QString> &topics)
{
    if (topics.isEmpty()) {
        addLanguage(LanguageManager::LangId());
    }
    for (auto it = topics.constBegin(); it != topics.constEnd(); ++it) {
        addLanguage(it.key(), it.value());
    }
}

void GroupchatTopicDlg::addLanguage(const LanguageManager::LangId &id, const QString &text)
{
    QFont f;
    f.fromString(PsiOptions::instance()->getOption("options.ui.look.font.chat").toString());

    QPlainTextEdit *edit = new QPlainTextEdit(text);
    edit->setFont(f);
    edit->setProperty("langId", QVariant::fromValue<LanguageManager::LangId>(id));
    m_ui->twLang->addTab(edit, LanguageManager::languageName(id));
}

void GroupchatTopicDlg::populateCountryAndScript()
{
    QLocale::Language lang = static_cast<QLocale::Language>(m_addLangUi->cmbLang->currentData().toInt());
    QMap<QString,QLocale::Script> scripts;
    QMap<QString,QLocale::Country> countries;
    m_addLangUi->cmbCountry->clear();
    m_addLangUi->cmbScript->clear();
    m_addLangUi->cmbCountry->addItem(tr("Any Country"), 0);
    m_addLangUi->cmbScript->addItem(tr("Any Script"), 0);
    if (lang) {
        for (auto const &loc : QLocale::matchingLocales(lang, QLocale::AnyScript, QLocale::AnyCountry)) {
            if (loc != QLocale::c()) {
                scripts.insert(QLocale::scriptToString(loc.script()), loc.script());
                countries.insert(QLocale::countryToString(loc.country()), loc.country());
            }
        }
        m_addLangUi->cmbScript->setVisible(scripts.count() > 1);
        m_addLangUi->lblScript->setVisible(scripts.count() > 1);
        if (scripts.count() > 1) {
            for (auto it = scripts.constBegin(); it != scripts.constEnd(); ++it) {
                m_addLangUi->cmbScript->addItem(it.key(), it.value());
            }
        }
        m_addLangUi->cmbCountry->setVisible(countries.count() > 1);
        m_addLangUi->lblCountry->setVisible(countries.count() > 1);
        if (countries.count() > 1) {
            for (auto it = countries.constBegin(); it != countries.constEnd(); ++it) {
                LanguageManager::LangId id;
                id.language = lang;
                id.country = it.value();
                QString country = LanguageManager::countryName(id);
                m_addLangUi->cmbCountry->addItem(country, it.value());
            }
        }
    }
}

void GroupchatTopicDlg::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
        case QEvent::LanguageChange:
            m_ui->retranslateUi(this);
            break;
        default:
            break;
    }
}
