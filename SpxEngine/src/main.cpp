#include "log.h" // must be included before glfw3.h to avoid conflicts
#include "engine.h"
#include <iostream>


int main() {
    Engine engine;
    EngineConfig cfg;
    cfg.windowConfig.width = 1280;
    cfg.windowConfig.height = 720;
    cfg.windowConfig.title = "SPXEngine - Sandbox";
    cfg.windowConfig.vsync = true;
	cfg.enableImGui = true; // Enable ImGui for GUI rendering
	cfg.enableDocking = true; // Enable docking in ImGui    
	// Initialize engine with config 
    if (!engine.Initialize(cfg)) {
		LOG_DEBUG("Failed to initialize engine");
        return -1;
    }

    // Blocking loop managed by engine
    engine.Run();

    return 0;
}
