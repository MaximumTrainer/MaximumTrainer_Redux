#ifndef GLOBALVARS_H
#define GLOBALVARS_H

#include <QObject>
#include <QApplication>

#include <QNetworkReply>
#ifndef GC_WASM_BUILD
#include <QSslError>
#endif

class GlobalVars : public QObject
{
    Q_OBJECT
public:
    explicit GlobalVars(QObject *parent = nullptr);

signals:

public slots:
#ifndef GC_WASM_BUILD
    void sslErrorHandler(QNetworkReply* qnr, const QList<QSslError> & errlist);
#endif

};

#endif // GLOBALVARS_H
