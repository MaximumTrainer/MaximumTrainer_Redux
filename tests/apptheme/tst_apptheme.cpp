#include <QtTest>
#include <QApplication>
#include <QRegularExpression>

#include "apptheme.h"

// Unit tests for the AppTheme dark/light theming helper (issue #151).
class TstAppTheme : public QObject
{
    Q_OBJECT

private slots:
    void resolveMode_explicitModesPassThrough();
    void apply_setsNonEmptyStylesheet();
    void apply_lightAndDarkDiffer();
    void darkStylesheet_hasDarkWindowBackground();
    void stylesheets_containNoInvalidColours();
};

// Explicit Light/Dark must resolve to themselves (only System is OS-dependent).
void TstAppTheme::resolveMode_explicitModesPassThrough()
{
    QCOMPARE(AppTheme::resolveMode(AppTheme::Light), AppTheme::Light);
    QCOMPARE(AppTheme::resolveMode(AppTheme::Dark),  AppTheme::Dark);
}

void TstAppTheme::apply_setsNonEmptyStylesheet()
{
    AppTheme::apply(qApp, AppTheme::Dark);
    QVERIFY(!qApp->styleSheet().isEmpty());

    AppTheme::apply(qApp, AppTheme::Light);
    QVERIFY(!qApp->styleSheet().isEmpty());
}

void TstAppTheme::apply_lightAndDarkDiffer()
{
    AppTheme::apply(qApp, AppTheme::Light);
    const QString light = qApp->styleSheet();

    AppTheme::apply(qApp, AppTheme::Dark);
    const QString dark = qApp->styleSheet();

    QVERIFY2(light != dark, "Light and Dark stylesheets should not be identical");
}

void TstAppTheme::darkStylesheet_hasDarkWindowBackground()
{
    // The dark theme sets a dark default widget background (#2b2b2b).
    const QString dark = AppTheme::darkStylesheet();
    QVERIFY(dark.contains(QStringLiteral("#2b2b2b")));
}

// Every #rrggbb / rgb()/rgba() literal in both stylesheets must parse to a
// valid QColor — guards against typos that would render as invalid/transparent.
void TstAppTheme::stylesheets_containNoInvalidColours()
{
    const QStringList sheets{ AppTheme::lightStylesheet(), AppTheme::darkStylesheet() };

    QRegularExpression hexRe(QStringLiteral("#[0-9a-fA-F]{6}\\b"));
    QRegularExpression rgbRe(QStringLiteral("rgba?\\(([^)]*)\\)"));

    for (const QString &sheet : sheets) {
        auto hexIt = hexRe.globalMatch(sheet);
        while (hexIt.hasNext()) {
            const QString hex = hexIt.next().captured(0);
            QVERIFY2(QColor(hex).isValid(), qPrintable("Invalid hex colour: " + hex));
        }

        auto rgbIt = rgbRe.globalMatch(sheet);
        while (rgbIt.hasNext()) {
            const QStringList parts = rgbIt.next().captured(1).split(QLatin1Char(','), Qt::SkipEmptyParts);
            QVERIFY2(parts.size() == 3 || parts.size() == 4,
                     "rgb()/rgba() must have 3 or 4 components");
            for (int i = 0; i < parts.size(); ++i) {
                bool ok = false;
                const int v = parts.at(i).trimmed().toInt(&ok);
                QVERIFY(ok);
                QVERIFY(v >= 0 && v <= 255);
            }
        }
    }
}

QTEST_MAIN(TstAppTheme)
#include "tst_apptheme.moc"
