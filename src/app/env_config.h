#ifndef ENV_CONFIG_H
#define ENV_CONFIG_H

/**
 * @file env_config.h
 * @brief Environment variable names used by MaximumTrainer.
 *
 * Define compile-time string constants for every environment variable
 * the application reads at runtime.  Using constants avoids typos and
 * provides a single searchable location for all supported knobs.
 *
 * ------------------------------------------------------------------
 * Variable          | Default | Purpose
 * ------------------------------------------------------------------
 * MT_NO_NETWORK     |  (unset)| Disable all outbound network calls.
 *                   |         | Set to "1" in CI / headless runners
 *                   |         | where remote servers are unreachable.
 *                   |         | QProcess child processes inherit the
 *                   |         | variable automatically.
 * ------------------------------------------------------------------
 *
 * Usage in CI (GitHub Actions step):
 * @code{.yaml}
 *   env:
 *     MT_NO_NETWORK: "1"
 * @endcode
 *
 * Usage in C++:
 * @code{.cpp}
 *   #include "env_config.h"
 *   if (qEnvironmentVariableIntValue(EnvConfig::NoNetwork) != 0) {
 *       account->isOffline = true;
 *   }
 * @endcode
 */

namespace EnvConfig {

/**
 * @brief MT_NO_NETWORK — disable all outbound network calls.
 *
 * When set to any non-zero integer value the application forces
 * @c Account::isOffline = @c true immediately after startup, which
 * suppresses:
 *   - Radio list fetch (MainWindow::slotGetRadio)
 *   - Session check / @c UserDAO::putAccount
 *   - Intervals.icu sync, Strava upload, and all other REST calls
 *
 * Leave unset (or set to @c 0) for normal network access.
 *
 * @note This variable is set to @c "1" by the @c test_ui_navigation
 *       CI step on all three platforms (Linux, macOS, Windows) so
 *       that the launched @c MaximumTrainer child process runs fully
 *       offline during automated screenshot tests.
 */
constexpr const char *NoNetwork = "MT_NO_NETWORK";

} // namespace EnvConfig

#endif // ENV_CONFIG_H
