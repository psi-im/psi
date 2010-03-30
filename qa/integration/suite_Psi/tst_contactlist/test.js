var UserRole = 32;
var StatusTypeRole = UserRole + 5;
var IsAlertingRole = UserRole + 10;
var OnlineContactsRole = UserRole + 15;
var TotalContactsRole = UserRole + 16;

var Status_Offline = 0;
var Status_Online = 1;
var Status_Away = 2;
var Status_XA = 3;
var Status_DND = 4;
var Status_Invisible = 5;
var Status_FFC = 6;

var Status_Names = new Array();
Status_Names[Status_Offline] = "Offline";
Status_Names[Status_Online] = "Online";
Status_Names[Status_Away] = "Away";
Status_Names[Status_XA] = "Not Available";
Status_Names[Status_DND] = "Do not Disturb";
Status_Names[Status_Invisible] = "Invisible";
Status_Names[Status_FFC] = "Free for Chat";

function add_account(account_name, account_jid, account_password)
{
    waitForObjectItem(":Psi_QMenuBar", "General");
    activateItem(":Psi_QMenuBar", "General");
    waitForObjectItem(":Psi.General_QMenu", "Account Setup");
    activateItem(":Psi.General_QMenu", "Account Setup");
    waitForObject(":Psi: Jabber Accounts.Add_IconButton");
    clickButton(":Psi: Jabber Accounts.Add_IconButton");
    waitForObject(":Psi: Add Account.le_name_QLineEdit");
    type(":Psi: Add Account.le_name_QLineEdit", account_name);
    waitForObject(":Psi: Add Account.le_name_QLineEdit");
    type(":Psi: Add Account.le_name_QLineEdit", "<Return>");

    waitForObject(":Account.le_jid_QLineEdit");
    type(":Account.le_jid_QLineEdit", account_jid);
    waitForObject(":Account.le_jid_QLineEdit");
    type(":Account.le_jid_QLineEdit", "<Tab>");
    waitForObject(":Account.le_pass_QLineEdit");
    type(":Account.le_pass_QLineEdit", account_password);
    waitForObject(":Account.le_pass_QLineEdit");
    type(":Account.le_pass_QLineEdit", "<Return>");
}

function remove_account(account_name)
{
    waitForObject(":Psi: Jabber Accounts.lv_accs_QTreeWidget");
    clickItem(":Psi: Jabber Accounts.lv_accs_QTreeWidget", account_name, 31, 10, 1, Qt.LeftButton);

    waitForObject(":Psi: Jabber Accounts.Remove_IconButton");
    clickButton(":Psi: Jabber Accounts.Remove_IconButton");
    waitForObject(":Psi: Remove Account.Remove_QPushButton");
    clickButton(":Psi: Remove Account.Remove_QPushButton");
    waitForObject(":Remove_QPushButton");
    clickButton(":Remove_QPushButton");
}

function dismiss_ssl_error()
{
    if (object.exists(":Trust this certificate_QPushButton")) {
        clickButton(":Trust this certificate_QPushButton");
    }
}

function set_global_status(status_type)
{
    waitForObjectItem(":Psi_QMenuBar", "Status");
    activateItem(":Psi.Status_QMenu", Status_Names[status_type]);
    ensure_all_accounts_status(status_type);
}

function ensure_all_accounts_status(status_type)
{
    var model = findObject(":Psi_PsiContactListView").model();
    var total_time = 0.0;
    var interval = 0.1;
    while (true) {
        total_time += interval;
        snooze(interval);

        dismiss_ssl_error();

        var ok = true;
        for (i = 0; i < model.rowCount(); ++i) {
            if (model.index(i, 0).data(StatusTypeRole).toInt() != status_type ||
                model.index(i, 0).data(IsAlertingRole).toBool())
            {
                ok = false;
                break;
            }
        }

        if (ok) {
            break;
        }

        if (total_time > 20.0) {
            test.fail("ensure_all_accounts_status " + status_type);
        }
    }
}

function ensure_all_connected()
{
    ensure_all_accounts_status(Status_Online);
}

function ensure_all_disconnected()
{
    ensure_all_accounts_status(Status_Offline);
}

function set_account_status(account_name, status_name)
{
    waitForObject(":Psi_PsiContactListView");
    openItemContextMenu(":Psi_PsiContactListView", account_name, 173, 9, 2);
    waitForObjectItem(":_ContactListAccountMenu", "Status");
    activateItem(":_ContactListAccountMenu", "Status");
    waitForObjectItem(":Status_StatusMenu", status_name);
    activateItem(":Status_StatusMenu", status_name);
}

function add_contact(account_name, contact_jid)
{
    add_contact_to_group(account_name, contact_jid, '', '', true);
}

function add_contact_to_group(account_name, contact_jid, contact_name, group_name, request_auth)
{
    waitForObjectItem(":Psi_PsiContactListView", account_name);
    openItemContextMenu(":Psi_PsiContactListView", account_name, 102, 6, 2);

    waitForObjectItem(":_ContactListAccountMenu", "Add a Contact");
    activateItem(":_ContactListAccountMenu", "Add a Contact");
    waitForObject(":Psi: Add Contact.le_jid_QLineEdit");
    type(":Psi: Add Contact.le_jid_QLineEdit", contact_jid);
    waitForObject(":Psi: Add Contact.le_jid_QLineEdit");
    type(":Psi: Add Contact.le_jid_QLineEdit", "<Tab>");
    if (contact_name.length) {
        waitForObject(":Psi: Add Contact.le_nick_QLineEdit");
        type(":Psi: Add Contact.le_nick_QLineEdit", contact_name);
    }
    waitForObject(":Psi: Add Contact.le_nick_QLineEdit");
    type(":Psi: Add Contact.le_nick_QLineEdit", "<Tab>");
    if (group_name.length) {
        waitForObject(":Psi: Add Contact.cb_group_QComboBox");
        type(":Psi: Add Contact.cb_group_QComboBox", group_name);
    }
    waitForObject(":Psi: Add Contact.cb_group_QComboBox");
    type(":Psi: Add Contact.cb_group_QComboBox", "<Tab>");
    if (!request_auth) {
        waitForObject(":Psi: Add Contact.Request authorization when adding_QCheckBox");
        type(":Psi: Add Contact.Request authorization when adding_QCheckBox", " ");
    }
    waitForObject(":Psi: Add Contact.Request authorization when adding_QCheckBox");
    type(":Psi: Add Contact.Request authorization when adding_QCheckBox", "<Return>");
    waitForObject(":OK_QPushButton");
    type(":OK_QPushButton", "<Return>");
}

function remove_contact(contact_list_item)
{
    waitForObjectItem(":Psi_PsiContactListView", contact_list_item);
    openItemContextMenu(":Psi_PsiContactListView", contact_list_item, 102, 6, 2);
    waitForObjectItem(":_PsiContactMenu", "Remove");
    activateItem(":_PsiContactMenu", "Remove");
    waitForObject(":Delete_QPushButton");
    type(":Delete_QPushButton", "<Return>");
    // FIXME: ensure that the item actually disappeared
}

function remove_group(contact_list_item)
{
    waitForObjectItem(":Psi_PsiContactListView", contact_list_item);
    openItemContextMenu(":Psi_PsiContactListView", contact_list_item, 102, 6, 2);
    waitForObjectItem(":_ContactListGroupMenu", "Remove Group");
    activateItem(":_ContactListGroupMenu", "Remove Group");
    waitForObject(":_QMessageBox_2");
    sendEvent("QMoveEvent", ":_QMessageBox_2", 0, 323, 253, 250);
    waitForObject(":Yes_QPushButton");
    type(":Yes_QPushButton", "<Enter>");
    // FIXME: ensure that the group was actually deleted
}

function remove_group_and_contacts(contact_list_item)
{
    waitForObjectItem(":Psi_PsiContactListView", contact_list_item);
    openItemContextMenu(":Psi_PsiContactListView", contact_list_item, 102, 6, 2);
    waitForObjectItem(":_ContactListGroupMenu", "Remove Group and Contacts");
    activateItem(":_ContactListGroupMenu", "Remove Group and Contacts");
    waitForObject(":Delete_QPushButton");
    type(":Delete_QPushButton", "<Return>");
    // FIXME: ensure that the group was actually deleted
}

function send_chat_message(contact_list_item, message)
{
    doubleClickItem(":Psi_PsiContactListView", "user-66.General.user01", 114, 10, 0, Qt.LeftButton);
    waitForObject(":bottomFrame_LineEdit");
    type(":bottomFrame_LineEdit", message);
    waitForObject(":bottomFrame_LineEdit");
    type(":bottomFrame_LineEdit", "<Return>");
    waitForObject(":bottomFrame_LineEdit");
    type(":bottomFrame_LineEdit", "<Esc>");
}

function rename_group(contact_list_item, new_group_name)
{
    waitForObject(":Psi_PsiContactListView");
    openItemContextMenu(":Psi_PsiContactListView", contact_list_item, 135, 12, 2);

    waitForObjectItem(":_ContactListGroupMenu", "Rename");
    activateItem(":_ContactListGroupMenu", "Rename");

    waitForObject(":contactListView_QExpandingLineEdit");
    type(":contactListView_QExpandingLineEdit", new_group_name);
    waitForObject(":contactListView_QExpandingLineEdit");
    type(":contactListView_QExpandingLineEdit", "<Return>");
    // FIXME: ensure that the group was actually renamed
}

// existing_group_name: "None" / "1. Test" / etc.
function set_contact_existing_group(contact_list_item, existing_group_name)
{
    waitForObject(":Psi_PsiContactListView");
    openItemContextMenu(":Psi_PsiContactListView", contact_list_item, 133, 13, 2);
    waitForObjectItem(":_PsiContactMenu", "Group");
    activateItem(":_PsiContactMenu", "Group");
    waitForObjectItem(":Group_GroupMenu", existing_group_name);
    activateItem(":Group_GroupMenu", existing_group_name);
    // FIXME: ensure that the item was actually moved to new group
}

// FIXME: needs work
function drag_and_drop_contacts(contacts)
{
    waitForObjectItem(":Psi_PsiContactListView", "user01.Test2.user03");
    clickItem(":Psi_PsiContactListView", "user01.Test2.user03", 174, 12, 67108865, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "user01.Test2.user04");
    clickItem(":Psi_PsiContactListView", "user01.Test2.user04", 172, 6, 67108865, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "user01.Test2.user05");
    clickItem(":Psi_PsiContactListView", "user01.Test2.user05", 164, 5, 67108865, Qt.LeftButton);

    waitForObject(":Psi_PsiContactListView");
    dragAndDrop(":Psi_PsiContactListView", 163, 122, ":Psi_PsiContactListView", 152, 65, Qt.CopyAction);

    waitForObject(":Psi_PsiContactListView");
    dragItemBy(":Psi_PsiContactListView", 163, 122, -11, -57, 1, Qt.LeftButton);
}

function tst_account_contact_counter()
{
    waitForObject(":Psi_PsiContactListView");
    snooze(1.0);
    var model = findObject(":Psi_PsiContactListView").model();
    test.compare(model.rowCount(), 3);

    set_global_status(Status_Online);

    for (i = 0; i < 3; ++i) {
        test.compare(model.index(i, 0).data(OnlineContactsRole).toInt(), 2);
        test.compare(model.index(i, 0).data(TotalContactsRole).toInt(), 4);
    }

    set_global_status(Status_Offline);
}

function tst_1080_account_removing()
{
    add_account("user04", "user04@localhost", "user04");

    set_global_status(Status_Online);
    set_account_status("user04", "Offline");
    snooze(1.0);

    remove_account("user04");
    set_global_status(Status_Offline);
}

function tst_remove_transport()
{
    add_contact('user-66', 'test');
    remove_contact("user-66.Agents/Transports.test");
}

function tst_remove_not_in_list()
{
    send_chat_message("user-66.General.user01", "hi");
    remove_contact("user01.Not in List.user-66");

    send_chat_message("user-66.General.user01", "hi");
    remove_group_and_contacts("user01.Not in List");
}

function tst_remove_groups()
{
    add_contact_to_group('user-66', 'user02@localhost', 'user02', 'Test', false);
    remove_group('user-66.Test');
    remove_contact("user-66.General.user02");

    add_contact_to_group('user-66', 'user02@localhost', 'user02', 'Test', false);
    remove_contact("user-66.Test.user02");
}

function tst_remove_items()
{
    add_account("user-66", "user-66@localhost", "user-66");
    set_global_status(Status_Online);

    tst_remove_transport();
    tst_remove_not_in_list();
    tst_remove_groups();

    set_global_status(Status_Offline);

    remove_account("user-66");
}

function tst_rename_items()
{
    set_global_status(Status_Online);

    rename_group("user01.General", "Test");
    set_contact_existing_group("user01.Test.user02", "None");
    set_contact_existing_group("user01.Test.user03", "None");
    set_contact_existing_group("user01.Test.user04", "None");
    set_contact_existing_group("user01.Test.user05", "None");

    // set_contact_new_group("user01.General.user02", "Test");
    // set_contact_existing_group("user01.Test.user02", "None");

    // rename_general_group();
    // rename_rosteR_items();

    set_global_status(Status_Offline);
}

function tst_drag_and_drop()
{
    // TODO
}

function main()
{
    // startApplication("psi");
    tst_account_contact_counter();
    tst_1080_account_removing();
    tst_remove_items();
    tst_rename_items();
    tst_drag_and_drop();

    waitForObject(":Psi_PsiApplication");
    sendEvent("QCloseEvent", ":Psi_PsiApplication");
}
