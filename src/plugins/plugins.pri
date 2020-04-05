INCLUDEPATH += $$psi_plugins_include_dir 

HEADERS += \
    $$psi_plugins_include_dir/psiplugin.h \
    $$psi_plugins_include_dir/stanzafilter.h \
    $$psi_plugins_include_dir/stanzasender.h \
    $$psi_plugins_include_dir/stanzasendinghost.h \
    $$psi_plugins_include_dir/iqfilter.h \
    $$psi_plugins_include_dir/iqnamespacefilter.h \
    $$psi_plugins_include_dir/iqfilteringhost.h \
    $$psi_plugins_include_dir/eventfilter.h \
    $$psi_plugins_include_dir/optionaccessor.h \
    $$psi_plugins_include_dir/optionaccessinghost.h \
    $$psi_plugins_include_dir/popupaccessor.h \
    $$psi_plugins_include_dir/popupaccessinghost.h \
    $$psi_plugins_include_dir/shortcutaccessor.h \
    $$psi_plugins_include_dir/shortcutaccessinghost.h\
    $$psi_plugins_include_dir/iconfactoryaccessor.h \
    $$psi_plugins_include_dir/iconfactoryaccessinghost.h\
    $$psi_plugins_include_dir/activetabaccessor.h \
    $$psi_plugins_include_dir/activetabaccessinghost.h \
    $$psi_plugins_include_dir/applicationinfoaccessor.h \
    $$psi_plugins_include_dir/applicationinfoaccessinghost.h \
    $$psi_plugins_include_dir/accountinfoaccessor.h \
    $$psi_plugins_include_dir/accountinfoaccessinghost.h\
    $$psi_plugins_include_dir/gctoolbariconaccessor.h \
    $$psi_plugins_include_dir/toolbariconaccessor.h \
    $$psi_plugins_include_dir/menuaccessor.h \
    $$psi_plugins_include_dir/contactstateaccessor.h \
    $$psi_plugins_include_dir/contactstateaccessinghost.h \
    $$psi_plugins_include_dir/plugininfoprovider.h \
    $$psi_plugins_include_dir/psiaccountcontroller.h\
    $$psi_plugins_include_dir/psiaccountcontrollinghost.h \
    $$psi_plugins_include_dir/eventcreatinghost.h \
    $$psi_plugins_include_dir/eventcreator.h \
    $$psi_plugins_include_dir/contactinfoaccessor.h \
    $$psi_plugins_include_dir/contactinfoaccessinghost.h \
    $$psi_plugins_include_dir/soundaccessor.h \
    $$psi_plugins_include_dir/soundaccessinghost.h \
    $$psi_plugins_include_dir/chattabaccessor.h \
    $$psi_plugins_include_dir/webkitaccessor.h \
    $$psi_plugins_include_dir/webkitaccessinghost.h

OTHER_FILES += $$PWD/psiplugin.pri
