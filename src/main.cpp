// main.cpp
#include <cstdlib>
#include <exception>
#include <iostream>

#include "Application.hpp"

int main()
{
    try
    {
        vkp::Application app;
        app.run();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
