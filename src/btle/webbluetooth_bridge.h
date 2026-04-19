#ifndef WEBBLUETOOTH_BRIDGE_H
#define WEBBLUETOOTH_BRIDGE_H

/*
 * WebBluetooth Bridge
 *
 * Exposes C++ callable functions that invoke the browser's WebBluetooth API
 * via Emscripten EM_JS macros.  On non-Wasm platforms this header is never
 * compiled (see btle.pri).
 *
 * Flow:
 *  1. scanForDevices()   → navigator.bluetooth.requestDevice(…)
 *  2. User picks a device in the browser dialog
 *  3. connectToDevice()  → device.gatt.connect() + discover services
 *  4. For each characteristic, startNotifications() is called
 *  5. On notification, js_notificationCallback() fires into C++ via a
 *     registered function pointer, feeding raw bytes to BtleHubWasm
 *
 * WebBluetooth security requirements:
 *  - Must be called from a user gesture (handled by BtleHubWasm / UI)
 *  - Page must be served over HTTPS
 */

#include <QObject>
#include <QByteArray>
#include <functional>

// C++ callback type invoked by JS when a BLE characteristic changes value.
// uuid16 is the 16-bit BT SIG UUID; data is the raw characteristic bytes.
using BleNotificationCallback = std::function<void(quint16 uuid16, const QByteArray &data)>;

// C++ callback type invoked by JS once the GATT connection and all
// characteristic notifications are fully set up and ready.
using BleConnectedCallback = std::function<void()>;

// C++ callback type invoked by JS when the GATT connection attempt fails.
// errorString contains a human-readable description of the failure.
using BleConnectionErrorCallback = std::function<void(const QString &errorString)>;

// C++ callback type invoked by JS when an unexpected GATT disconnection occurs
// and all automatic reconnect attempts have been exhausted.
using BleDisconnectedCallback = std::function<void()>;

// C++ callback type invoked by JS when the user presses the DOM reconnect
// button (ble-reconnect-btn) after a disconnect.
using BleReconnectRequestCallback = std::function<void()>;

namespace WebBluetoothBridge {

// Register the C++ callback that JS will invoke on each notification.
// Must be called before scanForDevices().
void setNotificationCallback(BleNotificationCallback cb);

// Register the C++ callback invoked once GATT connection and all
// characteristic notifications are fully ready.
void setConnectedCallback(BleConnectedCallback cb);

// Register the C++ callback invoked if the GATT connection attempt fails.
void setConnectionErrorCallback(BleConnectionErrorCallback cb);

// Register the C++ callback invoked when an unexpected GATT disconnection
// occurs (device powered off, went out of range, etc.).
void setDisconnectedCallback(BleDisconnectedCallback cb);

// Register the C++ callback invoked when the user presses the DOM reconnect
// button after a disconnect.
void setReconnectRequestCallback(BleReconnectRequestCallback cb);

// Expose window.mt_startBleScan() in the browser environment so that automated
// tests (Playwright) can trigger a BLE scan without a real UI user gesture.
// Safe to call unconditionally; in real browsers with no mock, requestDevice()
// still requires a user gesture and will silently fail if called without one.
void registerTestScanApi();

// Trigger navigator.bluetooth.requestDevice() – must be called from a
// user-initiated slot (button click, etc.) to satisfy browser security policy.
// On success, automatically connects and starts notifications, then fires
// the connected callback registered with setConnectedCallback().
void scanForDevices();

// Disconnect from the currently connected GATT device (if any).
void disconnectDevice();

// Send an FTMS resistance command (WriteWithoutResponse on 0x2AD9).
// grade is a slope percentage × 100 (e.g. 200 = 2.0 %).
void sendFtmsSetIndoorBikeSimulation(int grade100);

// Send an FTMS ERG load command (WriteWithoutResponse on 0x2AD9).
// watts is the target power in whole watts.
void sendFtmsSetTargetPower(int watts);

// Send FTMS opcode 0x00 (Request Control) to characteristic 0x2AD9.
// Must be called once after GATT connection is established, before any
// Set Target Power (0x05) or Indoor Bike Simulation (0x11) commands.
// This is called automatically inside scanForDevices() after connection.
void requestFtmsControl();

} // namespace WebBluetoothBridge

#endif // WEBBLUETOOTH_BRIDGE_H
