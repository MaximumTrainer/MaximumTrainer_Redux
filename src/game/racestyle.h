#ifndef RACESTYLE_H
#define RACESTYLE_H

#include <QColor>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

/// Typed colour and number-format helpers for RetroRace.qml. Qt.lighter/
/// Qt.tint/Qt.rgba and Number.toFixed return untyped values to the QML
/// compiler, so every binding using them fell back to the interpreter; these
/// keep those bindings compiled to C++.
class RaceStyle : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit RaceStyle(QObject *parent = nullptr);

    Q_INVOKABLE QColor rgba(double red, double green, double blue, double alpha = 1.0) const;
    Q_INVOKABLE QColor lighter(const QColor &colour, double factor) const;
    Q_INVOKABLE QColor darker(const QColor &colour, double factor) const;
    /// Same blend as Qt.tint: overlay drawn on top of base with its own alpha.
    Q_INVOKABLE QColor tint(const QColor &base, const QColor &overlay) const;
    /// Linear blend from a (t = 0) to b (t = 1), opaque.
    Q_INVOKABLE QColor mix(const QColor &from, const QColor &to, double t) const;
    /// Number.toFixed equivalent.
    Q_INVOKABLE QString fixed(double value, int decimals) const;
    /// Truncates long names (ghost names can be whole history filenames).
    Q_INVOKABLE QString shortName(const QString &name, int maxLength) const;
};

#endif // RACESTYLE_H
