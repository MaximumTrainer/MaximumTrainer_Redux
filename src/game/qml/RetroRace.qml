import QtQuick

// Retro side-scrolling ghost race. Camera locked on the player (screen-centre);
// the opponent slides relative by the distance gap. Pixel-art sprites + layered
// parallax scenery, all primitives — no external assets (open-source / offline).
Item {
    id: root
    width: 960
    height: 540
    focus: true

    readonly property real pxPerM: 9.0                  // world scale (scroll + gaps)
    readonly property real roadY: height - 130          // top of the road (tidy strip)
    function screenX(distM) { return width * 0.5 + (distM - race.playerDistanceM) * pxPerM }
    function fmtTime(s) { var m = Math.floor(s / 60); var ss = Math.floor(s % 60); return m + ":" + (ss < 10 ? "0" : "") + ss; }

    // ---------------------------------------------------------------- pixel art
    // 18×13 cyclist, two pedalling frames. Chars: H helmet, K skin, J jersey,
    // A arm(skin), F frame, S shoe, W tyre, '.' transparent.
    readonly property var cyclistA: [
        "............HHH...",
        "...........HKKKH..",
        "..........KKKK....",
        ".......JJJJJK.....",
        "......JJJMMJAA....",
        ".....JJJJMMJA.....",
        "....JJJJ..F.......",
        "...LL..FF..F......",
        "..WWW..L.F...WWW..",
        ".W...W.SSF..W...W.",
        ".W.h.W...F..W.h.W.",
        ".W...W......W...W.",
        "..WWW........WWW.."
    ]
    readonly property var cyclistB: [
        "............HHH...",
        "...........HKKKH..",
        "..........KKKK....",
        ".......JJJJJK.....",
        "......JJJMMJAA....",
        ".....JJJJMMJA.....",
        "....JJJJ..F.......",
        "....L..FF.LF......",
        "..WWW...LF...WWW..",
        ".W...W..SSF.W...W.",
        ".W.h.W...F..W.h.W.",
        ".W...W......W...W.",
        "..WWW........WWW.."
    ]

    component PixelBike: Item {
        property var  frames: root.cyclistA
        property var  altFrames: root.cyclistB
        property real phase: 0          // drives the pedal animation
        property color body: "#e23b3b"
        property real  cell: 9          // zoomed-in rider
        property real  alpha: 1.0
        readonly property var rows: (Math.floor(phase) % 2 === 0) ? frames : altFrames
        opacity: alpha
        width: 18 * cell
        height: 13 * cell
        Repeater {
            model: 18 * 13
            Rectangle {
                readonly property int r: Math.floor(index / 18)
                readonly property int c: index % 18
                readonly property string ch: parent.rows[r][c]
                visible: ch !== "."
                x: c * parent.cell
                y: r * parent.cell
                width: parent.cell
                height: parent.cell
                color: ch === "W" ? "#1c1c1c"
                     : ch === "K" || ch === "A" ? "#f1c27d"
                     : ch === "H" ? "#2b3440"
                     : ch === "S" ? "#111111"
                     : ch === "M" ? "#ffffff"     // jersey accent stripe
                     : ch === "h" ? "#555555"     // wheel hub
                     : parent.body
            }
        }
    }

    // Big menu button for the pre-start opponent chooser.
    component BigButton: Rectangle {
        property string title
        property string subtitle
        property bool   active: true
        signal picked()
        width: 380; height: 84; radius: 10
        color: !active ? "#2a2a2a"
             : ma.containsMouse ? "#2f6196" : "#1f456b"
        border.color: "#9fd0ec"; border.width: 2
        opacity: active ? 1.0 : 0.45
        Column {
            anchors.centerIn: parent; spacing: 4
            Text { anchors.horizontalCenter: parent.horizontalCenter
                   text: title; color: "white"; font.family: "monospace"; font.pixelSize: 20; font.bold: true }
            Text { anchors.horizontalCenter: parent.horizontalCenter
                   visible: subtitle !== ""; text: subtitle
                   color: "#cde"; font.family: "monospace"; font.pixelSize: 12 }
        }
        MouseArea {
            id: ma; anchors.fill: parent; hoverEnabled: true
            cursorShape: parent.active ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (parent.active) parent.picked()
        }
    }

    // Race finish gantry: two posts, a checkered overhead banner, and a tape
    // stretched across the road at chest height for the rider to break.
    component FinishGantry: Item {
        readonly property real topY: root.roadY - 174
        Rectangle { x: -98; y: topY; width: 12; height: 198; color: "#c0392b" }   // left post
        Rectangle { x:  86; y: topY; width: 12; height: 198; color: "#c0392b" }   // right post
        Grid {                                                                    // checkered banner
            x: -98; y: topY - 2; columns: 16; rows: 2; spacing: 0
            Repeater {
                model: 32
                Rectangle { width: 12; height: 12
                    color: (((index % 16) + Math.floor(index / 16)) % 2 === 0) ? "#111" : "#fff" }
            }
        }
        Text {
            x: -98; y: topY - 24; width: 196; horizontalAlignment: Text.AlignHCenter
            text: "FINISH"; color: "#ffe14d"; font.family: "monospace"; font.pixelSize: 17; font.bold: true
        }
        Rectangle { x: -98; y: root.roadY - 52; width: 196; height: 7; radius: 3; color: "#ffe14d" } // tape
        Rectangle { x: -98; y: root.roadY - 52; width: 196; height: 2; color: "#fff8c0" }            // highlight
    }

    // ---------------------------------------------------------------- sky
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#243b66" }
            GradientStop { position: 0.55; color: "#4f7bb0" }
            GradientStop { position: 1.0; color: "#9fd0ec" }
        }
    }
    Rectangle { x: 770; y: 48; width: 66; height: 66; radius: 33; color: "#ffe7a0" } // sun

    // Drifting blocky clouds (slow parallax). Each item wraps independently so
    // it only appears/disappears off-screen (no mid-screen pop).
    Repeater {
        model: 9
        Item {
            readonly property real spacing: 320
            readonly property real period: spacing * 9
            y: 50 + (index % 3) * 46
            x: ((index * spacing - race.visualDist * root.pxPerM * 0.08) % period + period) % period - 120
            Rectangle { x: 0;  y: 10; width: 90; height: 22; radius: 11; color: "#ffffff"; opacity: 0.85 }
            Rectangle { x: 26; y: 0;  width: 54; height: 30; radius: 15; color: "#ffffff"; opacity: 0.85 }
        }
    }

    // Stepped 8-bit mountains (parallax).
    Repeater {
        model: 10
        Column {
            readonly property real spacing: 300
            readonly property real period: spacing * 10
            x: ((index * spacing - race.visualDist * root.pxPerM * 0.18) % period + period) % period - 260
            y: root.roadY - 250
            Repeater {
                model: 9
                Rectangle {
                    width: 220 - index * 24
                    height: 16
                    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
                    color: index < 2 ? "#cfe6f2" : "#3f5d72"   // snow cap
                }
            }
        }
    }

    // Rolling green hills (tall, to keep the sky band small).
    Repeater {
        model: 12
        Rectangle {
            readonly property real spacing: 240
            readonly property real period: spacing * 12
            width: 300; height: 270; radius: 150; color: "#3f7a4a"
            y: root.roadY - 205
            x: ((index * spacing - race.visualDist * root.pxPerM * 0.45) % period + period) % period - 320
        }
    }

    // ---------------------------------------------------------------- roadside
    component Tree: Item {
        width: 50; height: 88
        Rectangle { x: 21; y: 56; width: 10; height: 34; color: "#5a3a1f" }           // trunk
        Rectangle { x: 6;  y: 26; width: 38; height: 26; color: "#2f6f38" }           // canopy (stepped)
        Rectangle { x: 12; y: 9;  width: 26; height: 24; color: "#357d40" }
        Rectangle { x: 18; y: -3; width: 14; height: 18; color: "#3c8a47" }
    }

    // Trees just behind the road.
    Repeater {
        model: 16
        Tree {
            readonly property real spacing: 190
            readonly property real period: spacing * 16
            y: root.roadY - 80
            x: ((index * spacing - race.visualDist * root.pxPerM) % period + period) % period - 70
        }
    }

    // ---------------------------------------------------------------- road
    Rectangle { anchors.left: parent.left; anchors.right: parent.right
        y: root.roadY; height: parent.height - root.roadY; color: "#56565f" }
    Rectangle { anchors.left: parent.left; anchors.right: parent.right
        y: root.roadY - 7; height: 7; color: "#46b04a" }                  // grass lip
    Rectangle { anchors.left: parent.left; anchors.right: parent.right
        y: root.roadY - 11; height: 4; color: "#3b8f3e" }
    // Centre dashes (full speed).
    Repeater {
        model: Math.ceil(root.width / 104) + 2
        Rectangle {
            width: 64; height: 12; color: "#e9e9c4"; y: root.roadY + 58
            x: index * 104 - ((race.visualDist * root.pxPerM) % 104)
        }
    }
    // Fence posts on the verge (sense of speed).
    Repeater {
        model: Math.ceil(root.width / 82) + 2
        Rectangle {
            width: 7; height: 30; color: "#caa46a"; y: root.roadY - 26
            x: index * 82 - ((race.visualDist * root.pxPerM) % 82)
        }
    }

    // Distance markers every 1 km (gamification: progress along the road).
    Repeater {
        model: 6
        Item {
            readonly property int km: Math.floor(race.playerDistanceM / 1000) - 1 + index
            readonly property real sx: root.screenX(km * 1000)
            visible: km >= 1 && sx > -40 && sx < root.width + 40
            x: sx; y: root.roadY - 46
            Rectangle { x: -2; y: 16; width: 5; height: 30; color: "#888" }       // post
            Rectangle { x: -16; y: 0; width: 34; height: 18; radius: 3; color: "#1f6feb"; border.color: "#fff"; border.width: 2 }
            Text { x: -16; y: 1; width: 34; height: 16; horizontalAlignment: Text.AlignHCenter
                   text: km + "K"; color: "white"; font.family: "monospace"; font.pixelSize: 11; font.bold: true }
        }
    }

    // Interval boundary markers — a bright line is painted across the road when
    // a new workout interval starts, then scrolls back as you ride on.
    Repeater {
        model: race.intervalMarks
        Item {
            // Anchored in warped scenery distance so it scrolls with the road decor.
            readonly property real sx: root.width * 0.5 + (modelData - race.visualDist) * root.pxPerM
            visible: sx > -30 && sx < root.width + 30
            x: sx
            Rectangle { x: -6; y: root.roadY - 8; width: 12; height: root.height - root.roadY + 8; color: "#ffce3a" }
            Rectangle { x: -8; y: root.roadY - 8; width: 2;  height: root.height - root.roadY + 8; color: "#7a5a00" }
            Rectangle { x:  6; y: root.roadY - 8; width: 2;  height: root.height - root.roadY + 8; color: "#7a5a00" }
            Rectangle { x: -2; y: root.roadY - 40; width: 4; height: 32; color: "#888" }              // flag pole
            Rectangle { x:  2; y: root.roadY - 40; width: 20; height: 13; color: "#ffce3a" }          // flag
        }
    }

    // Upcoming target sign on the road ahead — projected from your speed × the
    // time until the next interval, so it approaches and tells you what's coming.
    Item {
        id: nextSign
        // Position purely from time-to-next (not speed), so it enters from the
        // right edge and only ever moves left toward the rider as the interval
        // nears — no jitter from the speed ramp.
        readonly property real leadSec: 15
        readonly property real frac: Math.min(1, Math.max(0, race.nextSecs / leadSec))
        readonly property real edgeX: root.width - 54
        visible: race.started && !race.finished && race.nextTargetW > 0 && race.nextSecs >= 0
        x: root.width * 0.5 + frac * (edgeX - root.width * 0.5)
        Rectangle { x: -3;  y: root.roadY - 58;  width: 6;  height: 58; color: "#999999" }   // post
        Rectangle { x: -48; y: root.roadY - 110; width: 96; height: 52; radius: 6
                    color: "#16324a"; border.color: "#ffce3a"; border.width: 2 }             // board
        Column {
            x: -48; y: root.roadY - 107; width: 96; spacing: 0
            Text { width: 96; horizontalAlignment: Text.AlignHCenter
                   text: "NEXT ▸ " + Math.round(race.nextSecs) + "s"
                   color: "#ffce3a"; font.family: "monospace"; font.pixelSize: 10; font.bold: true }
            Text { width: 96; horizontalAlignment: Text.AlignHCenter
                   text: Math.round(race.nextTargetW) + " W"
                   color: "white"; font.family: "monospace"; font.pixelSize: 19; font.bold: true }
            Text { width: 96; horizontalAlignment: Text.AlignHCenter; visible: race.nextTargetCad > 0
                   text: Math.round(race.nextTargetCad) + " rpm"
                   color: "#7ee0ff"; font.family: "monospace"; font.pixelSize: 11 }
        }
    }

    // Finish gantry — appears at the moment the race ends, on the player (who is
    // screen-centred), so they break the tape. The race is time-based (it ends
    // with the workout / the ghost's run), so there is no fixed finish distance
    // to place it at beforehand.
    FinishGantry {
        visible: race.finished
        x: root.screenX(race.playerDistanceM)
    }

    // Speed lines — motion streaks over the road that get longer, brighter and
    // denser the faster you go (really kicking in past ~30 km/h).
    Repeater {
        model: 18
        Rectangle {
            readonly property real sp: Math.min(1.0, race.playerSpeedKmh / 38.0)
            visible: race.started && !race.finished && race.playerSpeedKmh > 10
            height: 3
            width: 40 + sp * 120 + (index % 4) * 18
            color: "#ffffff"
            opacity: 0.10 + sp * 0.35
            y: (root.roadY - 26) + ((index * 47) % (root.height - root.roadY + 16))
            // Same scroll rate as the road dashes — they share the road plane.
            x: root.width + 140
               - ((race.visualDist * root.pxPerM + index * 150) % (root.width + 300))
        }
    }

    // ---------------------------------------------------------------- riders
    PixelBike {   // opponent: translucent ghost (same look for last-ride and pacer)
        id: oppBike
        body: "#e8e8ff"; alpha: 0.55
        phase: race.oppCrankRev * 2          // two leg-swaps per crank revolution
        x: root.screenX(race.oppDistanceM) - width / 2
        y: root.roadY - height + 6 - bob
        property real bob: 2 * Math.abs(Math.sin(race.oppCrankRev * Math.PI * 2))
    }
    Text {   // small tag so the pacer ghost is distinguishable from your last ride
        visible: race.oppIsBot && !race.finished
        text: "PACER"
        color: "#e8e8ff"; font.family: "monospace"; font.pixelSize: 11; font.bold: true
        x: oppBike.x + oppBike.width / 2 - width / 2
        y: oppBike.y - 14
    }
    PixelBike {   // player: always centred
        body: "#e23b3b"
        phase: race.playerCrankRev * 2
        x: root.width / 2 - width / 2
        y: root.roadY - height + 6 - bob
        property real bob: 2 * Math.abs(Math.sin(race.playerCrankRev * Math.PI * 2))
    }

    // ---------------------------------------------------------------- HUD
    Rectangle { anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 72; color: "#000000"; opacity: 0.45 }

    // Left: secondary stats (time / distance / speed).
    Row {
        anchors { top: parent.top; left: parent.left; margins: 10 }
        spacing: 16
        component Stat: Column {
            property string label; property string value
            Text { text: label; color: "#bcd"; font.family: "monospace"; font.pixelSize: 11; font.bold: true }
            Text { text: value; color: "white"; font.family: "monospace"; font.pixelSize: 18; font.bold: true }
        }
        Stat { label: "TIME";  value: root.fmtTime(race.workoutElapsedSec) }
        Stat { label: "DIST";  value: (race.playerDistanceM / 1000).toFixed(2) + " km" }
        Stat { label: "SPEED"; value: race.playerSpeedKmh.toFixed(1) + " km/h" }
    }

    // Centre: your live effort, large and easy to read at a glance. Each value
    // is coloured by the workout target (reusing the alert thresholds): white =
    // no target, green = on target, red = too low (▼), orange = too high (▲).
    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top; anchors.topMargin: 3
        spacing: 34
        component BigStat: Column {
            property string label
            property real   value
            property string unit
            property real   target: -1
            property real   range: 0
            readonly property bool hasTarget: target > 0
            readonly property bool low:  hasTarget && value < target - range
            readonly property bool high: hasTarget && value > target + range
            // Match the bottom widgets: red = too high, blue = too low, white = in range.
            readonly property color vcolor: low ? "#5aa0ff" : high ? "#ff5a5a" : "#ffffff"
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: label
                   color: "#bcd"; font.family: "monospace"; font.pixelSize: 12; font.bold: true }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: value.toFixed(0) + " " + unit
                   color: vcolor; font.family: "monospace"; font.pixelSize: 34; font.bold: true }
            Text { anchors.horizontalCenter: parent.horizontalCenter; visible: hasTarget
                   text: "⌖ " + Math.round(target) + " " + unit
                         + (low ? "  ▼" + Math.round(target - value)
                              : high ? "  ▲" + Math.round(value - target) : "")
                   color: vcolor
                   font.family: "monospace"; font.pixelSize: 11; font.bold: true }
        }
        BigStat { label: "POWER";   value: race.playerPowerW;  unit: "W";   target: race.targetPower;   range: race.targetPowerRange }
        BigStat { label: "CADENCE"; value: race.playerCadence; unit: "rpm"; target: race.targetCadence; range: race.targetCadenceRange }
        BigStat { label: "HR";      value: race.playerHr;      unit: "bpm"; target: race.targetHr;      range: race.targetHrRange }
    }

    // Gap headline + a swinging tug-of-war bar (works for ghost and bot).
    Column {
        anchors.right: parent.right; anchors.rightMargin: 18
        anchors.top: parent.top; anchors.topMargin: 8
        spacing: 4
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: race.gapMeters >= 0
                  ? "AHEAD +" + race.gapMeters.toFixed(0) + " m"
                  : "BEHIND " + race.gapMeters.toFixed(0) + " m"
            color: race.gapMeters >= 0 ? "#5dff5d" : "#ff6b6b"
            font.family: "monospace"; font.pixelSize: 24; font.bold: true
        }
        Rectangle {   // battle bar: centre = even, fills toward whoever leads
            width: 240; height: 8; radius: 4; color: "#ffffff"; opacity: 0.25
            anchors.horizontalCenter: parent.horizontalCenter
            Rectangle {
                readonly property real frac: Math.max(-1, Math.min(1, race.gapMeters / 60))
                height: parent.height; radius: 4
                color: frac >= 0 ? "#5dff5d" : "#ff6b6b"
                x: frac >= 0 ? parent.width / 2 : parent.width / 2 + frac * (parent.width / 2)
                width: Math.abs(frac) * (parent.width / 2)
            }
        }
    }

    // Route progress bar (recorded routes): you vs opponent toward the finish.
    Item {
        visible: race.routeLengthM > 0
        anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 70 }
        anchors.leftMargin: 16; anchors.rightMargin: 16
        height: 16
        Rectangle { id: track; anchors.fill: parent; radius: 7; color: "#000"; opacity: 0.4 }
        Text { anchors.right: parent.right; anchors.top: parent.bottom
               text: Math.min(100, race.playerDistanceM / race.routeLengthM * 100).toFixed(0) + "%  🏁"
               color: "#cde"; font.family: "monospace"; font.pixelSize: 11 }
        Rectangle {   // opponent marker
            width: 4; height: parent.height; color: "#cccccc"
            x: Math.min(1, race.oppDistanceM / race.routeLengthM) * (parent.width - 4)
        }
        Rectangle {   // player marker
            width: 6; height: parent.height + 6; y: -3; radius: 2; color: "#e23b3b"
            x: Math.min(1, race.playerDistanceM / race.routeLengthM) * (parent.width - 6)
        }
    }

    // Sky panel — fills the empty upper space with pacing info: time left in the
    // current interval and the interval's message.
    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top; anchors.topMargin: 100
        spacing: 4
        visible: race.started && !race.finished && race.nextSecs >= 0
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: "INTERVAL LEFT"; color: "#cfe0f0"; font.family: "monospace"; font.pixelSize: 13; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: root.fmtTime(Math.max(0, race.nextSecs)); color: "#ffffff"
               font.family: "monospace"; font.pixelSize: 46; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter
               visible: race.intervalMessage.length > 0
               text: race.intervalMessage; color: "#ffe08a"
               font.family: "monospace"; font.pixelSize: 18; font.bold: true }
        // Whole-ride metrics.
        Row {
            anchors.horizontalCenter: parent.horizontalCenter; spacing: 18
            visible: race.np > 0 || race.tss > 0
            Text { text: "NP "  + Math.round(race.np);     color: "#cfe0f0"; font.family: "monospace"; font.pixelSize: 13; font.bold: true }
            Text { text: "IF "  + race.ifactor.toFixed(2); color: "#cfe0f0"; font.family: "monospace"; font.pixelSize: 13; font.bold: true }
            Text { text: "TSS " + Math.round(race.tss);    color: "#cfe0f0"; font.family: "monospace"; font.pixelSize: 13; font.bold: true }
        }
        // Mini-map: you (red, centred) vs opponent (ghost) by the distance gap.
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 230; height: 16
            readonly property real sc: (width / 2 - 8) / 60      // ±60 m fills the track
            Rectangle { anchors.fill: parent; radius: 8; color: "#000000"; opacity: 0.4 }
            Rectangle {   // opponent
                width: 11; height: 11; radius: 6; color: "#e8e8ff"
                anchors.verticalCenter: parent.verticalCenter
                x: Math.max(3, Math.min(parent.width - 14,
                       parent.width / 2 - 5 - race.gapMeters * parent.sc))
            }
            Rectangle { width: 11; height: 11; radius: 6; color: "#e23b3b"; anchors.centerIn: parent }  // you
        }
    }

    Text {
        anchors { bottom: parent.bottom; left: parent.left; leftMargin: 8; bottomMargin: 72 }
        text: (race.oppIsBot ? "🤖 " : "👻 ") + race.oppName
              + "   ·   " + race.oppPowerW.toFixed(0) + " W"
        color: "#e8e8e8"; font.family: "monospace"; font.pixelSize: 12
    }
    // Pause is driven by the workout (button / cadence / configured trigger).
    Text {
        anchors { bottom: parent.bottom; right: parent.right; rightMargin: 46; bottomMargin: 72 }
        visible: race.started && !race.running && !race.finished
        text: "⏸ PAUSED"
        color: "#ffd166"; font.family: "monospace"; font.pixelSize: 14; font.bold: true
    }

    // Built-in workout profile strip ("what's next") + fullscreen toggle, so the
    // game stands alone without the external graph / widgets.
    Item {
        id: profileStrip
        readonly property int stripH: 66
        readonly property int barArea: 56
        visible: race.workoutProfile.length > 0 && !race.finished
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: stripH
        Rectangle { anchors.fill: parent; color: "#000000"; opacity: 0.5 }
        Repeater {
            model: race.workoutProfile.length
            Rectangle {
                readonly property int n: race.workoutProfile.length
                readonly property real bw: profileStrip.width / Math.max(1, n)
                readonly property real val: race.workoutProfile[index]
                readonly property bool done: (index + 0.5) / n <= race.workoutProgress
                x: index * bw
                width: bw + 0.6
                height: Math.max(2, (val > 0 ? val : 0) * profileStrip.barArea)
                y: profileStrip.stripH - height - 4
                color: done ? "#5fae5f" : "#41708f"
            }
        }
        Rectangle {   // current position through the workout
            width: 2; color: "#ffce3a"
            anchors { top: parent.top; bottom: parent.bottom }
            x: race.workoutProgress * profileStrip.width
        }
        Rectangle {   // fullscreen toggle
            anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 6 }
            width: 30; height: 30; radius: 5
            color: fsMa.containsMouse ? "#2f6196" : "#1f456b"
            border.color: "#9fd0ec"; border.width: 1
            Text { anchors.centerIn: parent; text: race.gameFullscreen ? "🗗" : "⛶"; color: "white"; font.pixelSize: 16 }
            MouseArea { id: fsMa; anchors.fill: parent; hoverEnabled: true
                cursorShape: Qt.PointingHandCursor; onClicked: race.requestFullscreenToggle() }
        }
    }

    // ---------------------------------------------------------------- overlays
    // Pre-start dim.
    Rectangle {
        anchors.fill: parent; color: "#000000"; opacity: 0.5
        visible: !race.started && !race.finished
    }

    // Step 1 — choose your opponent.
    Column {
        anchors.centerIn: parent; spacing: 14
        visible: !race.oppChosen && !race.started && !race.finished
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: "CHOOSE YOUR RACE"; color: "#ffe08a"
               font.family: "monospace"; font.pixelSize: 30; font.bold: true }
        BigButton {
            title: "🏁  Race your last performance"
            subtitle: race.hasLastRide ? "your most recent ride of this workout"
                                       : "no previous ride for this workout yet"
            active: race.hasLastRide
            onPicked: race.chooseGhost()
        }
        BigButton {
            title: "🤖  Race the Pacer"
            subtitle: "holds the workout's target watts"
            onPicked: race.choosePacer()
        }
    }

    // Step 2 — chosen, waiting for the workout to start.
    Column {
        anchors.centerIn: parent; spacing: 8
        visible: race.oppChosen && !race.started && !race.finished
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: "READY"; color: "#ffe08a"; font.family: "monospace"; font.pixelSize: 42; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: "Start the workout to begin the race"; color: "white"; font.family: "monospace"; font.pixelSize: 16 }
    }

    // Finish: confetti + result.
    Repeater {
        model: 28
        Rectangle {
            visible: race.finished
            width: 8; height: 8
            color: ["#e23b3b", "#5dff5d", "#3aa0ff", "#ffe08a", "#ff6bd6"][index % 5]
            x: Math.random() * root.width
            y: -20
            NumberAnimation on y {
                running: race.finished; loops: Animation.Infinite
                from: -20; to: root.height + 20; duration: 1700 + (index % 7) * 280
            }
            NumberAnimation on rotation {
                running: race.finished; loops: Animation.Infinite
                from: 0; to: 360; duration: 700 + (index % 5) * 220
            }
        }
    }
    Rectangle { anchors.fill: parent; color: "#000000"; opacity: 0.4; visible: race.finished }
    Column {
        anchors.centerIn: parent; spacing: 10; visible: race.finished
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: "🏁  FINISH!"; color: "#ffe08a"; font.family: "monospace"; font.pixelSize: 46; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: race.gapMeters >= 0
                     ? "You won by " + race.gapMeters.toFixed(0) + " m! 🎉"
                     : "Beaten by " + (-race.gapMeters).toFixed(0) + " m"
               color: race.gapMeters >= 0 ? "#5dff5d" : "#ff6b6b"
               font.family: "monospace"; font.pixelSize: 22; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: (race.playerDistanceM / 1000).toFixed(2) + " km   ·   vs " + race.oppName
               color: "#cde"; font.family: "monospace"; font.pixelSize: 14 }
    }
}
