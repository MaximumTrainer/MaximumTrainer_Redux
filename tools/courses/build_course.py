#!/usr/bin/env python3
"""Build a race course file from a list of waypoints.

Road geometry comes from OpenStreetMap routing (the public OSRM servers: car
routing for mountain roads, bike routing where car one-way rules would send
the course round extra blocks),
elevation from the EU-DEM 25 m model (OpenTopoData). The track is resampled
every STEP_M metres. Through a hairpin the DEM samples the hillside beside the
road, so the raw profile alternates flat bits and fake 15-20 % steps; a median
filter removes single spikes, then a 400 m moving average spreads each step
back over the road it belongs to. Output: resources/courses/<id>.json

Usage:
    python3 tools/courses/build_course.py            # build every course
    python3 tools/courses/build_course.py alpe-dhuez # build one

Map data (c) OpenStreetMap contributors, ODbL. Elevation: EU-DEM v1.1
(Copernicus Land Monitoring Service) via OpenTopoData.
"""
import json
import math
import sys
import time
import urllib.request
from pathlib import Path

STEP_M = 20.0
MEDIAN_WINDOW_M = 100.0
SMOOTH_WINDOW_M = 400.0
MAX_ABS_GRADE = 0.18
USER_AGENT = "MaximumTrainer-course-builder"
OUT_DIR = Path(__file__).resolve().parents[2] / "resources" / "courses"

ROUTERS = {
    "car": "https://router.project-osrm.org/route/v1/driving",
    "bike": "https://routing.openstreetmap.de/routed-bike/route/v1/driving",
}

COURSES = {
    "longchamp": {
        "name": "Longchamp loop",
        "place": "Bois de Boulogne, Paris, France",
        "laps": 6,
        "loop": True,
        "router": "bike",
        # Carrefour de Longchamp -> Route des Tribunes -> Anneau de Longchamp
        # -> Allee de la Reine Marguerite -> Carrefour de Longchamp.
        "waypoints": [
            (48.86360, 2.23960), (48.86220, 2.23060), (48.85910, 2.22920),
            (48.85170, 2.23650), (48.85110, 2.24180), (48.85630, 2.24340),
            (48.86360, 2.23960),
        ],
    },
    "alpe-dhuez": {
        "name": "Alpe d'Huez",
        "place": "Bourg-d'Oisans → Huez, France",
        "laps": 1,
        "loop": False,
        "router": "car",
        "waypoints": [(45.05570, 6.03130), (45.07280, 6.04760), (45.09130, 6.06950)],
    },
    "mont-ventoux": {
        "name": "Mont Ventoux",
        "place": "Bédoin → summit, France",
        "laps": 1,
        "loop": False,
        "router": "car",
        "waypoints": [(44.12380, 5.17970), (44.17410, 5.27880)],
    },
}


def http_json(url):
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def route_geometry(waypoints, router):
    coords = ";".join(f"{lon:.6f},{lat:.6f}" for lat, lon in waypoints)
    url = (f"{ROUTERS[router]}/{coords}"
           "?overview=full&geometries=geojson&continue_straight=true")
    data = http_json(url)
    if data.get("code") != "Ok":
        raise RuntimeError(f"routing failed: {data.get('code')} {data.get('message')}")
    return [(lat, lon) for lon, lat in data["routes"][0]["geometry"]["coordinates"]]


def haversine_m(a, b):
    lat1, lon1 = map(math.radians, a)
    lat2, lon2 = map(math.radians, b)
    h = (math.sin((lat2 - lat1) / 2) ** 2
         + math.cos(lat1) * math.cos(lat2) * math.sin((lon2 - lon1) / 2) ** 2)
    return 2 * 6371000.0 * math.asin(math.sqrt(h))


def prune_detours(points, close_m=15.0, max_share=0.25):
    """Cut out-and-back spurs and small side loops the router adds when a
    waypoint sits just off the road: any stretch that returns to within
    close_m of where it left, and is shorter than max_share of the route."""
    cumulative = [0.0]
    for previous, current in zip(points, points[1:]):
        cumulative.append(cumulative[-1] + haversine_m(previous, current))
    limit = max_share * cumulative[-1]
    kept, index = [], 0
    while index < len(points):
        kept.append(points[index])
        rejoin = None
        for later in range(len(points) - 1, index + 1, -1):
            detour = cumulative[later] - cumulative[index]
            if 3 * close_m < detour <= limit and haversine_m(points[index], points[later]) < close_m:
                rejoin = later
                break
        index = rejoin if rejoin is not None else index + 1
    return kept


def resample(points, step_m):
    cumulative = [0.0]
    for previous, current in zip(points, points[1:]):
        cumulative.append(cumulative[-1] + haversine_m(previous, current))
    total = cumulative[-1]
    samples, segment = [], 0
    distance = 0.0
    while distance <= total:
        while segment < len(points) - 2 and cumulative[segment + 1] < distance:
            segment += 1
        span = cumulative[segment + 1] - cumulative[segment]
        t = 0.0 if span <= 0 else (distance - cumulative[segment]) / span
        lat = points[segment][0] + t * (points[segment + 1][0] - points[segment][0])
        lon = points[segment][1] + t * (points[segment + 1][1] - points[segment][1])
        samples.append((distance, lat, lon))
        distance += step_m
    if samples[-1][0] < total:
        samples.append((total, points[-1][0], points[-1][1]))
    return samples


DEMS = {
    "eudem25m": "EU-DEM v1.1 (Copernicus)",
    "srtm30m": "SRTM 30 m (NASA)",
}


def elevations(samples, dem):
    result = []
    for start in range(0, len(samples), 100):
        batch = samples[start:start + 100]
        locations = "|".join(f"{lat:.6f},{lon:.6f}" for _, lat, lon in batch)
        data = http_json(f"https://api.opentopodata.org/v1/{dem}?locations={locations}")
        if data.get("status") != "OK":
            raise RuntimeError(f"elevation lookup failed: {data}")
        result.extend(r["elevation"] if r["elevation"] is not None else float("nan")
                      for r in data["results"])
        time.sleep(1.1)  # the public API allows one request per second
    return result


def window_values(values, index, half, wrap):
    if wrap:
        count = len(values) - 1  # a loop's last sample repeats the first
        return [values[(index + offset) % count] for offset in range(-half, half + 1)]
    return values[max(0, index - half): index + half + 1]


def median_filter(values, window_points, wrap):
    half = max(1, window_points // 2)
    return [sorted(w)[len(w) // 2] for w in
            (window_values(values, i, half, wrap) for i in range(len(values)))]


def smooth(values, window_points, wrap):
    half = max(1, window_points // 2)
    return [sum(w) / len(w) for w in
            (window_values(values, i, half, wrap) for i in range(len(values)))]


def limit_grades(distances, elevations_m):
    limited = [elevations_m[0]]
    for index in range(1, len(elevations_m)):
        run = distances[index] - distances[index - 1]
        rise = elevations_m[index] - limited[-1]
        cap = MAX_ABS_GRADE * run
        limited.append(limited[-1] + max(-cap, min(cap, rise)))
    return limited


def build(course_id, spec):
    print(f"[{course_id}] routing {len(spec['waypoints'])} waypoints")
    samples = resample(prune_detours(route_geometry(spec["waypoints"], spec["router"])), STEP_M)
    print(f"[{course_id}] {samples[-1][0] / 1000:.2f} km, {len(samples)} samples; fetching elevation")
    dem = spec.get("dem", "eudem25m")
    raw = elevations(samples, dem)
    known = [e for e in raw if not math.isnan(e)]
    raw = [e if not math.isnan(e) else known[0] for e in raw]
    distances = [s[0] for s in samples]
    wrap = spec["loop"]
    despiked = median_filter(raw, int(MEDIAN_WINDOW_M / STEP_M), wrap)
    smoothed = limit_grades(distances, smooth(despiked, int(SMOOTH_WINDOW_M / STEP_M), wrap))
    if spec["loop"]:
        # A lap has to end where it started, or every lap would climb or drop.
        drift = smoothed[-1] - smoothed[0]
        total = distances[-1]
        smoothed = [e - drift * (d / total) for e, d in zip(smoothed, distances)]

    climb = sum(max(0.0, b - a) for a, b in zip(smoothed, smoothed[1:]))
    course = {
        "id": course_id,
        "name": spec["name"],
        "place": spec["place"],
        "laps": spec["laps"],
        "loop": spec["loop"],
        "attribution": f"Map data © OpenStreetMap contributors (ODbL). Elevation: {DEMS[dem]}.",
        "lapDistanceM": round(distances[-1], 1),
        "lapClimbM": round(climb, 1),
        "points": [[round(d, 1), round(lat, 6), round(lon, 6), round(e, 1)]
                   for (d, lat, lon), e in zip(samples, smoothed)],
    }
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    out = OUT_DIR / f"{course_id}.json"
    out.write_text(json.dumps(course, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf-8")
    gain = smoothed[-1] - smoothed[0]
    print(f"[{course_id}] wrote {out.name}: lap {distances[-1] / 1000:.2f} km, "
          f"climb {climb:.0f} m, net {gain:+.0f} m, avg {100 * gain / distances[-1]:.1f} %")


def main(argv):
    wanted = argv[1:] or list(COURSES)
    for course_id in wanted:
        build(course_id, COURSES[course_id])


if __name__ == "__main__":
    main(sys.argv)
