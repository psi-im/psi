var UserRole = 32;
var OnlineContactsRole = UserRole + 15;
var TotalContactsRole = UserRole + 16;

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
    waitForObject(":Yes_QPushButton");
    clickButton(":Yes_QPushButton");
}

function dismiss_ssl_error()
{
    waitForObject(":Trust this certificate_QPushButton");
    clickButton(":Trust this certificate_QPushButton");
}

function set_global_status(status_name)
{
    waitForObjectItem(":Psi_QMenuBar", "Status");
    activateItem(":Psi.Status_QMenu", status_name);
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

function tst_1080_account_removing()
{
    add_account("user04", "user04@localhost", "user04");

    set_global_status("Online");
    dismiss_ssl_error();
    set_account_status("user04", "Offline");
    snooze(1.0);

    remove_account("user04");
}

function tst_account_contact_counter()
{
    waitForObject(":Psi_PsiContactListView");
    snooze(1.0);
    var model = findObject(":Psi_PsiContactListView").model();
    test.compare(model.rowCount(), 3);

    set_global_status("Online");
    snooze(3.0);

    for (i = 0; i < 3; ++i) {
        test.compare(model.index(i, 0).data(OnlineContactsRole).toInt(), 2);
        test.compare(model.index(i, 0).data(TotalContactsRole).toInt(), 4);
    }

    set_global_status("Offline");
    snooze(1.0);
}

function add_contact(account_name, contact_jid)
{
    waitForObject(":Psi_PsiContactListView");
    openItemContextMenu(":Psi_PsiContactListView", account_name, 155, 13, 2);
    waitForObjectItem(":_ContactListAccountMenu", "Add a Contact");
    activateItem(":_ContactListAccountMenu", "Add a Contact");
    waitForObject(":Psi: Add Contact.le_jid_QLineEdit");
    type(":Psi: Add Contact.le_jid_QLineEdit", contact_jid);
    waitForObject(":Psi: Add Contact.le_jid_QLineEdit");
    type(":Psi: Add Contact.le_jid_QLineEdit", "<Enter>");
    waitForObject(":OK_QPushButton");
    type(":OK_QPushButton", "<Enter>");
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

function tst_remove_transport()
{
    add_contact('user-66', 'test');
    remove_contact("user-66.Agents/Transports.test");
}

function tst_remove_not_in_list()
{
    send_chat_message("user-66.General.user01", "hi");
    remove_contact("user01.Not in List.user-66");
}

function tst_remove_items()
{
    add_account("user-66", "user-66@localhost", "user-66");
    set_global_status("Online");
    dismiss_ssl_error();
    snooze(3.0);

    tst_remove_transport();
    tst_remove_not_in_list();

    set_global_status("Offline");
    snooze(1.0);

    remove_account("user-66");
}

function main()
{
    tst_account_contact_counter();
    tst_1080_account_removing();
    tst_remove_items();

    waitForObject(":Psi_PsiApplication");
    sendEvent("QCloseEvent", ":Psi_PsiApplication");
}
