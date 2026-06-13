import QtQuick

// Modern live-metric strip — an alternative to the classic widget band.
// Context property `metrics` is a MetricStripModel (see metricstripmodel.h).
Rectangle {
    id: root
    color: "#1d1d1d"

    readonly property int pad: Math.max(8, height * 0.06)

    Row {
        anchors.centerIn: parent
        spacing: root.pad
        height: parent.height - root.pad * 2

        MetricCard {
            accent: "#ffd54f"; icon: "⚡"          // ⚡
            caption: "POWER"; unit: "W"
            value: metrics.power
            avg: metrics.avgPower; max: metrics.maxPower
            target: metrics.targetPower; range: metrics.targetRange
        }
        MetricCard {
            accent: "#64b5f6"; icon: "⚙"          // ⚙
            caption: "CADENCE"; unit: "rpm"
            value: metrics.cadence
            avg: metrics.avgCadence; max: metrics.maxCadence
        }
        MetricCard {
            accent: "#ef5350"; icon: "❤"          // ❤
            caption: "HEART RATE"; unit: "bpm"
            value: metrics.hr
            avg: metrics.avgHr; max: metrics.maxHr
        }
        MetricCard {
            accent: "#4dd0e1"; icon: "➤"          // ➤
            caption: "SPEED"; unit: "km/h"
            visible: metrics.hasSpeed
            value: Math.round(metrics.speed * 10) / 10
            decimals: 1
        }

        // Session summary: NP / IF / TSS / kcal / distance.
        Rectangle {
            width: 220
            height: Math.min(parent.height, 170)
            anchors.verticalCenter: parent.verticalCenter
            radius: 10
            color: "#262626"
            border { width: 1; color: "#333333" }

            Rectangle {
                width: 4; radius: 2
                anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 8 }
                color: "#9575cd"
            }

            Column {
                anchors { left: parent.left; right: parent.right;
                          verticalCenter: parent.verticalCenter;
                          leftMargin: 24; rightMargin: 14 }
                spacing: 6

                Text {
                    text: "SESSION"
                    color: "#8d8d8d"
                    font { family: "Inter"; pixelSize: 12; weight: Font.DemiBold; letterSpacing: 1.5 }
                }
                Grid {
                    columns: 2
                    columnSpacing: 26
                    rowSpacing: 4
                    Repeater {
                        model: [
                            { lbl: "NP",   v: metrics.np  > 0 ? Math.round(metrics.np).toString() : "–" },
                            { lbl: "IF",   v: metrics.intensity > 0 ? metrics.intensity.toFixed(2) : "–" },
                            { lbl: "TSS",  v: metrics.tss > 0 ? Math.round(metrics.tss).toString() : "–" },
                            { lbl: "KCAL", v: metrics.kcal > 0 ? Math.round(metrics.kcal).toString() : "–" },
                            { lbl: "KM",   v: metrics.distanceKm > 0.005 ? metrics.distanceKm.toFixed(2) : "–" }
                        ]
                        delegate: Row {
                            spacing: 6
                            Text { text: modelData.lbl; color: "#777777"; width: 38
                                   font { family: "Inter"; pixelSize: 11; weight: Font.DemiBold } }
                            Text { text: modelData.v; color: "#d6d6d6"
                                   font { family: "Inter"; pixelSize: 14; weight: Font.DemiBold } }
                        }
                    }
                }
            }
        }
    }

    component MetricCard: Rectangle {
        id: card
        property color  accent: "#ffffff"
        property string icon: ""
        property string caption: ""
        property string unit: ""
        property real   value: 0
        property int    decimals: 0
        property int    avg: 0
        property int    max: 0
        property int    target: 0
        property int    range: 0

        // Animated display value so jumps glide instead of snapping.
        property real shown: 0
        Behavior on shown { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }
        onValueChanged: shown = value

        readonly property bool inZone:
            target <= 0 || Math.abs(value - target) <= Math.max(1, range)

        width: 250
        height: Math.min(parent.height, 170)
        anchors.verticalCenter: parent.verticalCenter
        radius: 10
        color: "#262626"
        border.width: 1
        border.color: target <= 0 ? "#333333"
                    : card.inZone ? "#3f6647" : "#7a4a25"
        Behavior on border.color { ColorAnimation { duration: 300 } }

        // Accent edge
        Rectangle {
            width: 4; radius: 2
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 8 }
            color: card.accent
        }

        Column {
            anchors { left: parent.left; right: parent.right;
                      verticalCenter: parent.verticalCenter;
                      leftMargin: 24; rightMargin: 14 }
            spacing: 2

            Text {
                text: card.icon + "  " + card.caption
                color: "#8d8d8d"
                font { family: "Inter"; pixelSize: 12; weight: Font.DemiBold; letterSpacing: 1.5 }
            }

            Row {
                spacing: 6
                Text {
                    text: card.decimals > 0 ? card.shown.toFixed(card.decimals)
                                            : Math.round(card.shown).toString()
                    color: "#f2f2f2"
                    font { family: "Inter"; pixelSize: 44; weight: Font.DemiBold }
                }
                Text {
                    anchors.baseline: parent.children[0].baseline
                    text: card.unit
                    color: "#8d8d8d"
                    font { family: "Inter"; pixelSize: 16 }
                }
            }

            // Target band (power card only): position of the live value inside
            // target ± range, the zone drawn in the accent colour.
            Item {
                width: parent.width; height: 10
                visible: card.target > 0
                Rectangle {                      // groove
                    anchors.fill: parent
                    radius: 5; color: "#3a3a3a"
                }
                Rectangle {                      // target zone
                    readonly property real lo: card.target - card.range
                    readonly property real hi: card.target + card.range
                    readonly property real span: Math.max(1, (card.target + card.range * 3) - Math.max(0, card.target - card.range * 3))
                    x: parent.width * (lo - Math.max(0, card.target - card.range * 3)) / span
                    width: parent.width * (hi - lo) / span
                    height: parent.height; radius: 5
                    color: Qt.alpha(card.accent, 0.55)
                }
                Rectangle {                      // live value marker
                    readonly property real lo: Math.max(0, card.target - card.range * 3)
                    readonly property real span: Math.max(1, (card.target + card.range * 3) - lo)
                    x: Math.max(0, Math.min(parent.width - width,
                        parent.width * (card.shown - lo) / span))
                    width: 5; height: parent.height + 4; y: -2; radius: 2
                    color: "#ffffff"
                }
            }

            Row {
                spacing: 18
                topPadding: 4
                Repeater {
                    model: [ { lbl: "AVG", v: card.avg }, { lbl: "MAX", v: card.max } ]
                    delegate: Row {
                        spacing: 5
                        visible: modelData.v > 0
                        Text { text: modelData.lbl; color: "#777777"
                               font { family: "Inter"; pixelSize: 11; weight: Font.DemiBold } }
                        Text { text: modelData.v; color: "#bdbdbd"
                               font { family: "Inter"; pixelSize: 13; weight: Font.DemiBold } }
                    }
                }
            }
        }
    }
}
