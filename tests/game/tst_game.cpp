#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>

#include <memory>

#include "racestyle.h"
#include "retroracecontroller.h"

Q_IMPORT_QML_PLUGIN(MaximumTrainer_GamePlugin)

class TstGame : public QObject
{
    Q_OBJECT

private slots:
    void animClock_followsWallTimeInReplayMode();
    void perSecondSetters_doNotAddFramesWhileTicking();
    void perSecondSetters_refreshWhenIdle();
    void roadSurface_drawsRoadAndVerge();
    void raceStyle_coloursMatchQtHelpers();
    void raceStyle_formatsNumbersAndNames();
};

void TstGame::animClock_followsWallTimeInReplayMode()
{
    RetroRaceController controller;
    controller.setLiveMode(false);   // race physics run 10x fast
    controller.useComputerPacer(180.0);
    controller.setVisualActive(true);
    controller.start();
    controller.beginRace();

    QElapsedTimer wall;
    wall.start();
    QTRY_VERIFY_WITH_TIMEOUT(controller.animClock() > 0.3, 3000);
    // Ambient motion must not speed up with the replay time scale.
    QVERIFY(controller.animClock() <= wall.elapsed() / 1000.0 + 0.05);
    QVERIFY(controller.visualDist() > 0.0);
}

void TstGame::perSecondSetters_doNotAddFramesWhileTicking()
{
    RetroRaceController controller;
    controller.useComputerPacer(180.0);
    controller.setVisualActive(true);
    controller.start();
    controller.beginRace();
    QVERIFY(controller.running());

    QSignalSpy frames(&controller, &RetroRaceController::updated);
    for (int second = 0; second < 50; ++second) {
        controller.setWorkoutElapsedSec(second);
        controller.setWorkoutProgress(second / 50.0);
        controller.setLiveHr(140.0);
    }
    QCOMPARE(frames.count(), 0);
    QCOMPARE(controller.workoutElapsedSec(), 49);
}

void TstGame::perSecondSetters_refreshWhenIdle()
{
    RetroRaceController controller;
    controller.useComputerPacer(180.0);
    controller.setVisualActive(true);
    QVERIFY(!controller.running());

    QSignalSpy frames(&controller, &RetroRaceController::updated);
    controller.setWorkoutElapsedSec(12);
    QCOMPARE(frames.count(), 1);
}

void TstGame::roadSurface_drawsRoadAndVerge()
{
    QQuickWindow window;
    window.resize(400, 300);
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import MaximumTrainer.Game
        Item {
            width: 400; height: 300
            Rectangle { anchors.fill: parent; color: "#0000ff" }
            RoadSurface { x: 0; y: 100; width: 400; height: 200; maxHalfWidth: 120; slices: 60; visualDist: 0 }
        })", QUrl());
    std::unique_ptr<QObject> root(component.create());
    QVERIFY2(root, qPrintable(component.errorString()));
    auto *rootItem = qobject_cast<QQuickItem *>(root.get());
    rootItem->setParentItem(window.contentItem());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QImage frame = window.grabWindow();
    QVERIFY(!frame.isNull());
    const qreal dpr = frame.devicePixelRatio();
    const auto pixel = [&](int x, int y) { return frame.pixelColor(qRound(x * dpr), qRound(y * dpr)); };

    // Sky above the road item is untouched.
    QCOMPARE(pixel(200, 50).rgb(), QColor("#0000ff").rgb());
    // Bottom centre is asphalt (grey), the far bottom edge is grass (green).
    const QColor asphalt = pixel(200, 290);
    QVERIFY2(asphalt.red() > 0x50 && asphalt.red() < 0x60 && asphalt.blue() > asphalt.green(),
             qPrintable(asphalt.name()));
    const QColor verge = pixel(5, 290);
    QVERIFY2(verge.green() > verge.red() && verge.green() > verge.blue(), qPrintable(verge.name()));
}

void TstGame::raceStyle_coloursMatchQtHelpers()
{
    // RaceStyle replaces Qt.tint/lighter/darker/rgba in RetroRace.qml, so the
    // scene must look the same: compare against Qt's own helpers.
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        QtObject {
            property color tinted: Qt.tint("#5a3a1f", Qt.rgba(0.62, 0.76, 0.81, 0.3))
            property color tintedTranslucentBase: Qt.tint("#805a3a1f", Qt.rgba(0.62, 0.76, 0.81, 0.3))
            property color light: Qt.lighter("#2f7d32", 1.16)
            property color dark: Qt.darker("#e23b3b", 1.3)
            property color fromRgba: Qt.rgba(0.72, 0.4, 0.1, 1)
        })", QUrl());
    std::unique_ptr<QObject> reference(component.create());
    QVERIFY2(reference, qPrintable(component.errorString()));

    RaceStyle style;
    const auto near = [](const QColor &actual, const QColor &expected) {
        return qAbs(actual.redF() - expected.redF()) < 0.002f
            && qAbs(actual.greenF() - expected.greenF()) < 0.002f
            && qAbs(actual.blueF() - expected.blueF()) < 0.002f
            && qAbs(actual.alphaF() - expected.alphaF()) < 0.002f;
    };
    const auto expected = [&](const char *name) { return reference->property(name).value<QColor>(); };
    const QColor haze = style.rgba(0.62, 0.76, 0.81, 0.3);

    QVERIFY(near(style.tint(QColor("#5a3a1f"), haze), expected("tinted")));
    QVERIFY(near(style.tint(QColor("#805a3a1f"), haze), expected("tintedTranslucentBase")));
    QVERIFY(near(style.lighter(QColor("#2f7d32"), 1.16), expected("light")));
    QVERIFY(near(style.darker(QColor("#e23b3b"), 1.3), expected("dark")));
    QVERIFY(near(style.rgba(0.72, 0.4, 0.1, 1), expected("fromRgba")));
    QCOMPARE(style.tint(QColor("#5a3a1f"), style.rgba(0, 0, 0, 0)), QColor("#5a3a1f"));
    QVERIFY(near(style.mix(QColor("#000000"), QColor("#ffffff"), 0.5), QColor::fromRgbF(0.5f, 0.5f, 0.5f)));
}

void TstGame::raceStyle_formatsNumbersAndNames()
{
    RaceStyle style;
    QCOMPARE(style.fixed(12.345, 2), QStringLiteral("12.35"));
    QCOMPARE(style.fixed(27.0, 1), QStringLiteral("27.0"));
    QCOMPARE(style.shortName(QStringLiteral("Pacer"), 12), QStringLiteral("Pacer"));
    QCOMPARE(style.shortName(QStringLiteral("2026-11-18 Threshold.fit"), 12),
             QStringLiteral("2026-11-18 ") + QChar(0x2026));
}

QTEST_MAIN(TstGame)
#include "tst_game.moc"
