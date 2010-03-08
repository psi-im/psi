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
    waitForObject(":Psi: Jabber Accounts.Remove_IconButton");
    sendEvent("QMouseEvent", ":Psi: Jabber Accounts.Remove_IconButton", QEvent.MouseButtonPress, 31, 14, Qt.LeftButton, 0);
    waitForObject(":Psi: Jabber Accounts.Remove_IconButton");
    sendEvent("QMouseEvent", ":Psi: Jabber Accounts.Remove_IconButton", QEvent.MouseButtonRelease, 31, 14, Qt.LeftButton, 1);
    waitForObject(":Psi: Remove Account.Remove_QPushButton");
    sendEvent("QMouseEvent", ":Psi: Remove Account.Remove_QPushButton", QEvent.MouseButtonPress, 31, 19, Qt.LeftButton, 0);
    waitForObject(":Psi: Remove Account.Remove_QPushButton");
    sendEvent("QMouseEvent", ":Psi: Remove Account.Remove_QPushButton", QEvent.MouseButtonRelease, 31, 19, Qt.LeftButton, 1);
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
    snooze(3.0);
}

function main()
{
    tst_account_contact_counter();
    // tst_1080_account_removing();
}
