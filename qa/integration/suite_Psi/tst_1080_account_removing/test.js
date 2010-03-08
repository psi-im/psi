function main()
{
    waitForObjectItem(":Psi_QMenuBar", "General");
    activateItem(":Psi_QMenuBar", "General");
    waitForObjectItem(":Psi.General_QMenu_2", "Account Setup");
    activateItem(":Psi.General_QMenu_2", "Account Setup");
    waitForObject(":Psi: Jabber Accounts.Add_IconButton");
    clickButton(":Psi: Jabber Accounts.Add_IconButton");
    waitForObject(":Psi: Add Account.le_name_QLineEdit");
    type(":Psi: Add Account.le_name_QLineEdit", "user04");
    waitForObject(":Psi: Add Account.Add_IconButton");
    clickButton(":Psi: Add Account.Add_IconButton");
    waitForObject(":Psi: Jabber Accounts.lv_accs_QTreeWidget");
    doubleClick(":Psi: Jabber Accounts.lv_accs_QTreeWidget", 325, 69, 0, Qt.LeftButton);
    waitForObject(":Account.le_jid_QLineEdit");
    mouseClick(":Account.le_jid_QLineEdit", 131, 12, 1, Qt.LeftButton);
    waitForObject(":Account.le_jid_QLineEdit");
    type(":Account.le_jid_QLineEdit", "user04@localhost");
    waitForObject(":Account.le_jid_QLineEdit");
    type(":Account.le_jid_QLineEdit", "<Tab>");
    waitForObject(":Account.le_pass_QLineEdit");
    type(":Account.le_pass_QLineEdit", "user04");
    waitForObject(":Account.le_pass_QLineEdit");
    type(":Account.le_pass_QLineEdit", "<Return>");
    waitForObjectItem(":Psi.Offline_QMenu_2", "Online");
    activateItem(":Psi.Offline_QMenu_2", "Online");
    waitForObject(":Trust this certificate_QPushButton");
    clickButton(":Trust this certificate_QPushButton");
    waitForObjectItem(":Psi_PsiContactListView", "user04");
    clickItem(":Psi_PsiContactListView", "user04", 151, 5, 2, Qt.RightButton);
    waitForObjectItem(":_ContactListAccountMenu", "Status");
    activateItem(":_ContactListAccountMenu", "Status");
    waitForObjectItem(":Status_StatusMenu", "Offline");
    activateItem(":Status_StatusMenu", "Offline");
    waitForObject(":Psi: Jabber Accounts.Remove_IconButton");
    sendEvent("QMouseEvent", ":Psi: Jabber Accounts.Remove_IconButton", QEvent.MouseButtonPress, 43, 18, Qt.LeftButton, 0);
    waitForObject(":Psi: Jabber Accounts.Remove_IconButton");
    sendEvent("QMouseEvent", ":Psi: Jabber Accounts.Remove_IconButton", QEvent.MouseButtonRelease, 43, 18, Qt.LeftButton, 1);
    waitForObject(":Psi: Remove Account.Remove_QPushButton");
    sendEvent("QMouseEvent", ":Psi: Remove Account.Remove_QPushButton", QEvent.MouseButtonPress, 36, 13, Qt.LeftButton, 0);
    waitForObject(":Psi: Remove Account.Remove_QPushButton");
    sendEvent("QMouseEvent", ":Psi: Remove Account.Remove_QPushButton", QEvent.MouseButtonRelease, 36, 13, Qt.LeftButton, 1);

}
