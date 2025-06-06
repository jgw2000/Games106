#include "base/vulkanexamplebase.h"

VulkanExampleBase* vulkanExample;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (vulkanExample != nullptr)
    {
        vulkanExample->handleMessage(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_  HINSTANCE hPrevInstance, _In_ LPSTR, _In_ int)
{
    for (int32_t i = 0; i < __argc; i++) { VulkanExampleBase::args.push_back(__argv[i]); };
    vulkanExample = new VulkanExampleBase();
    vulkanExample->initVulkan();
    vulkanExample->setupWindow(hInstance, WndProc);
    vulkanExample->prepare();
    vulkanExample->renderLoop();
    delete vulkanExample;
    return 0;
}