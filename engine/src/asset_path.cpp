// assets_path.cpp
#include "../include/asset_path.h"
#include <filesystem>

#include <string>

namespace fs = std::filesystem;

// Try several strategies in order:
// 1) Look upward from this source file's directory for a directory that contains "assets"
// 2) Look upward from the executable directory for a directory that contains "assets"
// 3) Fall back to exe_dir / relativePath
static fs::path FindAssetsRoot()
{
    // 1) Start from the directory of this source file (compile-time path)
    fs::path srcPath = fs::path(__FILE__).parent_path();

    fs::path cur = srcPath;
    while (true) {
        if (fs::exists(cur / "assets")) return cur;
        if (!cur.has_parent_path()) break;
        cur = cur.parent_path();
    }

    // 2) Start from executable directory (runtime)
    try {
        fs::path exe = fs::current_path(); // fallback to cwd

        cur = exe;
        while (true) {
            if (fs::exists(cur / "assets")) return cur;
            if (!cur.has_parent_path()) break;
            cur = cur.parent_path();
        }
    }
    catch (...) {
        // ignore and fall back
    }

    // 3) as last resort, return the executable/cwd path
    return fs::current_path();
}

std::string GetAssetPath(const std::string& relativePath)
{
    static fs::path assetsRoot = FindAssetsRoot();
    fs::path p = assetsRoot / relativePath;
    return p.make_preferred().string();
}

//#ifdef _WIN32
        // Try to get actual exe path on Windows
        /*char buf[MAX_PATH];
        DWORD len = ::GetModuleFileNameA(NULL, buf, (DWORD)MAX_PATH);
        if (len > 0 && len < MAX_PATH) exe = fs::path(std::string(buf, buf + len)).parent_path();*/
        //#endif