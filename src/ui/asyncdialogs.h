#ifndef ASYNCDIALOGS_H
#define ASYNCDIALOGS_H

#include <QAbstractButton>
#include <QMessageBox>
#include <functional>

/*
 * Non-blocking replacements for the static QMessageBox convenience functions.
 *
 * Those statics run QDialog::exec(), which spins a nested event loop — a
 * qFatal on Qt for WebAssembly without asyncify ("Calling exec() is not
 * supported..."), aborting the whole WASM runtime.  Every message box that can
 * be reached in the WASM build must therefore go through QDialog::open().
 * Used on all platforms so there is a single code path; on desktop the only
 * difference is that the call returns immediately instead of blocking.
 */
namespace AsyncDialogs {

inline QMessageBox *makeBox(QMessageBox::Icon icon, QWidget *parent,
                            const QString &title, const QString &text,
                            QMessageBox::StandardButtons buttons)
{
    auto *box = new QMessageBox(icon, title, text, buttons, parent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    return box;
}

inline void information(QWidget *parent, const QString &title, const QString &text)
{
    makeBox(QMessageBox::Information, parent, title, text, QMessageBox::Ok)->open();
}

inline void warning(QWidget *parent, const QString &title, const QString &text)
{
    makeBox(QMessageBox::Warning, parent, title, text, QMessageBox::Ok)->open();
}

/// Yes/No confirmation. onYes runs only when Yes is clicked; No, Esc and
/// closing the box all decline (No is the default button).
inline void question(QWidget *parent, const QString &title, const QString &text,
                     std::function<void()> onYes,
                     const QString &informativeText = QString())
{
    auto *box = makeBox(QMessageBox::Question, parent, title, text,
                        QMessageBox::Yes | QMessageBox::No);
    box->setDefaultButton(QMessageBox::No);
    if (!informativeText.isEmpty())
        box->setInformativeText(informativeText);
    QObject::connect(box, &QMessageBox::buttonClicked, box,
                     [box, onYes = std::move(onYes)](QAbstractButton *button) {
        if (box->standardButton(button) == QMessageBox::Yes)
            onYes();
    });
    box->open();
}

} // namespace AsyncDialogs

#endif // ASYNCDIALOGS_H
