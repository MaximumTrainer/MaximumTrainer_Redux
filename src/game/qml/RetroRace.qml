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

    // Free-running wall clock (≈ seconds) that drives ambient motion (clouds,
    // birds) independently of the race physics tick.
    property real animT: 0
    NumberAnimation on animT { from: 0; to: 100000; duration: 100000000; loops: Animation.Infinite; running: true }

    // Effort vs target → a screen-edge glow (gamifies holding the target):
    // red when over, blue when under, nothing when in the zone (calm = reward).
    readonly property int effortState: (race.targetPower <= 0 || !race.started || race.finished) ? 0
        : (race.playerPowerW > race.targetPower + race.targetPowerRange) ?  1
        : (race.playerPowerW < race.targetPower - race.targetPowerRange) ? -1 : 0
    readonly property color effortColor: effortState === 1 ? "#ff2a2a" : "#2f74ff"
    readonly property real effortGlow: effortState === 1 ? (0.52 + 0.28 * Math.sin(animT * 7.0))
                                     : effortState === -1 ? (0.46 + 0.16 * Math.sin(animT * 4.5)) : 0.0
    // How far out of the zone (0..1+), to scale the glow size with the miss.
    readonly property real effortMiss: race.targetPower <= 0 ? 0
        : Math.min(1.0, Math.abs(race.playerPowerW - race.targetPower) / Math.max(40, race.targetPower * 0.35))
    // Race position (1st / 2nd) from the gap sign — a racing-game staple.
    readonly property bool leading: race.gapMeters >= 0
    // Drafting: tucked in the ghost's slipstream (0.5–8 m back) — pays a small
    // score trickle, like the real-world free watts.
    readonly property bool drafting: race.started && !race.finished
        && race.gapMeters < -0.5 && race.gapMeters > -8
    // Final sprint: the last 15 s before the finish line.
    readonly property bool finalSprint: race.started && !race.finished
        && race.finishSecs > 0 && race.finishSecs <= 15

    // In-zone hold streak: seconds spent continuously inside the target band —
    // rewards precise, sustained effort (the core gamified-training loop).
    property real zoneSince: -1
    property real zoneHoldSec: 0
    // Arcade score: 10 pts/s while in the zone, scaled by a streak multiplier
    // (10 s → ×2, 30 s → ×3, 60 s → ×4); overtakes and km milestones pay a
    // bonus. Resets when the gun fires.
    property real score: 0
    property real bestStreakSec: 0
    property real lastScoreT: -1
    readonly property int zoneMult: zoneHoldSec >= 60 ? 4 : zoneHoldSec >= 30 ? 3 : zoneHoldSec >= 10 ? 2 : 1
    // Gap trend (m/s, positive = you are gaining ground), sampled coarsely so
    // the avatar bubble's »/« arrow is stable rather than flickering per tick.
    property real gapTrend: 0
    property real trendPrevGap: 0
    // Ghost reactions (drives the mirror sprite): 1 = salute — you just
    // dropped him (fires once per drop, re-arms when he claws back inside
    // 6 m); 2 = road-rash elbow + sway — he's closing in to pass.
    property int  ghostMood: 0
    property bool ghostDropped: false
    property real moodUntil: 0
    // The on-road ghost throws an arm up for a moment after passing YOU.
    property real oppCelebrateUntil: -1
    Timer {
        interval: 1500; repeat: true; running: race.started && !race.finished
        onTriggered: {
            root.gapTrend = (race.gapMeters - root.trendPrevGap) / 1.5
            root.trendPrevGap = race.gapMeters
            if (root.leading) {
                if (!root.ghostDropped && race.gapMeters > 15 && root.gapTrend > 0.2) {
                    root.ghostDropped = true; root.ghostMood = 1; root.moodUntil = root.animT + 3.5
                } else if (race.gapMeters < 6) {
                    root.ghostDropped = false
                }
                if (root.ghostMood === 1 && root.animT > root.moodUntil) root.ghostMood = 0
                if (root.ghostMood !== 1)
                    root.ghostMood = (race.gapMeters < 12 && root.gapTrend < -0.2) ? 2 : 0
            } else {
                root.ghostMood = 0
                root.ghostDropped = false
            }
        }
    }
    Connections {
        target: race
        function onUpdated() {
            if (race.started && !race.finished && race.targetPower > 0 && root.effortState === 0) {
                if (root.zoneSince < 0) root.zoneSince = root.animT
                root.zoneHoldSec = root.animT - root.zoneSince
            } else {
                root.zoneSince = -1
                root.zoneHoldSec = 0
            }
            if (root.zoneHoldSec > root.bestStreakSec) root.bestStreakSec = root.zoneHoldSec
            // dt clamped so a pause/resume cannot pay out one giant tick.
            var scoreDt = Math.min(0.5, root.animT - root.lastScoreT)
            if (root.lastScoreT >= 0 && race.started && !race.finished && race.running) {
                if (race.targetPower > 0 && root.effortState === 0)
                    root.score += scoreDt * 10 * root.zoneMult
                if (root.drafting)
                    root.score += scoreDt * 2
            }
            root.lastScoreT = root.animT
        }
        function onRaceStateChanged() {
            if (race.started && !race.finished) {
                root.score = 0; root.bestStreakSec = 0; root.lastScoreT = root.animT
                root.lastKm = 0; root.gapTrend = 0; root.trendPrevGap = race.gapMeters
                root.ghostMood = 0; root.ghostDropped = false; root.oppCelebrateUntil = -1
            }
        }
    }
    function fmtTime(s) { var m = Math.floor(s / 60); var ss = Math.floor(s % 60); return m + ":" + (ss < 10 ? "0" : "") + ss; }
    // Golden hour: 0 for the first 60% of the workout, then ramps to 1 at the
    // finish — the sky warms and the sun sinks, so a long ride visibly ends at
    // sunset (a quiet "almost there" cue).
    readonly property real dusk: Math.max(0, (race.workoutProgress - 0.6) / 0.4)
    function mixCol(a, b, t) {
        var c1 = Qt.color(a), c2 = Qt.color(b)
        return Qt.rgba(c1.r + (c2.r - c1.r) * t, c1.g + (c2.g - c1.g) * t, c1.b + (c2.b - c1.b) * t, 1)
    }
    // Ghost names can be whole history filenames — keep HUD chips readable.
    function shortName(n, max) { return n.length > max ? n.substring(0, max - 1) + "…" : n }
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
        property real  armUp: 0      // 0..1 — right arm raised (salute / celebration)
        property bool  elbow: false  // elbow thrown out + jabbing (road-rash chase)
        Behavior on armUp { NumberAnimation { duration: 450; easing.type: Easing.OutBack } }
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
        // left leg (dark tights): thigh + calf + shoe, flanking the wheel and
        // pumping together. No skin — from behind the legs read as dark tights.
        Rectangle { x: bike.bw*0.28;  y: bike.bh*0.52 + bike.amp; width: bike.bw*0.15; height: bike.bh*0.18; color: "#23232e" }
        Rectangle { x: bike.bw*0.295; y: bike.bh*0.66 + bike.amp; width: bike.bw*0.12; height: bike.bh*0.12; color: "#2b2b35" }
        Rectangle { x: bike.bw*0.28;  y: bike.bh*0.76 + bike.amp; width: bike.bw*0.15; height: bike.bh*0.05; color: "#101014" }
        // right leg (opposite phase)
        Rectangle { x: bike.bw*0.57;  y: bike.bh*0.52 - bike.amp; width: bike.bw*0.15; height: bike.bh*0.18; color: "#23232e" }
        Rectangle { x: bike.bw*0.585; y: bike.bh*0.66 - bike.amp; width: bike.bw*0.12; height: bike.bh*0.12; color: "#2b2b35" }
        Rectangle { x: bike.bw*0.57;  y: bike.bh*0.76 - bike.amp; width: bike.bw*0.15; height: bike.bh*0.05; color: "#101014" }
        // hips / saddle (dark shorts)
        Rectangle { x: bike.bw*0.26; y: bike.bh*0.46; width: bike.bw*0.48; height: bike.bh*0.12; color: "#1e1e28" }
        // jersey: lower back (widest) → mid back → shoulders (tapered, leaning in)
        Rectangle { x: bike.bw*0.23; y: bike.bh*0.34; width: bike.bw*0.54; height: bike.bh*0.16; color: bike.body }
        Rectangle { x: bike.bw*0.27; y: bike.bh*0.24; width: bike.bw*0.46; height: bike.bh*0.12; color: bike.body }
        Rectangle { x: bike.bw*0.30; y: bike.bh*0.17; width: bike.bw*0.40; height: bike.bh*0.10; color: Qt.darker(bike.body, 1.3) }
        // (No arms/hands drawn: from directly behind they sit forward on the
        // bars, hidden by the torso — drawing stubs read as hands poking out.)
        // raised right arm + fist (salute), rising from behind the shoulder
        Rectangle { visible: bike.armUp > 0.01; opacity: bike.armUp
                    x: bike.bw*0.64; y: bike.bh*(0.22 - 0.20*bike.armUp)
                    width: bike.bw*0.09; height: bike.bh*0.20; radius: bike.bw*0.04; color: bike.skin }
        Rectangle { visible: bike.armUp > 0.01; opacity: bike.armUp
                    x: bike.bw*0.615; y: bike.bh*(0.19 - 0.20*bike.armUp)
                    width: bike.bw*0.14; height: bike.bw*0.14; radius: bike.bw*0.07; color: bike.skin }
        // elbow jab out to the left (road-rash aggression), pumping
        Rectangle { visible: bike.elbow
                    x: -bike.bw*0.02 + Math.sin(root.animT*9)*bike.bw*0.05
                    y: bike.bh*0.30; width: bike.bw*0.34; height: bike.bh*0.06
                    radius: bike.bh*0.03; color: bike.skin }
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
            GradientStop { position: 0.0;  color: root.mixCol("#1f3566", "#2a2150", root.dusk) }
            GradientStop { position: 0.30; color: root.mixCol("#5f93cf", "#a6608c", root.dusk) }
            GradientStop { position: 0.62; color: root.mixCol("#a9d8ef", "#ef9f66", root.dusk) }
            GradientStop { position: 1.0;  color: root.mixCol("#cdeaf6", "#ffd9a0", root.dusk) }
        }
    }
    // warm low sun with a soft halo; sinks toward the ridge line at golden
    // hour (capped so it never disappears behind the mountain ranges)
    readonly property real sunY: 56 + dusk * 38
    Rectangle { x: root.width*0.77 - 22; y: root.sunY - 18; width: 104; height: 104; radius: 52
                color: root.mixCol("#ffd98a", "#ff9e5e", root.dusk); opacity: 0.30 }
    Rectangle { x: root.width*0.77;      y: root.sunY; width: 60;  height: 60;  radius: 30
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.mixCol("#fff1b4", "#ffd27a", root.dusk) }
            GradientStop { position: 1.0; color: root.mixCol("#ffb259", "#ff7a3c", root.dusk) }
        }
    }

    // Soft clouds drifting slowly across the sky (wrap cleanly via the animT clock).
    Repeater {
        model: 5
        Item {
            id: cloud
            readonly property real px:    [110, 360, 600, 840, 200][index] / 960 * root.width
            readonly property real py:    [60, 118, 48, 140, 92][index]
            readonly property real sc:    [1.0, 0.78, 1.12, 0.7, 0.9][index]
            readonly property real drift: [9, 6, 12, 5, 7.5][index]      // px/sec
            readonly property real cw:    100 * sc
            readonly property real lane:  root.width + cw + 60
            x: ((px + root.animT * drift) % lane + lane) % lane - cw
            y: py
            Rectangle { x: 0;           y: 10*cloud.sc; width: 72*cloud.sc; height: 26*cloud.sc; radius: 13*cloud.sc; color: "#ffffff"; opacity: 0.82 }
            Rectangle { x: 24*cloud.sc; y: 0;           width: 52*cloud.sc; height: 34*cloud.sc; radius: 17*cloud.sc; color: "#ffffff"; opacity: 0.9 }
            Rectangle { x: 52*cloud.sc; y: 8*cloud.sc;  width: 46*cloud.sc; height: 24*cloud.sc; radius: 12*cloud.sc; color: "#eef4ff"; opacity: 0.82 }
        }
    }

    // A few birds: drift across the sky and flap their wings (animT-driven).
    Repeater {
        model: 3
        Item {
            id: bird
            readonly property real py:    [118, 134, 98][index]
            readonly property real bs:    [1.0, 0.8, 1.15][index]
            readonly property real drift: [22, 17, 26][index]            // px/sec
            readonly property real phase: [0, 2.1, 4.0][index]
            readonly property real lane:  root.width + 80
            readonly property real flap:  16 + 12 * Math.sin(root.animT * 5 + phase)
            x: ((index*330 + root.animT * drift) % lane + lane) % lane - 40
            y: py + 5 * Math.sin(root.animT * 1.1 + phase)
            Rectangle { x: 0;           width: 13*bird.bs; height: 3*bird.bs; radius: 1.5; color: "#2a3c54"; antialiasing: true
                        rotation: bird.flap;  transformOrigin: Item.Right }
            Rectangle { x: 9.5*bird.bs; width: 13*bird.bs; height: 3*bird.bs; radius: 1.5; color: "#2a3c54"; antialiasing: true
                        rotation: -bird.flap; transformOrigin: Item.Left }
        }
    }

    // Mountain horizon — three jagged ranges (far snow-capped peaks → mid teal →
    // near green forested hills). Drawn once on a Canvas; their bases tuck behind
    // the green ground fill below.
    Canvas {
        id: mountains
        anchors.left: parent.left; anchors.right: parent.right
        y: 0; height: root.horizonY + 8
        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()
        onPaint: {
            var ctx = getContext("2d"); ctx.reset();
            var W = width, base = root.horizonY;
            function rj(n) { var v = Math.sin(n * 127.1) * 43758.5453; return v - Math.floor(v); }
            // One jagged range: filled body + a sunlit ridge highlight along the top.
            function range(amp, step, fill, hi, seed) {
                var pts = [];
                for (var x = -step, i = 0; x <= W + step; x += step, i++)
                    pts.push({ x: x, y: base - amp * (0.34 + 0.66 * rj(i * 1.7 + seed)) });
                ctx.fillStyle = fill; ctx.beginPath(); ctx.moveTo(pts[0].x, base);
                for (var k = 0; k < pts.length; k++) ctx.lineTo(pts[k].x, pts[k].y);
                ctx.lineTo(pts[pts.length - 1].x, base); ctx.closePath(); ctx.fill();
                ctx.strokeStyle = hi; ctx.lineWidth = 2.5; ctx.lineJoin = "round";
                ctx.beginPath(); ctx.moveTo(pts[0].x, pts[0].y);
                for (var j = 1; j < pts.length; j++) ctx.lineTo(pts[j].x, pts[j].y);
                ctx.stroke();
            }
            range(165, W / 11, "#566f9e", "#8ea6cc", 2.1);   // far blue mountains
            range(112, W / 8,  "#4d857e", "#79b3aa", 7.3);   // mid teal ridge
            range(64,  W / 12, "#3f7a52", "#5fa46c", 13.7);  // near green hills
            range(40,  W / 52, "#356b41", "#3f7a4c", 21.5);  // distant forest treeline (fine bumps)
        }
    }

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
            readonly property real cdW:  Math.max(1.5, hw * 0.032)            // centre-dash width (thin)
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
            // centre dash (alternate segments only) — runs from near the horizon
            Rectangle { visible: parent.even && parent.p > 0.05
                x: parent.cx - parent.cdW / 2; width: parent.cdW; height: parent.height; color: "#f3f1d6" }
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
    // depth/speed cue plus scenery. Six kinds (pine / leafy / bush / birch /
    // rock / flower patch) in varied greens (some leafy trees turn autumn),
    // scattered across the verge. Anchored in world distance.
    Repeater {
        model: 156
        Item {
            id: ob
            readonly property real span: 13 * 156
            readonly property real az: ((index * 13 - race.visualDist) % span + span) % span + 8  // distance ahead
            readonly property real p: Math.min(1.0, 38 / az)
            readonly property real r1: root.rnd(index * 1.3 + 3)
            // 0 pine · 1 leafy · 2 bush · 3 birch · 4 rock · 5 flower patch
            readonly property int  kind: r1 < 0.22 ? 2 : r1 < 0.40 ? 0 : r1 < 0.62 ? 1
                                       : r1 < 0.74 ? 3 : r1 < 0.86 ? 4 : 5
            readonly property int  side: root.rnd(index * 2.1) < 0.5 ? -1 : 1
            // Lateral world offset from centre: 1.1× road-half (just off the
            // edge) out to ~4.5× (far across the grass). Scattered, not hugging.
            readonly property real latWorld: root.maxHalfW * (1.10 + root.rnd(index + 5) * 3.4)
            readonly property real cy: root.horizonY + p * root.roadH
            readonly property real baseW: kind === 2 ? (88 + root.rnd(index+1)*54)
                                       : kind === 0 ? (74 + root.rnd(index+2)*40)
                                       : kind === 1 ? (86 + root.rnd(index+2)*54)
                                       : kind === 3 ? (58 + root.rnd(index+2)*30)
                                       : kind === 4 ? (66 + root.rnd(index+1)*58)
                                                    : (64 + root.rnd(index+1)*46)
            readonly property real baseH: kind === 2 ? (50 + root.rnd(index+7)*30)
                                       : kind === 0 ? (205 + root.rnd(index+4)*140)
                                       : kind === 1 ? (165 + root.rnd(index+4)*120)
                                       : kind === 3 ? (185 + root.rnd(index+4)*110)
                                       : kind === 4 ? (34 + root.rnd(index+7)*26)
                                                    : (16 + root.rnd(index+7)*10)
            readonly property real tw: baseW * p
            readonly property real th: baseH * p
            // a few leafy trees turn warm autumn colours for variety
            readonly property bool autumn: kind === 1 && root.rnd(index + 17) < 0.14
            readonly property color baseFoliage: autumn
                ? Qt.rgba(0.72 + root.rnd(index+9)*0.12, 0.40 + root.rnd(index+11)*0.16, 0.10 + root.rnd(index+13)*0.08, 1)
                : Qt.rgba(0.16 + root.rnd(index+9)*0.12, 0.44 + root.rnd(index+11)*0.18, 0.22 + root.rnd(index+13)*0.12, 1)
            // distance haze: 0 near .. 1 far, fades the object into the horizon.
            // Baked into the colours (Qt.tint) rather than a square overlay, which
            // showed a faint box around distant trees.
            readonly property real haz: Math.max(0, Math.min(1, (0.34 - p) / 0.20))
            readonly property color foliage: Qt.tint(baseFoliage, Qt.rgba(0.62, 0.76, 0.81, ob.haz * 0.55))
            readonly property color barkCol: Qt.tint("#5a3a1f", Qt.rgba(0.62, 0.76, 0.81, ob.haz * 0.55))
            readonly property color birchBark: Qt.tint("#e8e4da", Qt.rgba(0.62, 0.76, 0.81, ob.haz * 0.55))
            readonly property color rockCol: Qt.tint(Qt.rgba(0.48 + root.rnd(index+19)*0.14,
                                                             0.49 + root.rnd(index+19)*0.14,
                                                             0.53 + root.rnd(index+19)*0.14, 1),
                                                     Qt.rgba(0.62, 0.76, 0.81, ob.haz * 0.55))
            readonly property color petal: ["#ff7bb1", "#ffd84a", "#ff5a5a", "#f2f2f2"][Math.floor(root.rnd(index+23)*4)]
            visible: race.started && p > 0.14
            x: root.width/2 + side * p * latWorld - tw/2
            y: cy - th
            width: tw; height: th

            // ground shadow at the base
            Rectangle { x: ob.tw*0.06; y: ob.th*0.93; width: ob.tw*0.88; height: ob.th*0.06
                        radius: ob.th*0.03; color: "#000000"; opacity: 0.22 }
            // bush: low rounded clump (two blobs)
            Rectangle { visible: ob.kind===2; x: 0;          y: ob.th*0.22; width: ob.tw;      height: ob.th*0.78; radius: ob.tw*0.40; color: ob.foliage }
            Rectangle { visible: ob.kind===2; x: ob.tw*0.18; y: 0;          width: ob.tw*0.64; height: ob.th*0.50; radius: ob.tw*0.34; color: Qt.lighter(ob.foliage,1.16) }

            // trunk (pine + leafy)
            Rectangle { visible: ob.kind===0 || ob.kind===1; x: ob.tw*0.44; y: ob.th*0.66; width: ob.tw*0.12; height: ob.th*0.36; color: ob.barkCol }

            // pine: three tiers narrowing upward (conical)
            Rectangle { visible: ob.kind===0; x: ob.tw*0.10; y: ob.th*0.40; width: ob.tw*0.80; height: ob.th*0.32; radius: ob.tw*0.10; color: ob.foliage }
            Rectangle { visible: ob.kind===0; x: ob.tw*0.18; y: ob.th*0.20; width: ob.tw*0.64; height: ob.th*0.28; radius: ob.tw*0.10; color: Qt.lighter(ob.foliage,1.10) }
            Rectangle { visible: ob.kind===0; x: ob.tw*0.30; y: 0;          width: ob.tw*0.40; height: ob.th*0.26; radius: ob.tw*0.10; color: Qt.lighter(ob.foliage,1.20) }

            // leafy tree: full round canopy (base blob + two lighter highlights)
            Rectangle { visible: ob.kind===1; x: 0;          y: ob.th*0.24; width: ob.tw;      height: ob.th*0.50; radius: ob.tw*0.42; color: ob.foliage }
            Rectangle { visible: ob.kind===1; x: ob.tw*0.10; y: ob.th*0.06; width: ob.tw*0.62; height: ob.th*0.44; radius: ob.tw*0.34; color: Qt.lighter(ob.foliage,1.12) }
            Rectangle { visible: ob.kind===1; x: ob.tw*0.40; y: 0;          width: ob.tw*0.46; height: ob.th*0.34; radius: ob.tw*0.26; color: Qt.lighter(ob.foliage,1.22) }

            // birch: slim pale trunk with dark bark dashes, airy light canopy
            Rectangle { visible: ob.kind===3; x: ob.tw*0.45; y: ob.th*0.24; width: ob.tw*0.10; height: ob.th*0.78; color: ob.birchBark }
            Rectangle { visible: ob.kind===3; x: ob.tw*0.45; y: ob.th*0.46; width: ob.tw*0.06; height: ob.th*0.030; color: "#3c3a34" }
            Rectangle { visible: ob.kind===3; x: ob.tw*0.49; y: ob.th*0.62; width: ob.tw*0.06; height: ob.th*0.030; color: "#3c3a34" }
            Rectangle { visible: ob.kind===3; x: ob.tw*0.45; y: ob.th*0.80; width: ob.tw*0.06; height: ob.th*0.030; color: "#3c3a34" }
            Rectangle { visible: ob.kind===3; x: ob.tw*0.08; y: ob.th*0.10; width: ob.tw*0.84; height: ob.th*0.26; radius: ob.tw*0.30; color: Qt.lighter(ob.foliage,1.28) }
            Rectangle { visible: ob.kind===3; x: 0;          y: ob.th*0.02; width: ob.tw*0.60; height: ob.th*0.20; radius: ob.tw*0.24; color: Qt.lighter(ob.foliage,1.42) }
            Rectangle { visible: ob.kind===3; x: ob.tw*0.42; y: 0;          width: ob.tw*0.55; height: ob.th*0.18; radius: ob.tw*0.22; color: Qt.lighter(ob.foliage,1.18) }

            // rock: two grey boulders with a sunlit top facet
            Rectangle { visible: ob.kind===4; x: 0;          y: ob.th*0.25; width: ob.tw*0.72; height: ob.th*0.75; radius: ob.th*0.30; color: ob.rockCol }
            Rectangle { visible: ob.kind===4; x: ob.tw*0.40; y: 0;          width: ob.tw*0.60; height: ob.th*0.88; radius: ob.th*0.34; color: Qt.lighter(ob.rockCol,1.12) }
            Rectangle { visible: ob.kind===4; x: ob.tw*0.50; y: ob.th*0.10; width: ob.tw*0.28; height: ob.th*0.24; radius: ob.th*0.12; color: Qt.lighter(ob.rockCol,1.30) }

            // flower patch: low green pad with bright blossoms poking out
            Rectangle { visible: ob.kind===5; x: 0;          y: ob.th*0.35; width: ob.tw;      height: ob.th*0.65; radius: ob.th*0.32; color: ob.foliage }
            Rectangle { visible: ob.kind===5; x: ob.tw*0.10; y: ob.th*0.18; width: ob.tw*0.14; height: ob.tw*0.14; radius: ob.tw*0.07; color: ob.petal }
            Rectangle { visible: ob.kind===5; x: ob.tw*0.36; y: ob.th*0.02; width: ob.tw*0.14; height: ob.tw*0.14; radius: ob.tw*0.07; color: Qt.lighter(ob.petal,1.15) }
            Rectangle { visible: ob.kind===5; x: ob.tw*0.62; y: ob.th*0.22; width: ob.tw*0.14; height: ob.tw*0.14; radius: ob.tw*0.07; color: ob.petal }
            Rectangle { visible: ob.kind===5; x: ob.tw*0.80; y: ob.th*0.06; width: ob.tw*0.12; height: ob.tw*0.12; radius: ob.tw*0.06; color: Qt.lighter(ob.petal,1.25) }
        }
    }

    // Roadside warning: a big sign (flanked by bushes) on the right verge that
    // rolls in from the horizon and sweeps past the rider exactly as the next
    // interval begins. Anchored in world distance (nextSignZ) so it scrolls
    // smoothly with the scenery; it becomes visible far out for a calm approach.
    Item {
        id: ivSign
        readonly property real aheadZ: race.nextSignZ - race.visualDist   // smooth (visualDist)
        readonly property real p:      Math.min(1.0, 34 / Math.max(6, aheadZ))
        readonly property real halfw:  root.maxHalfW * p
        readonly property real cy:     root.horizonY + p * root.roadH
        readonly property real sw:     root.width * 0.12 * p              // panel width (scales with view)
        readonly property real sh:     sw * 0.66
        readonly property real postH:  sw * 1.05
        visible: race.started && !race.finished && race.nextTargetW > 0
                 && race.nextSecs >= 0 && aheadZ > 1.5 && aheadZ < 420
        // Follow the road's right edge, but clamp to the screen so the sign
        // stays fully visible (hugging the right margin and growing) right up to
        // the moment the interval starts — instead of sliding off-screen early
        // because the road is wider than the view near the rider.
        readonly property real rawX: root.width/2 + halfw + sw * 0.5
        x: Math.min(rawX, root.width - sw * 0.55)
        y: cy
        // post
        Rectangle { x: -ivSign.sw*0.05; y: -ivSign.postH; width: ivSign.sw*0.10; height: ivSign.postH; color: "#8c8c8c" }
        // panel
        Rectangle { x: -ivSign.sw/2; y: -ivSign.postH - ivSign.sh; width: ivSign.sw; height: ivSign.sh
                    radius: ivSign.sw*0.06; color: "#16324a"; border.color: "#ffce3a"; border.width: Math.max(1, ivSign.sw*0.028) }
        Text { x: -ivSign.sw/2; y: -ivSign.postH - ivSign.sh; width: ivSign.sw; height: ivSign.sh*0.62
               horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
               text: Math.round(race.nextTargetW) + "W"; color: "white"; font.family: "monospace"
               font.pixelSize: Math.max(8, ivSign.sw*0.34); font.bold: true }
        Text { x: -ivSign.sw/2; y: -ivSign.postH - ivSign.sh*0.40; width: ivSign.sw; height: ivSign.sh*0.40
               horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
               text: "NEXT"; color: "#ffce3a"; font.family: "monospace"
               font.pixelSize: Math.max(6, ivSign.sw*0.16); font.bold: true }
        // bushes at the foot of the post
        Rectangle { x: ivSign.sw*0.10;  y: -ivSign.sh*0.30; width: ivSign.sw*0.38; height: ivSign.sh*0.34; radius: ivSign.sw*0.18; color: "#357d40" }
        Rectangle { x: -ivSign.sw*0.46; y: -ivSign.sh*0.24; width: ivSign.sw*0.32; height: ivSign.sh*0.28; radius: ivSign.sw*0.15; color: "#2f7a3a" }
    }

    // Finish line: a checkered band spanning the road with a "FINISH" banner.
    // It is anchored at a fixed WORLD distance (finWorldZ) — exactly like the
    // roadside trees — so it scrolls at the same speed as the road/rumble border.
    // Re-predicted each second from (visualDist + secs × visualSpeed) and
    // BLENDED toward that prediction from both sides, with a weight that grows
    // as the countdown runs out (exact over the last couple of seconds). The
    // old closer-only ratchet banked every momentary speed-up, so the line
    // arrived seconds early; gentle two-way corrections stay unnoticeable
    // against the rushing road.
    property real finWorldZ: -1
    Connections {
        target: race
        function onFinishChanged() {
            if (race.started && !race.finished && race.finishSecs >= 0
                && race.finishSecs <= 30 && race.visualSpeed > 0.3) {
                var pred = race.visualDist + race.finishSecs * race.visualSpeed
                var alpha = 2 / Math.max(2, race.finishSecs)
                root.finWorldZ = root.finWorldZ < 0 ? pred
                    : root.finWorldZ + (pred - root.finWorldZ) * alpha
            } else if (race.finishSecs < 0 || race.finishSecs > 31) {
                root.finWorldZ = -1
            }
        }
    }
    Item {
        id: finishLine
        readonly property real aheadZ: root.finWorldZ < 0 ? -1 : (root.finWorldZ - race.visualDist)
        readonly property real p:      Math.min(1.0, 38 / Math.max(6, aheadZ))
        readonly property real halfw:  root.maxHalfW * p
        readonly property real roadW:  2 * halfw
        readonly property real cy:     root.horizonY + p * root.roadH
        readonly property real bandH:  Math.max(4, 28 * p)
        readonly property real postH:  74 * p
        readonly property int  cols:   12
        visible: race.started && !race.finished && root.finWorldZ >= 0 && aheadZ > 0.5 && aheadZ < 480
        x: root.width/2 - halfw
        y: cy - bandH
        width: roadW; height: bandH
        // checkered band (two rows of alternating squares)
        Repeater {
            model: finishLine.cols * 2
            Rectangle {
                readonly property int col:  index % finishLine.cols
                readonly property int rowi: Math.floor(index / finishLine.cols)
                x: col * (finishLine.roadW / finishLine.cols)
                y: rowi * (finishLine.bandH / 2)
                width:  finishLine.roadW / finishLine.cols + 0.6
                height: finishLine.bandH / 2 + 0.6
                color: ((col + rowi) % 2 === 0) ? "#f4f4f4" : "#161616"
            }
        }
        // posts
        Rectangle { x: -6*finishLine.p;        y: -finishLine.postH; width: 6*finishLine.p; height: finishLine.postH; color: "#cfcfcf" }
        Rectangle { x: finishLine.roadW;       y: -finishLine.postH; width: 6*finishLine.p; height: finishLine.postH; color: "#cfcfcf" }
        // overhead FINISH banner
        Rectangle { x: -6*finishLine.p; y: -finishLine.postH - 26*finishLine.p
            width: finishLine.roadW + 12*finishLine.p; height: 26*finishLine.p
            color: "#16324a"; border.color: "#ffce3a"; border.width: Math.max(1, 2*finishLine.p)
            Text { anchors.centerIn: parent; text: "FINISH"; color: "white"
                   font.family: "monospace"; font.bold: true; font.pixelSize: Math.max(7, 17*finishLine.p) } }
    }

    // ---------------------------------------------------------------- riders
    // Opponent: shown up the road only while ahead of you (you are behind).
    BackBike {
        id: oppBike
        readonly property real ahead: -race.gapMeters
        readonly property real f: 55 / (55 + Math.max(0, ahead))     // 1 near .. ->0 far
        // Keep the ghost riding right beside you (near the player's level and
        // size) across the whole racing range — the gap is conveyed by the HUD.
        readonly property real op: 0.60 + 0.16 * f                   // 0.60 .. 0.76 (always close)
        readonly property real oz: (1.0 / Math.max(0.03, op)) * 38 + race.visualDist
        visible: race.started && !race.finished && ahead > 0.5 && ahead < 250
        s: Math.min(2.15, 1.45 + 0.7 * f)                            // near the player's size
        body: "#7fd0ff"; helmet: "#15323f"; alpha: 0.85   // cyan so it stands out on grey + the dash
        phase: race.oppCrankRev
        lean: root.curveAt(oz) * 13
        armUp: root.animT < root.oppCelebrateUntil ? 1 : 0   // brief flex after passing you
        // Sit in the LEFT lane (offset grows as it nears) so you pull up
        // alongside it rather than staring at its back wheel. Converges toward
        // the centre/vanishing point when it is far up the road.
        // Sit just to the LEFT of the player, almost touching — offset by the
        // riders' own pixel widths (NOT maxHalfW, which scales with the window
        // and otherwise flings the ghost far away on a wide screen). The player
        // is centred at width/2 with half-width ≈ 62 px (s = 2.15).
        x: root.width / 2 - width - 64
        y: root.horizonY + root.roadH * (0.60 + 0.16 * f) - height
    }
    // Player: fixed near the bottom centre, large.
    BackBike {
        id: playerBike
        s: 2.15
        body: "#e23b3b"
        phase: race.playerCrankRev
        lean: root.leanDeg
        x: root.width/2 - width/2
        // Sit above the profile strip so the graph never hides the rider.
        y: root.height - height - (profileStrip.visible ? 78 : 12)
    }
    // Gap bubble pinned above the player — the race state lives where the eyes
    // already are (the avatar), not in a corner HUD: who you are racing, the
    // gap, and a »/« trend arrow (gaining/losing). Pulses when an overtake is
    // within reach.
    Rectangle {
        id: gapChip
        readonly property real absGap: Math.abs(race.gapMeters)
        readonly property bool hot: !root.leading && absGap < 15
        // A 1-2 m lead is noise, not news — drop the "behind" bubble until the
        // ghost actually loses contact (it still shows in the mirror).
        visible: race.started && !race.finished && (!root.leading || absGap >= 3)
        z: 40
        anchors.horizontalCenter: parent.horizontalCenter
        y: playerBike.y - height - 12
        width: gapTxt.implicitWidth + 26; height: 30; radius: 15
        color: "#000000"; opacity: 0.82
        border.width: 2
        border.color: root.leading ? "#5dff5d" : "#ff6b6b"
        scale: hot ? 1.0 + 0.05 * Math.sin(root.animT * 8) : 1.0
        Text {
            id: gapTxt; anchors.centerIn: parent
            text: (root.leading ? "▼ " : "▲ ") + root.shortName(race.oppName, 16) + " "
                  + gapChip.absGap.toFixed(0) + " m " + (root.leading ? "behind" : "ahead")
                  + (root.gapTrend > 0.15 ? "  »" : root.gapTrend < -0.15 ? "  «" : "")
            color: gapChip.border.color
            font.family: "monospace"; font.pixelSize: 14; font.bold: true
        }
    }
    // Slipstream pill under the gap bubble while tucked behind the ghost.
    Rectangle {
        visible: root.drafting
        z: 40
        anchors.horizontalCenter: parent.horizontalCenter
        y: gapChip.y + gapChip.height + 6
        width: draftTxt.implicitWidth + 20; height: 22; radius: 11
        color: "#0e2733"; border.color: "#7fd0ff"; border.width: 1
        opacity: 0.75 + 0.20 * Math.sin(root.animT * 5)
        Text { id: draftTxt; anchors.centerIn: parent; text: "≋ DRAFTING +2/s"
               color: "#7fd0ff"; font.family: "monospace"; font.pixelSize: 12; font.bold: true }
    }
    // Rear-view mirror — once you take the lead the ghost no longer renders on
    // the road ahead, so show it chasing you here instead; it grows in the
    // glass as it closes back in.
    Item {
        id: mirror
        readonly property real behindM: race.gapMeters
        readonly property real f: 30 / (30 + Math.max(0, behindM))   // 1 close .. →0 far
        visible: race.started && !race.finished && behindM > 0.5
        z: 40   // above the effort glow, like the HUD
        width: 148; height: 88
        // Ride just off the player's right shoulder, like a bar-end mirror —
        // close to where the eyes already are during the race.
        x: playerBike.x + playerBike.width + 70
        y: playerBike.y + playerBike.height/2 - height/2
        Rectangle { anchors.fill: parent; radius: 10
                    color: "#10161f"; border.color: "#cfd8e2"; border.width: 3 }
        Item {
            id: glass
            x: 5; y: 5; width: parent.width - 10; height: parent.height - 10
            clip: true
            readonly property real mHorizon: height * 0.38
            Rectangle { width: glass.width; height: glass.mHorizon
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#5f93cf" }
                    GradientStop { position: 1.0; color: "#a9d8ef" } } }
            Rectangle { y: glass.mHorizon; width: glass.width; height: glass.height - glass.mHorizon; color: "#3c7548" }
            Shape {
                anchors.fill: parent
                ShapePath {
                    strokeWidth: -1; fillColor: "#5b5b66"
                    startX: glass.width * -0.22; startY: glass.height
                    PathLine { x: glass.width * 0.44; y: glass.mHorizon }
                    PathLine { x: glass.width * 0.56; y: glass.mHorizon }
                    PathLine { x: glass.width * 1.22; y: glass.height }
                }
            }
            BackBike {
                s: 0.30 + 0.78 * mirror.f
                body: "#7fd0ff"; helmet: "#15323f"; alpha: 0.95
                phase: race.oppCrankRev
                armUp: root.ghostMood === 1 ? 1 : 0
                elbow: root.ghostMood === 2
                // weaves side to side hunting for a way past
                x: glass.width/2 - width/2
                   + (root.ghostMood === 2 ? Math.sin(root.animT*5.5) * glass.width * 0.07 : 0)
                y: glass.mHorizon + mirror.f * (glass.height - glass.mHorizon) - height
            }
            Text { x: 4; y: 2; text: "REAR"; color: "#ffffff"; opacity: 0.55
                   font.family: "monospace"; font.pixelSize: 8; font.bold: true }
        }
    }

    // Effort glow: a screen-edge wash that turns red when you're over the
    // target power and blue when under (nothing when in the zone) — a constant,
    // game-like nudge to hold the prescribed effort. Drawn under the HUD.
    Item {
        anchors.fill: parent; visible: root.effortGlow > 0.01; opacity: root.effortGlow
        readonly property real sideW: parent.width * (0.15 + 0.07 * root.effortMiss)
        readonly property real botH:  parent.height * (0.34 + 0.16 * root.effortMiss)
        Rectangle { anchors { left: parent.left; top: parent.top; bottom: parent.bottom } width: parent.sideW
            gradient: Gradient { orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: root.effortColor }
                GradientStop { position: 1.0; color: "#00000000" } } }
        Rectangle { anchors { right: parent.right; top: parent.top; bottom: parent.bottom } width: parent.sideW
            gradient: Gradient { orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#00000000" }
                GradientStop { position: 1.0; color: root.effortColor } } }
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: parent.botH
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00000000" }
                GradientStop { position: 1.0; color: root.effortColor } } }
        // a matching wash along the top edge for a fuller "alert" frame
        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: parent.height * 0.12
            gradient: Gradient {
                GradientStop { position: 0.0; color: root.effortColor }
                GradientStop { position: 1.0; color: "#00000000" } } }
    }

    // Final-sprint hype: a one-shot "FINAL SPRINT!" flash when the last 15 s
    // begin, plus a pulsing gold edge glow for the whole window.
    Item {
        anchors.fill: parent; visible: root.finalSprint
        opacity: 0.30 + 0.12 * Math.sin(root.animT * 6)
        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: parent.height * 0.10
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#ffce3a" }
                GradientStop { position: 1.0; color: "#00000000" } } }
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: parent.height * 0.10
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00000000" }
                GradientStop { position: 1.0; color: "#ffce3a" } } }
    }
    property bool sprintFlashed: false
    Text {
        id: sprintText; anchors.centerIn: parent; anchors.verticalCenterOffset: -root.height * 0.07; z: 58
        text: "FINAL SPRINT!"; color: "#ffce3a"; font.family: "monospace"
        font.pixelSize: Math.round(root.height * 0.09); font.bold: true; opacity: 0
        Connections { target: race
            function onFinishChanged() {
                if (root.finalSprint && !root.sprintFlashed) { root.sprintFlashed = true; sprintAnim.restart() }
                else if (race.finishSecs > 16 || race.finishSecs < 0) root.sprintFlashed = false
            }
        }
        SequentialAnimation { id: sprintAnim
            NumberAnimation { target: sprintText; property: "opacity"; from: 1.0; to: 1.0; duration: 900 }
            NumberAnimation { target: sprintText; property: "opacity"; to: 0.0; duration: 700 }
        }
    }

    // "GO!" splash when the gun fires (race transitions to started).
    Text {
        id: goText; anchors.centerIn: parent; z: 60
        text: "GO!"; color: "#ffe24a"; font.family: "monospace"
        font.pixelSize: Math.round(root.height * 0.22); font.bold: true
        opacity: 0; scale: 1
        Connections { target: race
            function onRaceStateChanged() { if (race.started && !race.finished) goAnim.restart() } }
        ParallelAnimation { id: goAnim
            NumberAnimation { target: goText; property: "opacity"; from: 1.0; to: 0.0; duration: 1100 }
            NumberAnimation { target: goText; property: "scale";   from: 0.6; to: 2.4; duration: 1100; easing.type: Easing.OutQuad }
        }
    }

    // OVERTAKE! / PASSED! flash when the lead changes hands.
    property real prevGap: 0
    Text {
        id: otText; anchors.centerIn: parent; anchors.verticalCenterOffset: -root.height * 0.18; z: 58
        text: ""; font.family: "monospace"; font.pixelSize: Math.round(root.height * 0.095); font.bold: true
        opacity: 0
        Connections { target: race
            function onUpdated() {
                if (!race.started || race.finished) return
                if (root.prevGap < 0 && race.gapMeters >= 0) { root.score += 250; otText.text = "OVERTAKE! +250"; otText.color = "#5dff5d"; otAnim.restart() }
                else if (root.prevGap >= 0 && race.gapMeters < 0) { otText.text = "PASSED!"; otText.color = "#ff6b6b"; otAnim.restart(); root.oppCelebrateUntil = root.animT + 2.5 }
                root.prevGap = race.gapMeters
            }
        }
        SequentialAnimation { id: otAnim
            NumberAnimation { target: otText; property: "opacity"; from: 1.0; to: 1.0; duration: 800 }
            NumberAnimation { target: otText; property: "opacity"; to: 0.0; duration: 650 }
        }
    }

    // Distance-milestone flash: a "X KM" pop each whole kilometre (progress reward).
    property int lastKm: 0
    Text {
        id: kmText; anchors.horizontalCenter: parent.horizontalCenter; y: root.height * 0.30; z: 58
        text: ""; color: "#7ee0ff"; font.family: "monospace"; font.pixelSize: Math.round(root.height * 0.085); font.bold: true
        opacity: 0
        Connections { target: race
            function onUpdated() {
                if (!race.started || race.finished) return
                var km = Math.floor(race.playerDistanceM / 1000)
                if (km > root.lastKm) { root.lastKm = km; root.score += 100; kmText.text = km + " KM  +100"; kmAnim.restart() }
            }
        }
        SequentialAnimation { id: kmAnim
            NumberAnimation { target: kmText; property: "opacity"; from: 1.0; to: 1.0; duration: 800 }
            NumberAnimation { target: kmText; property: "opacity"; to: 0.0; duration: 800 }
        }
    }

    // Effort-zone coaching pill (top-left): a clear, game-like cue to push,
    // ease off, or hold — colour-matched to the effort glow.
    Rectangle {
        visible: race.started && !race.finished && race.targetPower > 0
        anchors { left: parent.left; leftMargin: 12; top: parent.top; topMargin: 78 }
        width: zoneTxt.implicitWidth + 22; height: 26; radius: 13
        readonly property bool hot: root.effortState === 0 && root.zoneHoldSec >= 10
        color: root.effortState === 1 ? "#b02525" : root.effortState === -1 ? "#1f4f9e"
             : (hot ? "#1f8f3f" : "#1f7a37")
        border.color: hot ? "#ffe24a" : "#ffffff"; border.width: hot ? 2 : 1
        Text { id: zoneTxt; anchors.centerIn: parent
            text: root.effortState === 1 ? "▼ EASE OFF"
                : root.effortState === -1 ? "▲ PUSH!"
                : "✓ IN ZONE " + Math.floor(root.zoneHoldSec) + "s"
                  + (root.zoneMult > 1 ? "  ×" + root.zoneMult : "") + (parent.hot ? " 🔥" : "")
            color: "white"; font.family: "monospace"; font.pixelSize: 13; font.bold: true }
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
            property color vcolor: "white"
            Text { text: label; color: "#bcd"; font.family: "monospace"; font.pixelSize: 11; font.bold: true }
            Text { text: value; color: vcolor; font.family: "monospace"; font.pixelSize: 18; font.bold: true }
        }
        Stat { label: "TIME";  value: root.fmtTime(race.workoutElapsedSec) }
        Stat { label: "DIST";  value: (race.playerDistanceM / 1000).toFixed(2) + " km" }
        Stat { label: "SPEED"; value: race.playerSpeedKmh.toFixed(1) + " km/h" }
        Stat { label: "SCORE"; value: Math.floor(root.score).toString(); vcolor: "#ffe24a" }
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
        anchors.top: parent.top; anchors.topMargin: 6
        spacing: 3
        // Race-position medallion (1st / 2nd) — racing-game staple.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 56; height: 26; radius: 6
            color: root.leading ? "#1f7a37" : "#7a1f2a"; border.color: "#ffffff"; border.width: 1
            Text { anchors.centerIn: parent; text: root.leading ? "P1" : "P2"
                   color: "white"; font.family: "monospace"; font.pixelSize: 17; font.bold: true }
        }
        // (The gap headline lives in the avatar bubble now; only the battle bar
        // stays up here as a compact ±60 m trend view.)
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
        text: (race.oppIsBot ? "🤖 " : "👻 ") + root.shortName(race.oppName, 34) + "   ·   " + race.oppPowerW.toFixed(0) + " W"
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

    Repeater {   // confetti — a reward, so it only rains when you actually won
        model: 28
        Rectangle {
            readonly property bool celebrate: race.finished && race.gapMeters >= 0
            visible: celebrate
            width: 8; height: 8
            color: ["#e23b3b", "#5dff5d", "#3aa0ff", "#ffe08a", "#ff6bd6"][index % 5]
            x: Math.random() * root.width; y: -20
            NumberAnimation on y { running: celebrate; loops: Animation.Infinite
                from: -20; to: root.height + 20; duration: 1700 + (index % 7) * 280 }
            NumberAnimation on rotation { running: celebrate; loops: Animation.Infinite
                from: 0; to: 360; duration: 700 + (index % 5) * 220 }
        }
    }
    Rectangle { anchors.fill: parent; color: "#000000"; opacity: 0.4; visible: race.finished }
    Column {
        anchors.centerIn: parent; spacing: 10; visible: race.finished
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: race.gapMeters >= 0 ? "🏆  YOU WIN!" : "🏁  FINISH!"
               color: "#ffe08a"; font.family: "monospace"; font.pixelSize: 46; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: race.gapMeters >= 0 ? "You won by " + race.gapMeters.toFixed(0) + " m! 🎉"
                                         : "Beaten by " + (-race.gapMeters).toFixed(0) + " m"
               color: race.gapMeters >= 0 ? "#5dff5d" : "#ff6b6b"
               font.family: "monospace"; font.pixelSize: 22; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: (race.playerDistanceM / 1000).toFixed(2) + " km   ·   vs " + root.shortName(race.oppName, 24)
               color: "#cde"; font.family: "monospace"; font.pixelSize: 14 }
        Text { anchors.horizontalCenter: parent.horizontalCenter
               text: "SCORE " + Math.floor(root.score)
                     + "   ·   BEST ZONE STREAK " + root.fmtTime(root.bestStreakSec)
               color: "#ffe24a"; font.family: "monospace"; font.pixelSize: 16; font.bold: true }
    }
}
