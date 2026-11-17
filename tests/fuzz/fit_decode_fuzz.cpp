// libFuzzer target: arbitrary bytes through the FIT SDK decoder, reading every
// field value (the code paths the #160 bounds fixes guard).
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "fit_decode.hpp"
#include "fit_mesg_broadcaster.hpp"
#include "fit_mesg_listener.hpp"
#include "fit_runtime_exception.hpp"

namespace {
struct ReadEveryField : fit::MesgListener {
    void OnMesg(fit::Mesg &mesg) override
    {
        for (FIT_UINT16 fieldIndex = 0; fieldIndex < mesg.GetNumFields(); ++fieldIndex) {
            const fit::Field *field = mesg.GetFieldByIndex(fieldIndex);
            for (FIT_UINT8 valueIndex = 0; valueIndex < field->GetNumValues(); ++valueIndex) {
                (void)field->GetFLOAT64Value(valueIndex);
                (void)field->GetSTRINGValue(valueIndex);
            }
        }
    }
};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const std::string bytes(reinterpret_cast<const char *>(data), size);
    std::istringstream integrityStream(bytes);
    fit::Decode integrityDecode;
    (void)integrityDecode.CheckIntegrity(integrityStream);

    std::istringstream stream(bytes);
    fit::Decode decode;
    fit::MesgBroadcaster broadcaster;
    ReadEveryField reader;
    broadcaster.AddListener(static_cast<fit::MesgListener &>(reader));
    try {
        decode.Read(&stream, &broadcaster, &broadcaster, nullptr);
    } catch (const fit::RuntimeException &) {
        // Malformed input is expected to be rejected this way.
    }
    return 0;
}
