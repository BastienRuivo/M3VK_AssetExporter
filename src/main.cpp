
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

    if(argv[1] == std::string("-h") || argv[1] == std::string("--help"))
    {
        std::cout << "Assert export -- " << std::endl;
        std::cout << " -h --help : Display this help" << std::endl;
        std::cout << "-a --asset <IMPORT_PATH0> <EXPORT_PATH0> <IMPORT_PATH1> <EXPORT_PATH1> ... Export assets" << std::endl;
        /*std::cout << "-m --material <IMPORT_PATH0> <EXPORT_PATH0> <IMPORT_PATH1> <EXPORT_PATH1> ... Export materials" << std::endl;
        std::cout << "-t --texture <IMPORT_PATH0> <EXPORT_PATH0> <IMPORT_PATH1> <EXPORT_PATH1> ... Export textures" << std::endl;
        std::cout << "-c --cubemap <IMPORT_PATH0> <EXPORT_PATH0> <IMPORT_PATH1> <EXPORT_PATH1> ... Export cubemaps" << std::endl;*/
        return EXIT_SUCCESS;
    }

    if(argc % 2 != 0)
    {
        std::cerr << "Format is -{TYPE} <IMPORT_PATH0> <EXPORT_PATH0> <IMPORT_PATH1> <EXPORT_PATH1> ..." << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        for(int i = 2; i < argc; i+=2)
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
