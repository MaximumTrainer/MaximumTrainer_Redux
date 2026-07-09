#ifndef BTLE_UUIDS_H
#define BTLE_UUIDS_H

#include <QtGlobal>

/*
 * 16-bit BT SIG characteristic UUIDs used by BtleHub and the test suite.
 * Full 128-bit UUIDs are derived by the standard BT SIG base UUID expansion.
 */
static constexpr quint16 BTLE_UUID_HR_MEASUREMENT    = 0x2A37;
static constexpr quint16 BTLE_UUID_CSC_MEASUREMENT   = 0x2A5B;
static constexpr quint16 BTLE_UUID_POWER_MEASUREMENT = 0x2A63;
static constexpr quint16 BTLE_UUID_FTMS_BIKE_DATA    = 0x2AD2;
static constexpr quint16 BTLE_UUID_FTMS_ROWER_DATA   = 0x2AD1;
static constexpr quint16 BTLE_UUID_FTMS_FEATURE      = 0x2ACC;
static constexpr quint16 BTLE_UUID_BATTERY_LEVEL     = 0x2A19;

// Moxy Muscle Oxygen Monitor (proprietary, not BT SIG)
static constexpr quint16 BTLE_UUID_MOXY_SERVICE     = 0xAAB0;
static constexpr quint16 BTLE_UUID_MOXY_MEASUREMENT = 0xAAB2;

#endif // BTLE_UUIDS_H
