#include "pch.h"

#include "Application.h"
#include "FractalRadio.h"

using namespace std;

constexpr auto DEFAULT_CLIENT_WIDTH  = 100;
constexpr auto DEFAULT_CLIENT_HEIGHT = 100;
constexpr auto NUM_FRAMES            = 3;
constexpr auto USE_WARP              = true;
constexpr auto V_SYNC                = true;

// ReSharper disable once CppInconsistentNaming
int CALLBACK wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
    const auto app = make_unique<Application>(hInstance, L"Fractal Radio", DEFAULT_CLIENT_WIDTH, DEFAULT_CLIENT_HEIGHT,
                                              NUM_FRAMES, USE_WARP, V_SYNC);
    app->Run<FractalRadio>();

    return 0;
}
