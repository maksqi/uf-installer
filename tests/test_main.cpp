#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <windows.h>

#include "core/log.h"

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    uf::log::Init(uf::GetFolder(uf::Folder::Temp) / L"uf-installer-tests.log");
    doctest::Context ctx(argc, argv);
    return ctx.run();
}
