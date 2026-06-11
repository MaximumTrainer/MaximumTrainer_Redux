import QtQuick
import QtQuick.Shapes

// Pseudo-3D (OutRun-style) retro ghost race. The road recedes to a vanishing
// point (slice-based perspective — still 2D, no real 3D), the opponent appears
// up the road ahead when you are chasing, and the rider is seen from behind.
// All data comes from RetroRaceController (context property `race`).
Item {
    id: root
    width: 960
    height: 540
    focus: true

    readonly property real horizonY: Math.round(height * 0.40)
    readonly property real roadH:    height - horizonY
    readonly property real maxHalfW: width * 0.60
    function fmtTime(s) { var m = Math.floor(s / 60); var ss = Math.floor(s % 60); return m + ":" + (ss < 10 ? "0" : "") + ss; }
    // Deterministic pseudo-random in [0,1) from an index (stable per object).
    function rnd(n) { var x = Math.sin(n * 127.1) * 43758.5453; return x - Math.floor(x); }

    // Curves disabled for now (straight road) — the winding look fought the
    // sprite. Restore the sine sum below to re-enable bends later.
    function curveAt(z) { return 0.0; }
    function roadCx(p, z) { return width / 2 + curveAt(z) * (1 - p) * width * 0.62; }
    readonly property real nearCurve: curveAt(38 * 2.2 + race.visualDist)
    readonly property real leanDeg: nearCurve * 13

    // Slope disabled for now (flat road) while we get the graphics right.
    // Restore: 34 * (1 - p) * Math.sin(p * 9.0 + vd * 0.018)
    function hillYAt(p, vd) { return 0; }

    // ---------------------------------------------------------------- components
    // Cyclist seen from behind, scalable, with pumping legs driven by cadence.
    // Cyclist seen from behind: a hunched back is the dominant mass, the helmet
    // is small and tucked low/forward (so it never reads as a face), legs pump
    // at the sides of the rear wheel. `lean` tilts the whole rider into a turn.
    component BackBike: Item {
        id: bike
        property real  s: 1.0
        property color body:   "#e23b3b"   // jersey
        property color helmet: "#f2f2f2"
        property color skin:   "#d9a071"
        property real  alpha: 1.0
        property real  phase: 0
        property real  lean: 0
        width: 58 * s; height: 74 * s; opacity: alpha
        rotation: lean; transformOrigin: Item.Bottom
        readonly property real bw: width
        readonly property real bh: height
        // legs pump 180° out of phase, driven by crank revolutions (real cadence)
        readonly property real amp: bh * 0.055 * Math.sin(phase * 6.2832)

        // ground shadow (grounds the rider on the road)
        Rectangle { x: bike.bw*0.16; y: bike.bh*0.93; width: bike.bw*0.68; height: bike.bh*0.09
                    radius: bike.bh*0.045; color: "#000000"; opacity: 0.26 }
        // rear wheel + a lighter rim stripe down the middle
        Rectangle { x: bike.bw*0.42;  y: bike.bh*0.60; width: bike.bw*0.16; height: bike.bh*0.40; radius: bike.bw*0.05; color: "#16161a" }
        Rectangle { x: bike.bw*0.485; y: bike.bh*0.63; width: bike.bw*0.03; height: bike.bh*0.34; color: "#73737d" }
        // left leg: thigh (shorts) + calf (skin) + shoe, all pumping together
        Rectangle { x: bike.bw*0.24; y: bike.bh*0.52 + bike.amp; width: bike.bw*0.17; height: bike.bh*0.18; color: "#23232e" }
        Rectangle { x: bike.bw*0.26; y: bike.bh*0.66 + bike.amp; width: bike.bw*0.12; height: bike.bh*0.12; color: bike.skin }
        Rectangle { x: bike.bw*0.24; y: bike.bh*0.76 + bike.amp; width: bike.bw*0.16; height: bike.bh*0.05; color: "#101014" }
        // right leg (opposite phase)
        Rectangle { x: bike.bw*0.59; y: bike.bh*0.52 - bike.amp; width: bike.bw*0.17; height: bike.bh*0.18; color: "#23232e" }
        Rectangle { x: bike.bw*0.62; y: bike.bh*0.66 - bike.amp; width: bike.bw*0.12; height: bike.bh*0.12; color: bike.skin }
        Rectangle { x: bike.bw*0.60; y: bike.bh*0.76 - bike.amp; width: bike.bw*0.16; height: bike.bh*0.05; color: "#101014" }
        // hips / saddle (dark shorts)
        Rectangle { x: bike.bw*0.26; y: bike.bh*0.46; width: bike.bw*0.48; height: bike.bh*0.12; color: "#1e1e28" }
        // jersey: lower back (widest) → mid back → shoulders (tapered, leaning in)
        Rectangle { x: bike.bw*0.23; y: bike.bh*0.34; width: bike.bw*0.54; height: bike.bh*0.16; color: bike.body }
        Rectangle { x: bike.bw*0.27; y: bike.bh*0.24; width: bike.bw*0.46; height: bike.bh*0.12; color: bike.body }
        Rectangle { x: bike.bw*0.30; y: bike.bh*0.17; width: bike.bw*0.40; height: bike.bh*0.10; color: Qt.darker(bike.body, 1.3) }
        // arms reaching forward to the bars
        Rectangle { x: bike.bw*0.18; y: bike.bh*0.24; width: bike.bw*0.10; height: bike.bh*0.17; radius: bike.bw*0.03
                    color: bike.skin; rotation: 13; transformOrigin: Item.Top }
        Rectangle { x: bike.bw*0.72; y: bike.bh*0.24; width: bike.bw*0.10; height: bike.bh*0.17; radius: bike.bw*0.03
                    color: bike.skin; rotation: -13; transformOrigin: Item.Top }
        // handlebar tips poking out at the sides
        Rectangle { x: bike.bw*0.12; y: bike.bh*0.31; width: bike.bw*0.09; height: bike.bh*0.05; color: "#2a2a30" }
        Rectangle { x: bike.bw*0.79; y: bike.bh*0.31; width: bike.bw*0.09; height: bike.bh*0.05; color: "#2a2a30" }
        // aero helmet (rounded) with a darker vent stripe
        Rectangle { x: bike.bw*0.38; y: bike.bh*0.05; width: bike.bw*0.24; height: bike.bh*0.16; radius: bike.bw*0.10; color: bike.helmet }
        Rectangle { x: bike.bw*0.485; y: bike.bh*0.06; width: bike.bw*0.04; height: bike.bh*0.13; color: Qt.darker(bike.helmet, 1.4) }
    }

    component BigButton: Rectangle {
        property string title
        property string subtitle
        property bool   active: true
        signal picked()
        width: 380; height: 84; radius: 10
        color: !active ? "#2a2a2a" : ma.containsMouse ? "#2f6196" : "#1f456b"
        border.color: "#9fd0ec"; border.width: 2; opacity: active ? 1.0 : 0.45
        Column {
            anchors.centerIn: parent; spacing: 4
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: title
                   color: "white"; font.family: "monospace"; font.pixelSize: 20; font.bold: true }
            Text { anchors.horizontalCenter: parent.horizontalCenter; visible: subtitle !== ""; text: subtitle
                   color: "#cde"; font.family: "monospace"; font.pixelSize: 12 }
        }
        MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true
            cursorShape: parent.active ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (parent.active) parent.picked() }
    }

    // ---------------------------------------------------------------- sky
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0;  color: "#1f3566" }
            GradientStop { position: 0.30; color: "#5f93cf" }
            GradientStop { position: 0.62; color: "#a9d8ef" }
            GradientStop { position: 1.0;  color: "#cdeaf6" }
        }
    }
    // warm low sun with a soft halo and a top-lit → orange gradient
    Rectangle { x: root.width*0.77 - 22; y: 38; width: 104; height: 104; radius: 52; color: "#ffd98a"; opacity: 0.30 }
    Rectangle { x: root.width*0.77;      y: 56; width: 60;  height: 60;  radius: 30
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#fff1b4" }
            GradientStop { position: 1.0; color: "#ffb259" }
        }
    }

    // a few distant birds (simple shallow chevrons) for a touch of life
    Repeater {
        model: 3
        Item {
            id: bird
            readonly property real bx: [236, 300, 520][index]
            readonly property real by: [118, 134, 98][index]
            readonly property real bs: [1.0, 0.8, 1.15][index]
            x: bx; y: by
            Rectangle { x: 0;            width: 13*bird.bs; height: 3*bird.bs; radius: 1.5; color: "#2a3c54"; rotation: 20; antialiasing: true }
            Rectangle { x: 9.5*bird.bs;  width: 13*bird.bs; height: 3*bird.bs; radius: 1.5; color: "#2a3c54"; rotation: -20; antialiasing: true }
        }
    }

    // drifting-looking soft clouds (static positions — cheap, reads well)
    Repeater {
        model: 4
        Item {
            id: cloud
            readonly property real px: [110, 360, 600, 840][index]
            readonly property real py: [66, 120, 54, 150][index]
            readonly property real sc: [1.0, 0.78, 1.12, 0.7][index]
            x: px; y: py
            Rectangle { x: 0;            y: 10*cloud.sc; width: 72*cloud.sc; height: 26*cloud.sc; radius: 13*cloud.sc; color: "#ffffff"; opacity: 0.82 }
            Rectangle { x: 24*cloud.sc;  y: 0;           width: 52*cloud.sc; height: 34*cloud.sc; radius: 17*cloud.sc; color: "#ffffff"; opacity: 0.9 }
            Rectangle { x: 52*cloud.sc;  y: 8*cloud.sc;  width: 46*cloud.sc; height: 24*cloud.sc; radius: 12*cloud.sc; color: "#eef4ff"; opacity: 0.82 }
        }
    }

    // Layered hills on the horizon — three depths (farthest bluish → near green).
    Repeater {   // farthest mountains (hazy blue)
        model: Math.ceil(root.width / 180) + 2
        Rectangle { width: 300; height: 130; radius: 150; color: "#7fa6bf"; opacity: 0.72
            x: index * 180 - 160; y: root.horizonY - 100 }
    }
    Repeater {   // mid hills (blue-green)
        model: Math.ceil(root.width / 150) + 2
        Rectangle { width: 240; height: 100; radius: 120; color: "#5a9078"
            x: index * 150 - 90; y: root.horizonY - 74 }
    }
    Repeater {   // near hills (green)
        model: Math.ceil(root.width / 140) + 2
        Rectangle { width: 214; height: 76; radius: 110; color: "#3f7a52"
            x: index * 140 - 50; y: root.horizonY - 46 }
    }
    // distant hazy hill band sitting right on the horizon (softens the skyline)
    Rectangle { anchors.left: parent.left; anchors.right: parent.right
        y: root.horizonY - 26; height: 30; color: "#4f8a63"; opacity: 0.6 }

    // Solid ground fill behind the slices (prevents sub-pixel sky gaps showing).
    Rectangle { anchors.left: parent.left; anchors.right: parent.right
        y: root.horizonY - 2; height: root.height - root.horizonY + 2; color: "#3c7548" }

    // ---------------------------------------------------------------- road
    // Classic pseudo-3D road built from horizontal slices. Each slice colours its
    // grass / asphalt / rumble / centre-dash by a world-segment parity that
    // scrolls with visualDist — so the red-and-white rumble strips and grass
    // bands rush toward you (the signature OutRun sense of speed).
    readonly property int slices: 120
    Repeater {
        model: root.slices
        Item {
            readonly property real p:    (index + 1) / root.slices            // 0 horizon .. 1 near
            readonly property int  seg:  Math.floor((1.0 / p) * 1.5 + race.visualDist * 0.22)
            readonly property bool even: ((seg % 2) + 2) % 2 === 0
            readonly property real hw:   root.maxHalfW * p                     // road half-width
            readonly property real cx:   root.width / 2
            readonly property real rW:   Math.max(2, hw * 0.16 + 3)           // rumble width
            readonly property real eW:   Math.max(1, hw * 0.028 + 1)          // edge-line width
            readonly property real cdW:  Math.max(2, hw * 0.05)               // centre-dash width
            readonly property bool haze: p < 0.18                             // far field: no flicker
            x: 0; width: root.width
            y: root.horizonY + (index / root.slices) * root.roadH
            height: root.roadH / root.slices + 1.4
            // grass verge
            Rectangle { anchors.fill: parent
                color: parent.haze ? "#3c7548" : (parent.even ? "#47935b" : "#3a8050") }
            // asphalt
            Rectangle { x: parent.cx - parent.hw; width: 2 * parent.hw; height: parent.height
                color: parent.even ? "#5b5b66" : "#55555f" }
            // rumble strips (red / white, alternating per segment)
            Rectangle { x: parent.cx - parent.hw - parent.rW; width: parent.rW; height: parent.height
                color: parent.even ? "#d83a3a" : "#f2f2f2" }
            Rectangle { x: parent.cx + parent.hw;             width: parent.rW; height: parent.height
                color: parent.even ? "#d83a3a" : "#f2f2f2" }
            // white edge lines
            Rectangle { x: parent.cx - parent.hw;             width: parent.eW; height: parent.height; color: "#f4f4f4" }
            Rectangle { x: parent.cx + parent.hw - parent.eW; width: parent.eW; height: parent.height; color: "#f4f4f4" }
            // centre dash (alternate segments only; skip the hazy far field)
            Rectangle { visible: parent.even && !parent.haze
                x: parent.cx - parent.cdW / 2; width: parent.cdW; height: parent.height; color: "#e9e9c4" }
        }
    }

    // Soft atmospheric haze at the horizon — blends the road and verge into the
    // distant hills so the far field recedes instead of ending on a hard line.
    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right
        y: root.horizonY - 6; height: 52
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#c9deec" }
            GradientStop { position: 0.5; color: "#9fc4d6" }
            GradientStop { position: 1.0; color: "#00000000" }
        }
        opacity: 0.55
    }

    // Roadside nature that rushes past and scales up as it approaches — a strong
    // depth/speed cue plus scenery. Three kinds (pine / leafy tree / bush) in
    // varied greens, scattered across the verge. Anchored in world distance.
    Repeater {
        model: 40
        Item {
            id: ob
            readonly property real span: 30 * 40
            readonly property real az: ((index * 30 - race.visualDist) % span + span) % span + 8  // distance ahead
            readonly property real p: Math.min(1.0, 38 / az)
            readonly property real r1: root.rnd(index * 1.3 + 3)
            readonly property int  kind: r1 < 0.30 ? 2 : (r1 < 0.62 ? 0 : 1)   // 2 bush · 0 pine · 1 leafy
            readonly property int  side: root.rnd(index * 2.1) < 0.5 ? -1 : 1
            // Lateral world offset from centre: 1.12× road-half (just off the
            // edge) out to ~3.7× (far across the grass). Scattered, not hugging.
            readonly property real latWorld: root.maxHalfW * (1.12 + root.rnd(index + 5) * 2.6)
            readonly property real cy: root.horizonY + p * root.roadH
            readonly property real baseW: kind === 2 ? (64 + root.rnd(index+1)*42)
                                       : kind === 0 ? (54 + root.rnd(index+2)*30)
                                                    : (62 + root.rnd(index+2)*42)
            readonly property real baseH: kind === 2 ? (38 + root.rnd(index+7)*22)
                                       : kind === 0 ? (155 + root.rnd(index+4)*110)
                                                    : (120 + root.rnd(index+4)*90)
            readonly property real tw: baseW * p
            readonly property real th: baseH * p
            readonly property color foliage: Qt.rgba(0.16 + root.rnd(index+9)*0.12,
                                                     0.44 + root.rnd(index+11)*0.18,
                                                     0.22 + root.rnd(index+13)*0.12, 1)
            // distance haze: 0 near .. 1 far, fades the object into the horizon
            readonly property real haz: Math.max(0, Math.min(1, (0.26 - p) / 0.24))
            visible: race.started && p > 0.02
            x: root.width/2 + side * p * latWorld - tw/2
            y: cy - th
            width: tw; height: th

            // ground shadow at the base
            Rectangle { x: ob.tw*0.06; y: ob.th*0.93; width: ob.tw*0.88; height: ob.th*0.06
                        radius: ob.th*0.03; color: "#000000"; opacity: 0.22 }
            // bush: low rounded clump (two blobs)
            Rectangle { visible: ob.kind===2; x: 0;          y: ob.th*0.22; width: ob.tw;      height: ob.th*0.78; radius: ob.tw*0.40; color: ob.foliage }
            Rectangle { visible: ob.kind===2; x: ob.tw*0.18; y: 0;          width: ob.tw*0.64; height: ob.th*0.50; radius: ob.tw*0.34; color: Qt.lighter(ob.foliage,1.16) }

            // trunk (both tree kinds)
            Rectangle { visible: ob.kind!==2; x: ob.tw*0.44; y: ob.th*0.66; width: ob.tw*0.12; height: ob.th*0.36; color: "#5a3a1f" }

            // pine: three tiers narrowing upward (conical)
            Rectangle { visible: ob.kind===0; x: ob.tw*0.10; y: ob.th*0.40; width: ob.tw*0.80; height: ob.th*0.32; radius: ob.tw*0.10; color: ob.foliage }
            Rectangle { visible: ob.kind===0; x: ob.tw*0.18; y: ob.th*0.20; width: ob.tw*0.64; height: ob.th*0.28; radius: ob.tw*0.10; color: Qt.lighter(ob.foliage,1.10) }
            Rectangle { visible: ob.kind===0; x: ob.tw*0.30; y: 0;          width: ob.tw*0.40; height: ob.th*0.26; radius: ob.tw*0.10; color: Qt.lighter(ob.foliage,1.20) }

            // leafy tree: round canopy in two blobs
            Rectangle { visible: ob.kind===1; x: 0;          y: ob.th*0.26; width: ob.tw;      height: ob.th*0.46; radius: ob.tw*0.40; color: ob.foliage }
            Rectangle { visible: ob.kind===1; x: ob.tw*0.16; y: 0;          width: ob.tw*0.68; height: ob.th*0.40; radius: ob.tw*0.36; color: Qt.lighter(ob.foliage,1.14) }

            // atmospheric haze tint — distant trees melt into the horizon
            Rectangle { anchors.fill: parent; color: "#a9cdd9"; opacity: ob.haz * 0.75 }
        }
    }

    // Roadside warning: a sign (flanked by bushes) rolls in on the right verge
    // as the next interval approaches, showing its target watts.
    Item {
        id: ivSign
        readonly property real aheadZ: race.nextSignZ - race.visualDist   // smooth (visualDist)
        readonly property real p: Math.min(1.0, 38 / Math.max(8, aheadZ))
        readonly property real halfw: root.maxHalfW * p
        readonly property real cy: root.horizonY + p * root.roadH
        readonly property real sw: 90 * p
        readonly property real sh: 60 * p
        visible: race.started && !race.finished && race.nextTargetW > 0
                 && race.nextSecs >= 0 && aheadZ > 3 && aheadZ < 150
        x: root.width/2 + (halfw + sw * 0.7)
        y: cy
        Rectangle { x: -3*ivSign.p; y: -46*ivSign.p; width: 6*ivSign.p; height: 46*ivSign.p; color: "#999999" }   // post
        Rectangle { x: -ivSign.sw/2; y: -46*ivSign.p - ivSign.sh; width: ivSign.sw; height: ivSign.sh
                    radius: 5*ivSign.p; color: "#16324a"; border.color: "#ffce3a"; border.width: Math.max(1, 2*ivSign.p) }
        Text { x: -ivSign.sw/2; y: -46*ivSign.p - ivSign.sh; width: ivSign.sw; height: ivSign.sh
               horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
               text: Math.round(race.nextTargetW) + "W"; color: "white"; font.family: "monospace"
               font.pixelSize: Math.max(6, 22*ivSign.p); font.bold: true }
        // bushes at the foot of the sign and across the road
        Rectangle { x: ivSign.sw*0.2; y: -16*ivSign.p; width: 30*ivSign.p; height: 18*ivSign.p; radius: 9*ivSign.p; color: "#357d40" }
        Rectangle { x: -(2*ivSign.halfw + 40*ivSign.p); y: -14*ivSign.p; width: 28*ivSign.p; height: 16*ivSign.p; radius: 8*ivSign.p; color: "#2f7a3a" }
    }

    // ---------------------------------------------------------------- riders
    // Opponent: shown up the road only while ahead of you (you are behind).
    BackBike {
        readonly property real ahead: -race.gapMeters
        readonly property real f: 30 / (30 + Math.max(0, ahead))     // 1 near .. ->0 far
        readonly property real op: 0.10 + 0.74 * f                   // band fraction up the road
        readonly property real oz: (1.0 / Math.max(0.03, op)) * 38 + race.visualDist
        visible: race.started && !race.finished && ahead > 1 && ahead < 220
        s: 0.45 + 1.35 * f
        body: "#7fd0ff"; helmet: "#15323f"; alpha: 0.85   // cyan so it stands out on grey + the dash
        phase: race.oppCrankRev
        lean: root.curveAt(oz) * 13
        x: root.roadCx(op, oz) - width / 2          // on the road, following the curve
        y: root.horizonY + root.roadH * (0.10 + 0.74 * f) - height
    }
    // "ghost ahead" marker chevron when the opponent is too far to render.
    Text {
        visible: race.started && !race.finished && (-race.gapMeters) >= 220
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.horizonY + 6
        text: "▲ " + race.oppName; color: "#e8e8ff"; font.family: "monospace"; font.pixelSize: 12; font.bold: true
    }
    // Player: fixed near the bottom centre, large.
    BackBike {
        s: 2.15
        body: "#e23b3b"
        phase: race.playerCrankRev
        lean: root.leanDeg
        x: root.width/2 - width/2
        // Sit above the profile strip so the graph never hides the rider.
        y: root.height - height - (profileStrip.visible ? 78 : 12)
    }

    // ---------------------------------------------------------------- HUD
    Rectangle { anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 72; color: "#000000"; opacity: 0.45 }

    // Left: time / distance / speed.
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

    // Centre: live effort, coloured vs target (red = high, blue = low, white = in range).
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
            readonly property color vcolor: low ? "#5aa0ff" : high ? "#ff5a5a" : "#ffffff"
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: label
                   color: "#bcd"; font.family: "monospace"; font.pixelSize: 12; font.bold: true }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: value.toFixed(0) + " " + unit
                   color: vcolor; font.family: "monospace"; font.pixelSize: 34; font.bold: true }
            Text { anchors.horizontalCenter: parent.horizontalCenter; visible: hasTarget
                   text: "⌖ " + Math.round(target) + " " + unit
                         + (low ? "  ▼" + Math.round(target - value)
                              : high ? "  ▲" + Math.round(value - target) : "")
                   color: vcolor; font.family: "monospace"; font.pixelSize: 11; font.bold: true }
        }
        BigStat { label: "POWER";   value: race.playerPowerW;  unit: "W";   target: race.targetPower;   range: race.targetPowerRange }
        BigStat { label: "CADENCE"; value: race.playerCadence; unit: "rpm"; target: race.targetCadence; range: race.targetCadenceRange }
        BigStat { label: "HR";      value: race.playerHr;      unit: "bpm"; target: race.targetHr;      range: race.targetHrRange }
    }

    // Right: gap headline + battle bar.
    Column {
        anchors.right: parent.right; anchors.rightMargin: 18
        anchors.top: parent.top; anchors.topMargin: 8
        spacing: 4
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: race.gapMeters >= 0 ? "AHEAD +" + race.gapMeters.toFixed(0) + " m"
                                      : "BEHIND " + race.gapMeters.toFixed(0) + " m"
            color: race.gapMeters >= 0 ? "#5dff5d" : "#ff6b6b"
            font.family: "monospace"; font.pixelSize: 24; font.bold: true
        }
        Rectangle {
            width: 240; height: 8; radius: 4; color: "#ffffff"; opacity: 0.25
            anchors.horizontalCenter: parent.horizontalCenter
            Rectangle {
                readonly property real frac: Math.max(-1, Math.min(1, race.gapMeters / 60))
                height: parent.height; radius: 4
                color: frac >= 0 ? "#5dff5d" : "#ff6b6b"
                x: frac >= 0 ? parent.width/2 : parent.width/2 + frac * (parent.width/2)
                width: Math.abs(frac) * (parent.width/2)
            }
        }
    }

    // NEXT-interval target card (right side).
    Rectangle {
        visible: race.started && !race.finished && race.nextTargetW > 0 && race.nextSecs >= 0
        anchors.right: parent.right; anchors.rightMargin: 14
        anchors.top: parent.top; anchors.topMargin: 84
        width: 116; height: race.nextTargetCad > 0 ? 66 : 52; radius: 6
        color: "#16324a"; border.color: "#ffce3a"; border.width: 2
        Column {
            anchors.centerIn: parent; spacing: 0
            Text { anchors.horizontalCenter: parent.horizontalCenter
                   text: "NEXT ▸ " + Math.round(race.nextSecs) + "s"
                   color: "#ffce3a"; font.family: "monospace"; font.pixelSize: 11; font.bold: true }
            Text { anchors.horizontalCenter: parent.horizontalCenter
                   text: Math.round(race.nextTargetW) + " W"
                   color: "white"; font.family: "monospace"; font.pixelSize: 22; font.bold: true }
            Text { anchors.horizontalCenter: parent.horizontalCenter; visible: race.nextTargetCad > 0
                   text: Math.round(race.nextTargetCad) + " rpm"
                   color: "#7ee0ff"; font.family: "monospace"; font.pixelSize: 12 }
        }
    }

    // Sky panel — interval-remaining timer, interval message, mini-map.
    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top; anchors.topMargin: 84
        spacing: 3
        visible: race.started && !race.finished && race.nextSecs >= 0
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: "INTERVAL LEFT"; color: "#22456b"; font.family: "monospace"; font.pixelSize: 12; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: root.fmtTime(Math.max(0, race.nextSecs)); color: "#13314d"
               font.family: "monospace"; font.pixelSize: 38; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter; visible: race.intervalMessage.length > 0
               text: race.intervalMessage; color: "#1c4a22"; font.family: "monospace"; font.pixelSize: 16; font.bold: true }
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 220; height: 14
            readonly property real sc: (width/2 - 8) / 60
            Rectangle { anchors.fill: parent; radius: 7; color: "#000000"; opacity: 0.35 }
            Rectangle { width: 10; height: 10; radius: 5; color: "#e8e8ff"; anchors.verticalCenter: parent.verticalCenter
                        x: Math.max(3, Math.min(parent.width-13, parent.width/2 - 5 - race.gapMeters * parent.sc)) }
            Rectangle { width: 10; height: 10; radius: 5; color: "#e23b3b"; anchors.centerIn: parent }
        }
    }

    Text {
        anchors { bottom: parent.bottom; left: parent.left; leftMargin: 8; bottomMargin: 72 }
        text: (race.oppIsBot ? "🤖 " : "👻 ") + race.oppName + "   ·   " + race.oppPowerW.toFixed(0) + " W"
        color: "#e8e8e8"; font.family: "monospace"; font.pixelSize: 12
    }
    Text {
        anchors { bottom: parent.bottom; right: parent.right; rightMargin: 46; bottomMargin: 72 }
        visible: race.started && !race.running && !race.finished
        text: "⏸ PAUSED"; color: "#ffd166"; font.family: "monospace"; font.pixelSize: 14; font.bold: true
    }

    // Workout-profile strip ("what's next") + fullscreen toggle.
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
                x: index * bw; width: bw + 0.6
                height: Math.max(2, (val > 0 ? val : 0) * profileStrip.barArea)
                y: profileStrip.stripH - height - 4
                color: done ? "#5fae5f" : "#41708f"
            }
        }
        Rectangle { width: 2; color: "#ffce3a"; anchors { top: parent.top; bottom: parent.bottom }
                    x: race.workoutProgress * profileStrip.width }
        Rectangle {
            anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 6 }
            width: 30; height: 30; radius: 5; color: fsMa.containsMouse ? "#2f6196" : "#1f456b"
            border.color: "#9fd0ec"; border.width: 1
            Text { anchors.centerIn: parent; text: race.gameFullscreen ? "🗗" : "⛶"; color: "white"; font.pixelSize: 16 }
            MouseArea { id: fsMa; anchors.fill: parent; hoverEnabled: true
                cursorShape: Qt.PointingHandCursor; onClicked: race.requestFullscreenToggle() }
        }
    }

    // ---------------------------------------------------------------- overlays
    Rectangle { anchors.fill: parent; color: "#000000"; opacity: 0.5; visible: !race.started && !race.finished }

    Column {   // step 1: choose opponent
        anchors.centerIn: parent; spacing: 14
        visible: !race.oppChosen && !race.started && !race.finished
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "CHOOSE YOUR RACE"
               color: "#ffe08a"; font.family: "monospace"; font.pixelSize: 30; font.bold: true }
        BigButton { title: "🏁  Race your last performance"
            subtitle: race.hasLastRide ? "your most recent ride of this workout" : "no previous ride for this workout yet"
            active: race.hasLastRide; onPicked: race.chooseGhost() }
        BigButton { title: "🤖  Race the Pacer"; subtitle: "holds the workout's target watts"
            onPicked: race.choosePacer() }
    }
    Column {   // step 2: ready
        anchors.centerIn: parent; spacing: 8
        visible: race.oppChosen && !race.started && !race.finished
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "READY"
               color: "#ffe08a"; font.family: "monospace"; font.pixelSize: 42; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Start the workout to begin the race"
               color: "white"; font.family: "monospace"; font.pixelSize: 16 }
    }

    Repeater {   // confetti
        model: 28
        Rectangle {
            visible: race.finished
            width: 8; height: 8
            color: ["#e23b3b", "#5dff5d", "#3aa0ff", "#ffe08a", "#ff6bd6"][index % 5]
            x: Math.random() * root.width; y: -20
            NumberAnimation on y { running: race.finished; loops: Animation.Infinite
                from: -20; to: root.height + 20; duration: 1700 + (index % 7) * 280 }
            NumberAnimation on rotation { running: race.finished; loops: Animation.Infinite
                from: 0; to: 360; duration: 700 + (index % 5) * 220 }
        }
    }
    Rectangle { anchors.fill: parent; color: "#000000"; opacity: 0.4; visible: race.finished }
    Column {
        anchors.centerIn: parent; spacing: 10; visible: race.finished
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "🏁  FINISH!"
               color: "#ffe08a"; font.family: "monospace"; font.pixelSize: 46; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: race.gapMeters >= 0 ? "You won by " + race.gapMeters.toFixed(0) + " m! 🎉"
                                         : "Beaten by " + (-race.gapMeters).toFixed(0) + " m"
               color: race.gapMeters >= 0 ? "#5dff5d" : "#ff6b6b"
               font.family: "monospace"; font.pixelSize: 22; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: (race.playerDistanceM / 1000).toFixed(2) + " km   ·   vs " + race.oppName
               color: "#cde"; font.family: "monospace"; font.pixelSize: 14 }
    }
}
