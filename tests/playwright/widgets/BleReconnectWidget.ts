import { type Locator, type Page } from '@playwright/test';

/**
 * Widget wrapping the `#ble-reconnect-overlay` modal that appears when the
 * Bluetooth trainer disconnects mid-workout and auto-reconnect attempts are
 * exhausted.
 *
 * The overlay is hidden by default (`display: none`) and shown by the
 * `gattserverdisconnected` handler in `webbluetooth_bridge.cpp`.
 */
export class BleReconnectWidget {
  /** Root `<div id="ble-reconnect-overlay">` overlay container */
  readonly root: Locator;
  /** `<button id="ble-reconnect-btn">` — triggers a new BLE device picker */
  readonly reconnectButton: Locator;
  /** `<button id="ble-reconnect-dismiss">` — hides the overlay without reconnecting */
  readonly dismissButton: Locator;

  private readonly page: Page;

  constructor(page: Page) {
    this.page            = page;
    this.root            = page.locator('#ble-reconnect-overlay');
    this.reconnectButton = page.locator('#ble-reconnect-btn');
    this.dismissButton   = page.locator('#ble-reconnect-dismiss');
  }

  /** `true` when the overlay is currently displayed. */
  async isVisible(): Promise<boolean> {
    return this.root.isVisible();
  }

  /**
   * Programmatically show the overlay by setting `display: flex`.
   * Simulates the JavaScript `gattserverdisconnected` handler after
   * automatic reconnect retries are exhausted.
   */
  async show(): Promise<void> {
    await this.page.evaluate(() => {
      const el = document.getElementById('ble-reconnect-overlay');
      if (el) el.style.display = 'flex';
    });
  }

  /** Click the primary "Reconnect" action button. */
  async clickReconnect(): Promise<void> {
    await this.reconnectButton.click();
  }

  /** Click the "Continue without trainer" dismiss button. */
  async clickDismiss(): Promise<void> {
    await this.dismissButton.click();
  }
}
