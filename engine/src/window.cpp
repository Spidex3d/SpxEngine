#define STB_IMAGE_IMPLEMENTATION
#include "window.h"
#include <Windows.h>
#include <memory>
#include <vector>
#include <commdlg.h>
#include "imgui\imgui.h"
#include <imgui\imgui_impl_glfw.h>
#include <imgui\imgui_impl_opengl3.h>
#include <imgui\ImGuiAF.h>

#include "fonts\fonts.h"

#include "stb/stb_image.h"
#include "../include/asset_path.h" // for GetAssetPath
#include "../include/globalVar.h"
#include "../include/entity.h"
#include "../include/engine.h" // for Engine and EngineConfig
#include "../src/input/EditorInput.h" // Include the appropriate header file for EditorInput 
#include "../src/Ui/icon_loader.h"
#include "../src/Sky/skyBox.h"
#include "../src/Effects/lighting.h"


#include "log.h"
#include <iostream>
#include <minwindef.h>
#include <shobjidl.h> // for IFileDialog / folder picking
#include "Textures\textures.h"


// initialize static refcount
int SpxWindow::s_glfwRefCount = 0;




// small helper to destroy existing framebuffer resources
static void DestroyFBO(GLuint& fbo, GLuint& color, GLuint& depth) {
    if (depth) { glDeleteRenderbuffers(1, &depth); depth = 0; }
    if (color) { glDeleteTextures(1, &color); color = 0; }
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
}

// Frees a vector<TexTexture> using TextureManager so refcounts are decremented.
static void FreeTexTextureList(std::vector<TexTexture>& list) {
    for (auto& t : list) {
        if (!t.path.empty()) {
            TextureManager::Unload(t.path);
        }
        else if (t.id) {
            TextureManager::Unload(t.id);
        }
        t.id = 0;
        t.TexFaceTexID = 0;
    }
    list.clear();
}

// Framebuffer size callback to handle window resizing events
void SpxWindow::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    void* up = glfwGetWindowUserPointer(window);
    if (!up) return;
    SpxWindow* self = reinterpret_cast<SpxWindow*>(up);
    if (self) {
        // update stored sizes (we don't expose setters here, but we can rely on GLFW queries)
        if (self->m_resizeCallback) self->m_resizeCallback(width, height);
    }
}

SpxWindow::SpxWindow(const WindowConfig& config)
    : m_config(config)
{
    if (s_glfwRefCount == 0) {
        if (!glfwInit()) {
            LOG_DEBUG("SpxWindow: Failed to initialize GLFW");
            return;
        }
    }
    ++s_glfwRefCount;

    // Default to OpenGL 4.6 core; adjust if you need different version/profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, m_config.resizable ? GLFW_TRUE : GLFW_FALSE);

    window = glfwCreateWindow(m_config.width, m_config.height, m_config.title, nullptr, nullptr);
    if (!window) {
        LOG_DEBUG("SpxWindow: Failed to create GLFW window");
        --s_glfwRefCount;
        if (s_glfwRefCount == 0) glfwTerminate();
        return;
    }

    // store pointer to this for callbacks
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    // Make context current here so a caller can initialize GL loader (glad) immediately
    glfwMakeContextCurrent(window);

    // Set vsync as requested
    SetVSync(m_config.vsync);
}

void SpxWindow::SetIcon(GLFWwindow* window)
{
    std::string iconPath = GetAssetPath(ICON_PATH);
    GLFWimage images[1];
    images[0].pixels = stbi_load(iconPath.c_str(), &images[0].width, &images[0].height, 0, 4); // rgba = png
    glfwSetWindowIcon(window, 1, images);
    stbi_image_free(images[0].pixels);
}
// ############################################# ImGui Set up #############################################
void SpxWindow::SetUpImGui(GLFWwindow* window) {
    // ImGui set up
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // enable viewports/docking depending on flag
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    if (m_enableDocking) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    const char* glsl_version = "#version 460 core";
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Fonts
    ImFontConfig fontconfig;
    fontconfig.MergeMode = true;
    fontconfig.PixelSnapH = true;
    static const ImWchar ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };


    fontconfig.GlyphOffset = ImVec2(0.0f, 1.0f);
    std::string fontPath = GetAssetPath(FONT_PATH_MAIN_REL);
    io.Fonts->AddFontFromFileTTF(fontPath.c_str(), FONT_SIZE);

	std::string AFfontPath  = GetAssetPath(FA_SOLID_PATH);
    io.Fonts->AddFontFromFileTTF(AFfontPath.c_str(), FONT_SIZE, &fontconfig, ranges);

	// #################################################### my fonts and icons test #####################################################
	std::string MYfontPath = GetAssetPath(MY_SOLID_PATH);
    static const ImWchar icon_ranges[] = { ICON_MIN_MY, ICON_MAX_MY, 0 }; 
    ImFontConfig cfg;
    cfg.MergeMode = true;
    cfg.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF(MYfontPath.c_str(), 18.0f, &cfg, icon_ranges);
}

void SpxWindow::NewImguiFrame(GLFWwindow* window)
{
    // New Frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void SpxWindow::SetEnableDocking(bool enabled)
{
    m_enableDocking = enabled;
}

bool SpxWindow::GetEnableDocking() const
{
    return m_enableDocking;
}

void SpxWindow::MainDockSpace(bool* p_open)
{
    if (m_enableDocking) {
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }
        else
        {
            dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
        }

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", p_open, window_flags);
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        ImGui::End();
    }
}

// ######### The main Imgui window for rendering the scene #########
void SpxWindow::MainSceneWindow(GLFWwindow* window)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::Begin("Main scene");

    // Available size in UI units
    const float window_width = ImGui::GetContentRegionAvail().x;
    const float window_height = ImGui::GetContentRegionAvail().y;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGuiIO& io = ImGui::GetIO();

    // ################################### toolbar at the top of the MainSceneWindow ######################################
    {
        // toolbar height in UI units
        const float tbHeight = 20.0f;
        // make a child row so the toolbar can be styled and won't interfere with other content
        ImGui::BeginChild("##scene_toolbar", ImVec2(ImGui::GetContentRegionAvail().x, tbHeight), false, ImGuiWindowFlags_NoDecoration);

        // tighten spacing for the toolbar
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));

        
        ImGui::PushID("top_Buttons");
       
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // normal
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.70f, 0.16f, 1.0f)); // hover
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.50f, 0.10f, 1.0f)); // active/click
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.8f, 1.0f)); // active/click
        
		ImGui::GetStyle().FrameBorderSize = 0.3f; // Add a border to the button
		ImGui::GetStyle().FrameRounding = 6.0f; // rounded corners of buttons

        //ImGui::SameLine();
        // Small separator
        

        // ICON_FA_CROSSHAIRS ICON_FA_CUBE  ICON_FA_CUBES ICON_FA_EDIT
		// ICON_FA_EXPAND_ARROWS_ALT ICON_FA_EXPAND ICON_FA_FILE ICON_FA_FOLDER ICON_FA_FOLDER_OPEN
        //  ICON_FA_PALETTE ICON_FA_OBJECT_GROUP ICON_FA_OBJECT_UNGROUP ICON_FA_SAVE ICON_FA_SPIDER
        //  ICON_FA_STREET_VIEW ICON_FA_TOOLS ICON_FA_TV
        // Transform / view tools
        if (ImGui::Button(ICON_FA_WATER "##Settings", ImVec2(30, 0))) {}
        ImGui::SameLine();
		// Edit mode, settings, toggle visibility keep button hilighted when active (for example if we are in edit mode,
        // the Edit button should be highlighted)
        if (ImGui::Button(ICON_FA_EXPAND "##EditOn", ImVec2(30, 0))) {
            if (m_actionCallback) m_actionCallback("EditOn");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Edit mode");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_COGS "##Settings", ImVec2(30, 0))) {
            if (m_actionCallback) m_actionCallback("OpenSettings");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Settings");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_EYE "##Toggle", ImVec2(30, 0))) {
            if (m_actionCallback) m_actionCallback("ToggleVisibility");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle");
        // Fill remaining horizontal space (optional)
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f);
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 300); // push next group to right

        // Right-aligned quick actions
        if (ImGui::Button(ICON_FA_PLAY "##play", ImVec2(30, 0))) {
            if (m_actionCallback) m_actionCallback("Play");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play");
        
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_PAUSE " ##Pause", ImVec2(30, 0))) {
            if (m_actionCallback) m_actionCallback("Pause");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_IMAGE " ##Render", ImVec2(30, 0))) {
            if (m_actionCallback) m_actionCallback("RenderSettings");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("RenderSettings");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_VIDEO "##Reset Cam", ImVec2(30, 0))) {
            if (m_actionCallback) m_actionCallback("resetCameraPos");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset camera position");
        ImGui::SameLine();
		// focus the camera on the selected object in the scene (if any)
        if (ImGui::Button(ICON_FA_VIDEO "##focus Cam", ImVec2(30, 0))) {
            if (m_actionCallback) m_actionCallback("focusSelected");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set camera to selected");


        ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(4); // pop all 4 pushed colors has to match top
        ImGui::PopID();
        ImGui::EndChild();

        // Small spacing after toolbar
        ImGui::Spacing();
    }
    // ######################################################### End Top Toolbar ######################################################
    
    

    //    // ######################### End Score board, resource #############################
    



    // Determine desired framebuffer pixel size (account for HiDPI scale)
    int desired_w = static_cast<int>(window_width * io.DisplayFramebufferScale.x);
    int desired_h = static_cast<int>(window_height * io.DisplayFramebufferScale.y);

    // Recreate framebuffer if size changed or not created yet
    if (desired_w > 0 && desired_h > 0) {
        if (desired_w != m_fbWidth || desired_h != m_fbHeight || m_fbo == 0) {
            Rescale_frambuffer((float)desired_w, (float)desired_h);
        }
    }

    // If we have an FBO, bind it, clear and render scene into it.
    if (m_fbo) {
        // Bind FBO and clear
        Bind_Framebuffer();
        glClearColor(0.12f, 0.15f, 0.18f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Call the registered render callback so Engine renders into the bound FBO.
        // If no callback is set, nothing happens (safe).
        if (m_renderCallback) {
            m_renderCallback();
        }
        
        // Done rendering to FBO
        Unbinde_Frambuffer();

        

        ImGui::Image((void*)(intptr_t)m_fboColor,
            ImVec2(window_width, window_height),
            ImVec2(0, 1), ImVec2(1, 0)); // uv0, uv1 flipped for GL

     // ##################################################### Picking ########################################################
            // After drawing the resulting texture inside the ImGui window:
            // store viewport rectangle for picking (screen coords and UI units)
            m_sceneViewportPos = ImGui::GetItemRectMin();
            m_sceneViewportSize = ImGui::GetItemRectSize();
    }
    else {
        // fallback: draw empty box or placeholder text
        ImGui::TextWrapped("Frame buffer not initialized.");
    }
	
    

    
	
    // Detect right-click for popup menu (existing UI code)
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImGui::OpenPopup("RightClickMenu");
    }

    
    if (ImGui::BeginPopup("RightClickMenu"))
    {
        

        if (ImGui::BeginMenu("Add a new model")) {
            if (ImGui::MenuItem("Obj Model")) {
                // Request engine to add a Obj via action callback
                if (m_actionCallback) m_actionCallback("AddObj");
            }

            if (ImGui::MenuItem("Gltf Model")) {
                // Request engine to add a Gltf via action callback
                if (m_actionCallback) m_actionCallback("AddGltf");
            }
            // other menu items...
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Add a new mesh")) {
            
            if (ImGui::MenuItem("Cube")) {
                // Request engine to add a cube via action callback
                if (m_actionCallback) m_actionCallback("AddCube");               				                
            }

            if (ImGui::MenuItem("Plane")) {               
                // Request engine to add a plane via action callback
                if (m_actionCallback) m_actionCallback("AddPlane");                               
            }
            // other menu items...
            ImGui::EndMenu();
        }
		// Lighting call to action callback to add a new light to the scene,
        // the type of light will be determined by the menu item clicked and passed as a string argument to the callback
        if (ImGui::BeginMenu("Add a new Light")) {
            if (ImGui::MenuItem("Ambient")) {
                if (m_actionCallback) m_actionCallback("AddLight:Ambient");
                
            }
          
            if (ImGui::MenuItem("Spot")) {
                if (m_actionCallback) m_actionCallback("AddLight:Spot");
            }

            if (ImGui::MenuItem("Area")) {
              //  if (m_actionCallback) m_actionCallback("AddLight:Area");
            }
            ImGui::EndMenu();
        }

		if (ImGui::BeginMenu("Terrain")) {

            if (ImGui::MenuItem("Tile")) {
                // Request engine to add a cube via action callback
                if (m_actionCallback) m_actionCallback("AddTile");
            }

            if (ImGui::MenuItem("Add Floor")) {
                if (m_actionCallback) m_actionCallback("AddFloor");
            }
            if (ImGui::MenuItem("Add Terrain")) {
                if (m_actionCallback) m_actionCallback("AddTerrain");
            }
                ImGui::EndMenu();
		}

        if (ImGui::BeginMenu("Sky")) {

            if (ImGui::MenuItem("Add Sky Box")) {
				ShouldAddSkyBox = true; // set a flag to trigger skybox addition
                if (m_actionCallback) m_actionCallback("AddSkyBox");
            }
            if (ImGui::MenuItem("Add Sky Sphere ")) {
               // if (m_actionCallback) m_actionCallback("AddSkySpere");
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Reset Camera")) {
            if (ImGui::MenuItem("Reset Camera Position")) {
                // reset camera position
                if (m_actionCallback) m_actionCallback("resetCameraPos");
            }
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    }
    // Update scene-window hovered state (used by EditorInput)
    m_sceneWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    ImGui::End();
    ImGui::PopStyleVar();
}

    

    void SpxWindow::MainScreenMenu(GLFWwindow* window)
    {
    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New scene"))
        {

        }
        if (ImGui::MenuItem("Open scene"))
        {
            std::string picked = openFileDialog();
            if (!picked.empty()) {
                if (m_actionCallback) m_actionCallback(std::string("LoadScene") + picked);
            }

        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_SAVE" Save scene"))
        {
			if (m_actionCallback) m_actionCallback("SaveScene"); // save the scene via action callback to engine to a json file
        }

        
        if (ImGui::MenuItem("Save As scene"))
        {
    
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_SIGN_OUT_ALT" Exit"))
        {
            glfwSetWindowShouldClose(window, true);
        }
        ImGui::EndMenu();
	}
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Cut"))
            {

            }
            if (ImGui::MenuItem("Copy"))
            {

            }
            if (ImGui::MenuItem("Paste"))
            {

            }
            ImGui::Separator();
            if (ImGui::MenuItem("Wire Frame"))
            {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            }
            if (ImGui::MenuItem("Wire Frame off"))
            {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::MenuItem(ICON_FA_COGS" Open Settings"))
            {
                //show_settings_window = true; // show settings window

            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Open Tool Box"))
            {

            }

            ImGui::EndMenu();
        }

    
	ImGui::EndMainMenuBar();
}

    void SpxWindow::ResourcesInspector(GLFWwindow* window)
    {


        ImGui::Begin(ICON_FA_EDIT" Resources Inspector");

        if (ImGui::BeginTabBar("##Main", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("Resource Lab"))
            {
                if (ImGui::CollapsingHeader(ICON_FA_EDIT" Texture Settings", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    // Add your texture settings UI elements here
                    // And display all the textures that we can use
                    ImGui::Text("Texture filtering, wrapping, etc. can go here.");
                }
                ImGui::EndTabItem();
            }
            auto flags = ImGuiTreeNodeFlags_DefaultOpen;
            if (ImGui::BeginTabItem("Camera Lab"))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));


                ImGui::PushID("cam_Buttons");

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // normal
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.70f, 0.16f, 1.0f)); // hover
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.50f, 0.10f, 1.0f)); // active/click
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.8f, 1.0f)); // active/click

                ImGui::GetStyle().FrameBorderSize = 0.3f; // Add a border to the button
                ImGui::GetStyle().FrameRounding = 6.0f; // rounded corners of buttons
                // Camera Main

                if (ImGui::Button(ICON_FA_VIDEO " Reset Cam", ImVec2(70, 0))) {
                    if (m_actionCallback) m_actionCallback("resetCameraPos");
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset camera position");
                ImGui::SameLine();
                // focus the camera on the selected object in the scene (if any)
                if (ImGui::Button(ICON_FA_VIDEO " Focus Cam", ImVec2(70, 0))) {
                    if (m_actionCallback) m_actionCallback("focusSelected");
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set camera to selected");
                // Camera Top
                if (ImGui::Button(ICON_FA_VIDEO " Top Cam", ImVec2(70, 0))) {
                    if (m_actionCallback) m_actionCallback("TopDownView");
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set camera to selected");
                ImGui::SameLine(); //new
                if (ImGui::Button(ICON_FA_VIDEO " Back Cam", ImVec2(70, 0))) {
                    if (m_actionCallback) m_actionCallback("PerspectiveView");
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set camera to selected");
                // PerspectiveView

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(4); // pop all 4 pushed colors has to match top
                ImGui::PopID();

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Render Lab"))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));


                ImGui::PushID("cam_Buttons");

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // normal
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.70f, 0.16f, 1.0f)); // hover
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.50f, 0.10f, 1.0f)); // active/click
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.8f, 1.0f)); // active/click

                ImGui::GetStyle().FrameBorderSize = 0.3f; // Add a border to the button
                ImGui::GetStyle().FrameRounding = 6.0f; // rounded corners of buttons

                ImGui::Text("ID: Render Lab");
                ImGui::Text("Spidex Engine New Render Lab", nullptr);

                if (ImGui::Button("Render Image")) {
                    std::cout << "Render The Image on a new form" << std::endl;
                }

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(4); // pop all 4 pushed colors has to match top
                ImGui::PopID();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Help Lab"))
            {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), ICON_FA_AMBULANCE " Help doc's");
                ImGui::Text("SpxEngine New Help doc's", nullptr);
                ImGui::Text("To reset Camera from topCam press back Cam then Reset Cam", nullptr);
                ImGui::TextWrapped("Models Created/distributed by Kay Lousberg"
                    "www.kaylousberg.com Creation date : 22 / 10 / 2025 09  ", nullptr);
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "SpxEngine");
                ImGui::SeparatorText(" Info ");
                ImGui::TextWrapped("Hi I'm The Dyslexic Programmer.I'm not really a programmer, I'm a retired horticulturist."
                    "iv only played with C++ and found it very interesting,"
                    "so now I have lots of time to try and learn this stuff."
                    "my aim is to go from all but zero to making a 3d Game Engine using Opengl."
                );
                ImGui::SeparatorText( " GitHub ");
                ImGui::Text("https://github.com/Spidex3d/SpxEngine");
                ImGui::EndTabItem();
            }



            ImGui::EndTabItem();

        }

        ImGui::End();
    }
    // Sky lighting  Particles ICON_FA_IMAGE
   
    


    void SpxWindow::EnvironmentExplorer(GLFWwindow* window)
    {

            static std::vector<SkyTexture> skyTexture;
            static bool cached = false;
            // Hard coded for now
            static std::string skyFolder = "C:/Users/marty/Desktop/Models/Textures/Skybox/NewSky/"; // <--- change to your folder or use Browse
            
			const ImVec2 previewSize(64, 64); // button size for skybox previews in the grid

            // Header / controls
            ImGui::Begin(ICON_FA_EDIT " Environment Inspector");

            if (ImGui::BeginTabBar("##MainEnviro", ImGuiTabBarFlags_None))
            {

                if (ImGui::BeginTabItem("Sky Lab"))
                {

                    ImGui::Text("Sky Lab");
                              

                  if (ShouldAddSkyBox) {
                        // Load once (or after Reload/Browse). Use a temporary LoadSkybox to call the helper.
                        if (!cached) {
                            //cachedSkies.clear();
                            skyTexture.clear();
                            

                            // Create a small temporary loader object to reuse your load function.
                            LoadSkybox tmpLoader(0, "tmp", 0);
                            try {
                                skyTexture = tmpLoader.loadSkyTextureFromFolder(skyFolder);

                            }
                            catch (...) {
                                //cachedSkies.clear();
								skyTexture.clear();
                            }
                            cached = true;
                        }

                    
                    // Grid display
                      int columns = 3;
                      int count = 0;
                  
                       //for (const auto& st : cachedSkies) {
                       for (const auto& st : skyTexture) {
                           ImGui::PushID(st.id);
                           ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
                           ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));


                           ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // normal
                           ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.70f, 0.16f, 1.0f)); // hover
                           ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.50f, 0.10f, 1.0f)); // active/click
                           ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.8f, 1.0f)); // active/click

                           ImGui::GetStyle().FrameBorderSize = 0.3f; // Add a border to the button
                           ImGui::GetStyle().FrameRounding = 6.0f; // rounded corners of buttons
                       
                           // Use the preview texture (frontFaceTexID). If zero, show a placeholder button.
                           if (st.frontFaceTexID != 0) {
                               ImGui::ImageButton((void*)(intptr_t)st.frontFaceTexID, previewSize, ImVec2(0, 1), ImVec2(1, 0));
                       
							   // from here we need to go to CreatSkyBox in Engine to create the skybox and add it to the scene,
                               // we can pass the path of the skybox folder as a parameter to the callback and then load the textures
                               // again in Engine and create the skybox,
                               // this way we can reuse the loading code and also ensure that the textures are loaded in the correct order
                               // for the skybox creation
							   if (ImGui::IsItemClicked()) {
								   if (m_actionCallback) {
                                       LOG_INFO("EnvExplorer: requested AddSkyBox for" << st.path.c_str());

									   m_actionCallback(std::string("AddSkyBox:") + st.path); // correct path to the skybox folder & file
								   }
							   }
                           }
                           else {
                               // Placeholder box
                               ImGui::Button("No Preview", previewSize);
                               if (m_actionCallback) {
                                   m_actionCallback(std::string("AddSkyBox:") + st.path);
                               }
                       
                           }
                           ImGui::PopStyleVar(2);
                           ImGui::PopStyleColor(4); // pop all 4 pushed colors has to match top
                           ImGui::PopID();
                           // layout: 3 columns
                           if (++count % columns != 0) ImGui::SameLine();
                           //++idx;
                       }
                       
                  } // end if for add sky
                       

                    ImGui::EndTabItem();
				} // ######################################### End Sky Lab #########################################

                if (ImGui::BeginTabItem("Lighting Lab"))
                {
                    // Guard: ensure we have a LightManager
                    if (!m_lightManager) {
                        ImGui::TextWrapped("Lighting not available (no LightManager attached).");
                    }
                    else {
                        glm::vec3 amb = m_lightManager->GetGlobalAmbient();
                        float ambColor[4] = { amb.r, amb.g, amb.b, 1.0f };
                        float ambIntensity = m_lightManager->GetGlobalAmbientIntensity();

                        // In-window controls (you don't need a nested Begin here)
                        // ColorEdit4 (alpha not used by lighting but useful visually)
                        if (ImGui::ColorEdit4("Ambient Color", ambColor)) {
                            m_lightManager->SetGlobalAmbient(glm::vec3(ambColor[0], ambColor[1], ambColor[2]), ambIntensity);
                        }

                        // Intensity slider
                        if (ImGui::SliderFloat("Ambient Intensity", &ambIntensity, 0.0f, 5.0f)) {
                            m_lightManager->SetGlobalAmbient(glm::vec3(ambColor[0], ambColor[1], ambColor[2]), ambIntensity);
                        }

                        ImGui::Text("Note: shaders use uniform 'u_ambient'");
                    }

                    ImGui::EndTabItem();
                }


                if (ImGui::BeginTabItem("Particles Lab"))
                {
                    // TO DO Later
                    // Dispaly types of lighting we can use (directional, point, spot, etc.) as image buttons


                    ImGui::EndTabItem();
                } // End Particles Lab

                ImGui::EndTabBar();

            }

            ImGui::End();

        
    }

    void SpxWindow::AssetBrowser(GLFWwindow* window)
    {

        //static std::vector<TexTexture> texTexture; ! not needed here
        //static std::vector<TexTexture> m_Texture;

        ImGui::Begin(ICON_FA_EDIT " Asset Browser");
        if (ImGui::BeginTabBar("##MainEnviro", ImGuiTabBarFlags_None))
            if (ImGui::BeginTabItem("Level Template"))
            {
                // Dispaly types of Levels to set up ie: Blank, default, FPS, Top Down, ect; we can use as image buttons
				// Look in Scene_Files for .spxscene files and display them as image buttons with a preview image if available,
                // clicking a button will load the scene via action callback to engine.
               // std::string SceneSelected = "C://Users//marty//Desktop//SPXEngine//SPXEngine//SpxEngine//engine//Scene_Files//";
                std::string SceneSelected = GetAssetPath(SCENE_PATH);
                if (!SceneSelected.empty()) {
                    // Free previously cached previews to avoid leaks (must be called with GL context)
                    //FreeTexTextureList(texTexture);
                    FreeTexTextureList(m_texTextures);

                    // Load new previews (this will create GL textures and return vector)
                   // texTexture = TextureManager::LoadTexturesFromDirectory(picked);
                    m_texTextures = TextureManager::LoadTexturesFromDirectory(SceneSelected);
                    LOG_INFO("AssetBrowser: loaded " << m_texTextures.size() << " textures from " << SceneSelected);
                }
                else {
                    LOG_INFO("AssetBrowser: openFolderDialog cancelled or no folder selected");
                }
                const ImVec2 previewSize(64, 64);
                int columns = 6;
                int count = 0;

                for (const auto& st : m_texTextures) {
                    ImGui::PushID(st.id);

                    // style used in your original code (optional)
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.70f, 0.16f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.50f, 0.10f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.8f, 1.0f));
                    ImGui::GetStyle().FrameBorderSize = 0.3f;
                    ImGui::GetStyle().FrameRounding = 6.0f;

                    // Group button+label so layout stays together
                    ImGui::BeginGroup();

                    bool clicked = false;
                    if (st.TexFaceTexID != 0) {
                        // ImageButton returns true when clicked
                        if (ImGui::ImageButton((void*)(intptr_t)st.TexFaceTexID, previewSize, ImVec2(0, 1), ImVec2(1, 0))) {
                            clicked = true;
                        }
                    }
                    else {
                        // placeholder if no preview
                        if (ImGui::Button("No Preview", previewSize)) {
                            clicked = true;
                        }
                    }

                    // filename to show under the preview: use the image file's stem and show .spxscene
                    std::string stem = std::filesystem::path(st.path).stem().string();
                    std::string sceneLabel = stem + ".spxscene";

                    // center the label under the image
                    ImVec2 textSize = ImGui::CalcTextSize(sceneLabel.c_str());
                    float offset = (previewSize.x - textSize.x) * 0.5f;
                    if (offset > 0.0f) {
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
                    }
					ImGui::TextWrapped("%s", sceneLabel.c_str()); // long names will wrap under the button, which is why we use TextWrapped here instead of Text

                    ImGui::EndGroup();

                    // handle click after grouping so you can reuse same click logic
                    if (clicked && m_actionCallback) {
                        std::string scenePath = st.path.substr(0, st.path.find_last_of('.')) + ".spxscene";
                        LOG_INFO("AssetBrowser: requested Scene File for " << st.path.c_str());
                        m_actionCallback(std::string("LoadScene") + scenePath);
                    }

                    // clean up style & ID
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(4);
                    ImGui::PopID();

                    // layout in columns
                    if (++count % columns != 0) ImGui::SameLine();
                }

       
              

                ImGui::EndTabItem();
            } // End Level Lab
            if (ImGui::BeginTabItem("Texture Lab"))
            {
                    // TO DO Later
                    // Dispaly types of Textues we can use as image buttons
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));


                ImGui::PushID("tex_Buttons");

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // normal
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.70f, 0.16f, 1.0f)); // hover
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.50f, 0.10f, 1.0f)); // active/click
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.8f, 1.0f)); // active/click

                ImGui::GetStyle().FrameBorderSize = 0.3f; // Add a border to the button
                ImGui::GetStyle().FrameRounding = 6.0f; // rounded corners of buttons

                ImGui::Text("Spidex Engine New Texture Lab", nullptr);                

                if (ImGui::Button("Open Texture")) {
                 std::string picked = openFolderDialog();
                    if (!picked.empty()) {
                        // Free previously cached previews to avoid leaks (must be called with GL context)
                        //FreeTexTextureList(texTexture);
                        FreeTexTextureList(m_texTextures);

                        // Load new previews (this will create GL textures and return vector)
                       // texTexture = TextureManager::LoadTexturesFromDirectory(picked);
                        m_texTextures = TextureManager::LoadTexturesFromDirectory(picked);

                       // LOG_INFO("AssetBrowser: loaded " << texTexture.size() << " textures from " << picked);
                        LOG_INFO("AssetBrowser: loaded " << m_texTextures.size() << " textures from " << picked);
                    }
                    else {
                        LOG_INFO("AssetBrowser: openFolderDialog cancelled or no folder selected");
                    }
                    
                }

                

                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(4); // pop all 4 pushed colors has to match top
                ImGui::PopID();

                int columns = 20;
                int count = 0;
                const ImVec2 previewSize(32, 32);

                //for (const auto& st : texTexture) {
                for (const auto& st : m_texTextures) {
                    ImGui::PushID((int)st.id);

                    if (st.TexFaceTexID != 0) {
                        if (ImGui::ImageButton((void*)(intptr_t)st.TexFaceTexID, previewSize, ImVec2(0, 1), ImVec2(1, 0))) {
                            // Send AddTexture action to Engine with the file path
                            if (m_actionCallback) {
                                m_actionCallback(std::string("AddTexture:") + st.path);
                            }
                        }
                    }
                    else {
                        ImGui::Button("No Preview", previewSize);
                    }

                    ImGui::PopID();
                    if (++count % columns != 0) ImGui::SameLine();
                }

                ImGui::EndTabItem();
            } // End Texture Lab
            if (ImGui::BeginTabItem("Obj Lab"))
            {
                // TO DO Later
                // Dispaly types of Obj models we can use as image buttons


                ImGui::EndTabItem();
            } // End Obj Lab
            if (ImGui::BeginTabItem("Gltf Lab"))
            {
                // TO DO Later
                // Dispaly types of Gltf models we can use  as image buttons


                ImGui::EndTabItem();
            } // End Gltf Lab
            if (ImGui::BeginTabItem("Test Lab"))
            {
                // TO DO Later
                // Dispaly types of Materials we can use as image buttons
                // ImGui::Button((std::string(ICON_MY_LOAD) + " Load").c_str());
                ImGui::Button(ICON_MY_ADD_NEW   " Add New");
                ImGui::Button(ICON_MY_REMOVE    " Remove");
                ImGui::Button(ICON_MY_LOAD      " Load");
                ImGui::Button(ICON_MY_SAVE      " Save");
                ImGui::Button(ICON_MY_SKY       " Sky");
                ImGui::Button(ICON_MY_TERRAIN   " Terrain");

                ImGui::EndTabItem();
            }
			ImGui::EndTabBar();

		ImGui::End();

    }

    

// Create or recreate the FBO using current window size if needed
void SpxWindow::Creat_FrameBuffer()
{
    int w = GetWidth();
    int h = GetHeight();
    if (w <= 0 || h <= 0) return;
    Rescale_frambuffer((float)w, (float)h);
}

// Bind offscreen FBO for rendering. Sets viewport to framebuffer size.
void SpxWindow::Bind_Framebuffer()
{
    if (!m_fbo) return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_fbWidth, m_fbHeight);
}

// Unbind FBO and restore default framebuffer / viewport to window size
void SpxWindow::Unbinde_Frambuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    int w = GetWidth();
    int h = GetHeight();
    if (w > 0 && h > 0) {
        glViewport(0, 0, w, h);
    }
}

// Recreate framebuffer at requested pixel size (width, height in pixels)
// This deletes previous attachments safely and creates a color texture + depth renderbuffer.
void SpxWindow::Rescale_frambuffer(float width, float height)
{
    int w = static_cast<int>(width);
    int h = static_cast<int>(height);
    if (w <= 0 || h <= 0) return;

    // If same size, nothing to do
    if (m_fbo && m_fbWidth == w && m_fbHeight == h) return;

    // Destroy old attachments (if any)
    DestroyFBO(m_fbo, m_fboColor, m_fboDepth);

    // Create new color texture
    glGenTextures(1, &m_fboColor);
    glBindTexture(GL_TEXTURE_2D, m_fboColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Optional: clamp
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create depth+stencil renderbuffer
    glGenRenderbuffers(1, &m_fboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_fboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // Create framebuffer and attach
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fboColor, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_fboDepth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_WARNING("Failed to create framebuffer: status=0x%x", (unsigned)status);
        DestroyFBO(m_fbo, m_fboColor, m_fboDepth);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_fbWidth = m_fbHeight = 0;
        return;
    }

    // success
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_fbWidth = w;
    m_fbHeight = h;
    LOG_INFO("Created FBO %u (color=%u depth=%u) size=%dx%d", (unsigned)m_fbo, (unsigned)m_fboColor, (unsigned)m_fboDepth, w, h);
}


void SpxWindow::SetRenderCallback(RenderCallback cb) {
    m_renderCallback = std::move(cb);
}
// Action callback management (UI -> engine commands) like add plane, etc.
void SpxWindow::SetActionCallback(ActionCallback cb)
{
    m_actionCallback = std::move(cb);
}
int SpxWindow::GetFramebufferWidth() const { return m_fbWidth; }
int SpxWindow::GetFramebufferHeight() const { return m_fbHeight; }
GLuint SpxWindow::GetFramebufferColorTexture() const { return m_fboColor; }



void SpxWindow::RenderImGui(GLFWwindow* window)
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Handle multiple viewports / platform windows
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}
void SpxWindow::ImGuiShutdown()
{
    // destroy framebuffer resources when shutting down
    DestroyFBO(m_fbo, m_fboColor, m_fboDepth);
    if (m_playIconTex) { glDeleteTextures(1, &m_playIconTex); m_playIconTex = 0; }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

}
// ############################################# End ImGui Set up #############################################

SpxWindow::~SpxWindow() {
    // destroy FBO if still present
    DestroyFBO(m_fbo, m_fboColor, m_fboDepth);

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    --s_glfwRefCount;
    if (s_glfwRefCount == 0) {
        glfwTerminate();
    }
}

bool SpxWindow::IsValid() const {
    return window != nullptr;
}

bool SpxWindow::ShouldClose() const {
    return !IsValid() || glfwWindowShouldClose(window);
}

void SpxWindow::PollEvents() {
    // Delegate to GLFW (safe to call even if other code also polls)
    glfwPollEvents();
}

void SpxWindow::SwapBuffers() {
    if (window) glfwSwapBuffers(window);
}

int SpxWindow::GetWidth() const {
    if (!window) return 0;
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    return w;
}

int SpxWindow::GetHeight() const {
    if (!window) return 0;
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    return h;
}

void SpxWindow::SetVSync(bool enabled) {
    if (!window) return;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(enabled ? 1 : 0);
    m_config.vsync = enabled;
}

void SpxWindow::SetResizeCallback(ResizeCallback cb) {
    m_resizeCallback = cb;
}



void* SpxWindow::GetNativeWindow() const {
    return reinterpret_cast<void*>(window);
}


//New Openfile dialog and return the selected file path as a string. Uses Windows API.
std::string SpxWindow::openFileDialog()
{
    OPENFILENAMEW ofn;
    // Wide buffer for file path
    std::vector<wchar_t> filename(MAX_PATH, L'\0');

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);

    // If you have a native HWND for the window, put it here; otherwise NULL is fine.
    ofn.hwndOwner = NULL;

    ofn.lpstrFile = filename.data();
    ofn.nMaxFile = static_cast<DWORD>(filename.size());

    // Double-null terminated wide-string filter (last \0 terminates the filter list)
    static const wchar_t filter[] =
        L"Image Files\0*.jpg;*.jpeg;*.png;*.bmp\0"
        L"All Files\0*.*\0\0";
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;

    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;

    // Flags: require existing path/file, Explorer-style dialog
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn)) {
        // Convert selected wide string to UTF-8
        int required = WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, nullptr, 0, nullptr, nullptr);
        if (required > 0) {
            std::vector<char> utf8(required, 0);
            WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, utf8.data(), required, nullptr, nullptr);
            return std::string(utf8.data());
        }
        else {
            LOG_WARNING("openFileDialog: WideCharToMultiByte failed converting path.");
            return std::string();
        }
    }
    else {
        // If user cancelled, CommDlgExtendedError returns 0. Otherwise log the error code.
        DWORD err = CommDlgExtendedError();
        if (err != 0) {
            LOG_WARNING("openFileDialog: GetOpenFileNameW failed, CommDlgExtendedError=" << err);
        }
        return std::string();
    }
}

std::string SpxWindow::openSaveFileDialog(const char* defaultExt, const char* filter)
{
    // Uses Win32 GetSaveFileNameW to show a Save dialog and returns UTF-8 path.
    OPENFILENAMEW ofn;
    std::vector<wchar_t> filename(MAX_PATH, L'\0');

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL; // or obtain HWND from GLFW if you want parented dialog
    ofn.lpstrFile = filename.data();
    ofn.nMaxFile = static_cast<DWORD>(filename.size());

    // filter must be a double-null terminated wide string. We accept UTF-8 filter parameter.
    // Convert filter to wide char and ensure double-null termination.
    std::wstring wfilter;
    if (filter && filter[0]) {
        int required = MultiByteToWideChar(CP_UTF8, 0, filter, -1, nullptr, 0);
        if (required > 0) {
            wfilter.resize(required);
            MultiByteToWideChar(CP_UTF8, 0, filter, -1, &wfilter[0], required);
            // Ensure double null termination
            if (wfilter.size() == 0 || wfilter.back() != L'\0') wfilter.push_back(L'\0');
        }
    }
    else {
        wfilter = L"JSON Files\0*.json\0All Files\0*.*\0\0";
    }
    ofn.lpstrFilter = wfilter.c_str();
    ofn.nFilterIndex = 1;

    // Default extension (e.g. "json")
    std::wstring wdefExt;
    if (defaultExt && defaultExt[0]) {
        int req = MultiByteToWideChar(CP_UTF8, 0, defaultExt, -1, nullptr, 0);
        if (req > 0) {
            wdefExt.resize(req);
            MultiByteToWideChar(CP_UTF8, 0, defaultExt, -1, &wdefExt[0], req);
        }
    }
    ofn.lpstrDefExt = wdefExt.empty() ? nullptr : wdefExt.c_str();

    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;

    // Prompt to overwrite existing files
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER;

    std::string result;

    if (GetSaveFileNameW(&ofn)) {
        // Convert wide string to UTF-8
        int required = WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, nullptr, 0, nullptr, nullptr);
        if (required > 0) {
            std::vector<char> utf8(required, 0);
            WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, utf8.data(), required, nullptr, nullptr);
            result.assign(utf8.data());
        }
    }
    else {
        DWORD err = CommDlgExtendedError();
        if (err != 0) {
            LOG_WARNING("openSaveFileDialog: GetSaveFileNameW failed, CommDlgExtendedError=" << err);
        }
    }

    return result;
}

std::string SpxWindow::openFolderDialog()
{
    // Initialize COM for this thread (balanced with CoUninitialize below).
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        LOG_WARNING("openFolderDialog: CoInitializeEx failed");
        return std::string();
    }

    std::string result;

    IFileDialog* pFileDialog = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileDialog));
    if (SUCCEEDED(hr) && pFileDialog) {
        // Ask for folder selection only
        DWORD options = 0;
        if (SUCCEEDED(pFileDialog->GetOptions(&options))) {
            pFileDialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }

        // Show dialog (NULL owner; you can pass HWND if you want to parent it)
        hr = pFileDialog->Show(NULL);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileDialog->GetResult(&pItem)) && pItem) {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
                    // convert wide string to UTF-8
                    int required = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, nullptr, 0, nullptr, nullptr);
                    if (required > 0) {
                        std::vector<char> utf8(required, 0);
                        WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, utf8.data(), required, nullptr, nullptr);
                        result.assign(utf8.data());
                    }
                    CoTaskMemFree(pszPath);
                }
                pItem->Release();
            }
        }
        else {
            // user cancelled or error; do nothing (result is empty)
            DWORD dlgErr = CommDlgExtendedError();
            (void)dlgErr; // ignore or log if you like
        }

        pFileDialog->Release();
    }
    else {
        LOG_WARNING("openFolderDialog: CoCreateInstance(CLSID_FileOpenDialog) failed");
    }

    CoUninitialize();
    return result;
}
 



