function main()
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
    
    test.verify(object.exists(":Psi: Account Properties_AccountModifyDlg"))
}
