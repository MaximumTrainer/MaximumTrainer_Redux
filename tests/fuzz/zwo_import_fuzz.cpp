// libFuzzer target: arbitrary bytes through the ZWO (Zwift/Intervals.icu
// workout XML) importer, including the Workout metrics computed on the result.
#include <cstddef>
#include <cstdint>

#include <QByteArray>
#include <QCoreApplication>
#include <QVariant>

#include "account.h"
#include "importerworkoutzwo.h"

namespace {
void ensureApplication()
{
    static int argc = 1;
    static char name[] = "zwo_import_fuzz";
    static char *argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    static Account account;
    static const bool registered = [] {
        qApp->setProperty("Account", QVariant::fromValue<Account *>(&account));
        return true;
    }();
    (void)registered;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ensureApplication();
    (void)ImporterWorkoutZwo::importFromByteArray(QByteArray(reinterpret_cast<const char *>(data), int(size)));
    return 0;
}
