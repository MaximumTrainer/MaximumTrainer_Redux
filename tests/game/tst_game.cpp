#include <QtTest>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>

#include "retroracecontroller.h"

class TstGame : public QObject
{
    Q_OBJECT

private slots:
    void animClock_followsWallTimeInReplayMode();
    void perSecondSetters_doNotAddFramesWhileTicking();
    void perSecondSetters_refreshWhenIdle();
    void roadSurface_drawsRoadAndVerge();
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

QTEST_MAIN(TstGame)
#include "tst_game.moc"
