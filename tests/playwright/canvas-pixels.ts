/**
 * Pixel-level helpers for asserting that the Qt WASM canvas actually rendered.
 *
 * The whole Qt application paints into a single `<canvas>` that is opaque to
 * DOM locators, so the only evidence that a view (e.g. the QML metric
 * dashboard) painted is the pixels themselves. These helpers decode a PNG
 * screenshot (via the dependency-free `pngjs` decoder) and reduce it to a few
 * robust, runner-independent statistics — deliberately avoiding pixel-exact
 * baselines, which are flaky across GL drivers and font rasterizers.
 */

import { PNG } from 'pngjs';

export interface PixelStats {
  width: number;
  height: number;
  /** Standard deviation of per-pixel luminance. ~0 ⇒ a flat/blank fill. */
  luminanceStdDev: number;
  /** Distinct colours after 5-bit-per-channel quantization. 1 ⇒ flat fill. */
  distinctColors: number;
}

/** Decode a PNG screenshot and compute blank-detection statistics. */
export function analyzePng(buffer: Buffer): PixelStats {
  const png = PNG.sync.read(buffer);
  const { width, height, data } = png; // data: RGBA, 4 bytes per pixel

  const colors = new Set<number>();
  let sum = 0;
  let sumSq = 0;
  const pixelCount = width * height;

  for (let i = 0; i < data.length; i += 4) {
    const r = data[i];
    const g = data[i + 1];
    const b = data[i + 2];

    const lum = 0.299 * r + 0.587 * g + 0.114 * b;
    sum += lum;
    sumSq += lum * lum;

    // Quantize to 5 bits/channel so anti-aliasing noise does not inflate the
    // count, while genuinely distinct UI colours still register.
    colors.add(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
  }

  const mean = sum / pixelCount;
  const variance = sumSq / pixelCount - mean * mean;
  const luminanceStdDev = Math.sqrt(Math.max(0, variance));

  return { width, height, luminanceStdDev, distinctColors: colors.size };
}

/**
 * Assert that a canvas screenshot is not blank — i.e. it contains real,
 * varied content rather than a single flat fill (the failure signature when
 * a Qt view or an embedded QQuickWidget fails to composite).
 */
export function assertNotBlank(buffer: Buffer, label: string): PixelStats {
  const stats = analyzePng(buffer);
  if (stats.luminanceStdDev < 2 || stats.distinctColors < 8) {
    throw new Error(
      `${label}: canvas appears blank — luminanceStdDev=` +
      `${stats.luminanceStdDev.toFixed(2)} distinctColors=${stats.distinctColors} ` +
      `(${stats.width}x${stats.height}). Expected varied rendered content.`,
    );
  }
  return stats;
}
