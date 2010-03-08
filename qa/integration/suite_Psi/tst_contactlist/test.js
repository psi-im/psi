var UserRole = 32;
var OnlineContactsRole = UserRole + 15;
var TotalContactsRole = UserRole + 16;

function tst_1080_account_removing()
{
    waitForObjectItem(":Psi_QMenuBar", "General");
    activateItem(":Psi_QMenuBar", "General");
    waitForObjectItem(":Psi.General_QMenu", "Account Setup");
    activateItem(":Psi.General_QMenu", "Account Setup");
    waitForObject(":Psi: Jabber Accounts.Add_IconButton");
    clickButton(":Psi: Jabber Accounts.Add_IconButton");
    waitForObject(":Psi: Add Account.le_name_QLineEdit");
    type(":Psi: Add Account.le_name_QLineEdit", "user04");
    waitForObject(":Psi: Add Account.le_name_QLineEdit");
    type(":Psi: Add Account.le_name_QLineEdit", "<Return>");

    waitForObject(":Account.le_jid_QLineEdit");
    type(":Account.le_jid_QLineEdit", "user04@localhost");
    waitForObject(":Account.le_jid_QLineEdit");
    type(":Account.le_jid_QLineEdit", "<Tab>");
    waitForObject(":Account.le_pass_QLineEdit");
    type(":Account.le_pass_QLineEdit", "user04");
    waitForObject(":Account.le_pass_QLineEdit");
    type(":Account.le_pass_QLineEdit", "<Return>");

    waitForObject(":Psi: Jabber Accounts.lv_accs_QTreeWidget");
    clickItem(":Psi: Jabber Accounts.lv_accs_QTreeWidget", "user04", 31, 10, 1, Qt.LeftButton);

    waitForObjectItem(":Psi_QMenuBar", "Status");
    activateItem(":Psi.Status_QMenu", "Online");
    waitForObject(":Trust this certificate_QPushButton");
    clickButton(":Trust this certificate_QPushButton");
    waitForObject(":Psi_PsiContactListView");
    openItemContextMenu(":Psi_PsiContactListView", "user04", 173, 9, 2);
    waitForObjectItem(":_ContactListAccountMenu", "Status");
    activateItem(":_ContactListAccountMenu", "Status");
    waitForObjectItem(":Status_StatusMenu", "Offline");
    activateItem(":Status_StatusMenu", "Offline");
    snooze(1.0);

    waitForObject(":Psi: Jabber Accounts.Remove_IconButton");
    clickButton(":Psi: Jabber Accounts.Remove_IconButton");
    waitForObject(":Psi: Remove Account.Remove_QPushButton");
    clickButton(":Psi: Remove Account.Remove_QPushButton");
    waitForObject(":Yes_QPushButton");
    clickButton(":Yes_QPushButton");
}

function tst_account_contact_counter()
{
    waitForObject(":Psi_PsiContactListView");
    var model = findObject(":Psi_PsiContactListView").model();
    test.compare(model.rowCount(), 3);

    waitForObjectItem(":Psi_QMenuBar", "Status");
    activateItem(":Psi.Status_QMenu", "Online");
    snooze(3.0);

    for (i = 0; i < 3; ++i) {
        test.compare(model.index(i, 0).data(OnlineContactsRole).toInt(), 2);
        test.compare(model.index(i, 0).data(TotalContactsRole).toInt(), 4);
    }

    waitForObjectItem(":Psi_QMenuBar", "Status");
    activateItem(":Psi.Status_QMenu", "Offline");
    snooze(1.0);
}

function main()
{
    tst_account_contact_counter();
    tst_1080_account_removing();
}
