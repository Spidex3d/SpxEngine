#pragma once
#include <GLFW/glfw3.h>
#include <vector>

class Input {
public:
	explicit Input(GLFWwindow* window); // constructor takes GLFWwindow pointer
    // Polls events and updates internal key states.
    // Call once per frame (it calls glfwPollEvents()).
    void Update();

    bool HasKeyboardAttached() const;

    // Continuous: true while key is down
    bool IsKeyDown(int glfwKey) const;

    // Edge: true only on the frame the key became pressed
    bool IsKeyPressed(int glfwKey) const;

    // Edge: true only on the frame the key was released
    bool IsKeyReleased(int glfwKey) const;

private:
    GLFWwindow* window = nullptr;
    std::vector<char> currFrame; // current frame state
    std::vector<char> prevFrame; // previous frame state
};
