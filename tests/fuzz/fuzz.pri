# Shared settings for the libFuzzer targets. Build with clang, e.g.:
#   qmake6 -spec linux-clang QMAKE_CXX=clang++ QMAKE_LINK=clang++ fit_decode_fuzz.pro && make
# Run:  ../../build/tests/fit_decode_fuzz corpus/fit -max_total_time=60
QT       -= gui
CONFIG   += qt c++20 console sanitizer sanitize_address sanitize_undefined
CONFIG   -= app_bundle
TEMPLATE  = app
DESTDIR   = ../../build/tests
QMAKE_CXXFLAGS += -fsanitize=fuzzer-no-link -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g
QMAKE_LFLAGS   += -fsanitize=fuzzer
