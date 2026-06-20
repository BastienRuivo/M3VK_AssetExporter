
#include <iostream>
#include <cstdlib>

#include "application/Application.h"
#include "application/DebugLayer.h"

int main(int argc, char* argv[])
{
    Application application;

    if(argc < 2)
    {
        std::cerr << "No model path provided" << std::endl;
        return EXIT_FAILURE;
    }

    if(argc % 2 != 1)
    {
        std::cerr << "Format is IMPORT_PATH1 EXPORT_PATH1 IMPORT_PATH2 EXPORT_PATH2 ..." << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        for(int i = 1; i < argc; i+=2)
        {
            DebugLayer::Log(DebugLayer::LogType::INFO, "Loading from: " + std::string(argv[i]) + " to: " + std::string(argv[i + 1]));
            application.ExportAsset(argv[i], argv[i + 1]);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}
