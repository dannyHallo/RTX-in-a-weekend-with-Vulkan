#include "Window.hpp"
#include "utils/Logger.hpp"

#include <functional>

namespace {} // namespace

Window::Window(WindowStyle windowStyle, int widthIfWindowed, int heightIfWindowed)
    : mWidthIfWindowed(widthIfWindowed), mHeightIfWindowed(heightIfWindowed) {
  auto result = glfwInit();
  assert(result == GLFW_TRUE && "Failed to initialize GLFW");

  mMonitor = glfwGetPrimaryMonitor();
  assert(mMonitor != nullptr && "Failed to get primary monitor");

  // get primary monitor for future maximize function
  // may be used to change mode for this program
  const GLFWvidmode *mode = glfwGetVideoMode(mMonitor);
  assert(mode != nullptr && "Failed to get video mode");

  // only OpenGL Api is supported, so no API here
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  glfwWindowHint(GLFW_RED_BITS, mode->redBits);
  glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
  glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);       // adapt colors (notneeded)
  glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate); // adapt framerate

  // create a windowed fullscreen window temporalily, to obtain its property
  mWindow = glfwCreateWindow(mode->width, mode->height, "Loading window...", nullptr, nullptr);
  glfwMaximizeWindow(mWindow);
  glfwGetWindowPos(mWindow, 0, &mTitleBarHeight);
  glfwGetFramebufferSize(mWindow, &mMaximizedFullscreenClientWidth,
                         &mMaximizedFullscreenClientHeight);

  // change the created window to the desired style
  setWindowStyle(windowStyle);

  mWindowStyle = windowStyle;

  if (mCursorState == CursorState::kInvisible) {
    hideCursor();
  } else {
    showCursor();
  }

  glfwSetWindowUserPointer(mWindow, this); // set this pointer to the window class
  glfwSetKeyCallback(mWindow, _keyCallback);
  glfwSetCursorPosCallback(mWindow, _cursorPosCallback);
  glfwSetFramebufferSizeCallback(mWindow, _frameBufferResizeCallback);
}

Window::~Window() {
  glfwDestroyWindow(mWindow);
  glfwTerminate();
}

void Window::toggleWindowStyle() {
  switch (mWindowStyle) {
  case WindowStyle::kNone:
    assert(false && "Cannot toggle window style while it is none");
    break;
  case WindowStyle::kFullScreen:
    setWindowStyle(WindowStyle::kMaximized);
    break;
  case WindowStyle::kMaximized:
    setWindowStyle(WindowStyle::kHover);
    break;
  case WindowStyle::kHover:
    setWindowStyle(WindowStyle::kFullScreen);
    break;
  }
}

void Window::setWindowStyle(WindowStyle newStyle) {
  if (newStyle == mWindowStyle) {
    return;
  }

  const GLFWvidmode *mode = glfwGetVideoMode(mMonitor);
  assert(mode != nullptr && "Failed to get video mode");

  switch (newStyle) {
  case WindowStyle::kNone:
    assert(false && "Cannot set window style to none");
    break;

  case WindowStyle::kFullScreen:
    glfwSetWindowMonitor(mWindow, mMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    break;

  case WindowStyle::kMaximized:
    glfwSetWindowMonitor(mWindow, nullptr, 0, mTitleBarHeight, mMaximizedFullscreenClientWidth,
                         mMaximizedFullscreenClientHeight, mode->refreshRate);
    break;

  case WindowStyle::kHover:
    int hoverWindowX =
        static_cast<int>(mMaximizedFullscreenClientWidth / 2.F - mWidthIfWindowed / 2.F);
    int hoverWindowY =
        static_cast<int>(mMaximizedFullscreenClientHeight / 2.F - mHeightIfWindowed / 2.F);
    glfwSetWindowMonitor(mWindow, nullptr, hoverWindowX, hoverWindowY, mWidthIfWindowed,
                         mHeightIfWindowed, mode->refreshRate);
    break;
  }

  mWindowStyle = newStyle;
}

void Window::showCursor() {
  glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  mCursorState = CursorState::kVisible;
  glfwSetCursorPos(mWindow, getFrameBufferWidth() / 2.F, getFrameBufferHeight() / 2.F);
}

void Window::hideCursor() {
  glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  if (glfwRawMouseMotionSupported() != 0) {
    glfwSetInputMode(mWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
  }

  mCursorState = CursorState::kInvisible;
}

void Window::toggleCursor() {
  if (mCursorState == CursorState::kInvisible) {
    showCursor();
  } else {
    hideCursor();
  }
}

void Window::addMouseCallback(std::function<void(float, float)> callback) {
  mouseCallbacks.emplace_back(std::move(callback));
}

void Window::_keyCallback(GLFWwindow *window, int key, int /*scancode*/, int action, int /*mods*/) {
  auto *thisWindowClass = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));

  if (action == GLFW_PRESS || action == GLFW_RELEASE) {
    thisWindowClass->mKeyInputMap[key] = action == GLFW_PRESS;
  }
}

void Window::_cursorPosCallback(GLFWwindow *window, double xpos, double ypos) {
  auto *thisWindowClass = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));

  static float lastX;
  static float lastY;
  static bool firstMouse = true;

  if (firstMouse) {
    lastX      = static_cast<float>(xpos);
    lastY      = static_cast<float>(ypos);
    firstMouse = false;
  }

  thisWindowClass->mouseDeltaX = static_cast<float>(xpos) - lastX;
  // inverted y axis
  thisWindowClass->mouseDeltaY = lastY - static_cast<float>(ypos);

  lastX = static_cast<float>(xpos);
  lastY = static_cast<float>(ypos);

  if (thisWindowClass->mouseCallbacks.empty()) {
    return;
  }

  for (auto &callback : thisWindowClass->mouseCallbacks) {
    callback(thisWindowClass->mouseDeltaX, thisWindowClass->mouseDeltaY);
  }
}

void Window::_frameBufferResizeCallback(GLFWwindow *window, int width, int height) {
  auto *thisWindowClass = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
  thisWindowClass->setWindowSizeChanged(true);
}
