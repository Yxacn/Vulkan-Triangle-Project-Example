// main.cpp
#include <iostream>

#include "GLWindow.hpp"


int main()
{
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif

    try
    {
        vkp::GLWindow window;
        window.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    std::cin.get();
}