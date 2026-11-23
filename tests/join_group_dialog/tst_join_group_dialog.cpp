/*
 * Regression guard for the 2026-11-23 report: pressing Enter in the room-code
 * box started the join, which disabled the inputs and moved focus to Cancel;
 * the same key press then reached the dialog and clicked Cancel, closing the
 * dialog and dropping the join.
 */
#include <QApplication>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include "joingroupdialog.h"

class TstJoinGroupDialog : public QObject
{
    Q_OBJECT

private slots:
    void enterInRoomCodeStartsJoinWithoutClosingTheDialog();
};

void TstJoinGroupDialog::enterInRoomCodeStartsJoinWithoutClosingTheDialog()
{
    QPointer<JoinGroupDialog> dialog =
        new JoinGroupDialog(QUrl(QStringLiteral("ws://127.0.0.1:9")), QStringLiteral("Rider"));
    QSignalSpy rejectedSpy(dialog.data(), &QDialog::rejected);
    dialog->show();
    QVERIFY(QTest::qWaitForWindowActive(dialog.data()));

    auto *codeEdit = dialog->findChildren<QLineEdit *>().value(0);
    QVERIFY(codeEdit);
    codeEdit->setFocus();
    QTest::keyClicks(codeEdit, QStringLiteral("6VXX"));
    QTest::keyClick(codeEdit, Qt::Key_Return);

    QCOMPARE(rejectedSpy.count(), 0);
    QVERIFY(dialog);
    QVERIFY(dialog->isVisible());
    QVERIFY(!codeEdit->isEnabled());
    delete dialog;
}

QTEST_MAIN(TstJoinGroupDialog)
#include "tst_join_group_dialog.moc"
