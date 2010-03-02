function main()
{
    waitForObjectItem(":Psi_PsiContactListView", "default");
    clickItem(":Psi_PsiContactListView", "default", 119, 12, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 01");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 01", 119, 9, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 02");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 02", 118, 7, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 03");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 03", 120, 8, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 04");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 04", 120, 4, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 04");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 04", 117, 4, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 04.General");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 04.General", 100, 8, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 04.General");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 04.General", 100, 8, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 04");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 04", 106, 8, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 04");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 04", 94, 9, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 03");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 03", 96, 8, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 02");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 02", 99, 6, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "ya\\.ru 01");
    clickItem(":Psi_PsiContactListView", "ya\\.ru 01", 104, 6, 1, Qt.LeftButton);
    waitForObjectItem(":Psi_PsiContactListView", "default");
    clickItem(":Psi_PsiContactListView", "default", 105, 12, 1, Qt.LeftButton);
    waitForObject(":Psi_PsiApplication");
    sendEvent("QCloseEvent", ":Psi_PsiApplication");

}
