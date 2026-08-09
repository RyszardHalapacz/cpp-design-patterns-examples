#include "patterns/app/Application.hpp"
#include <filesystem>

int main(int argc, char* argv[]) {
    const std::filesystem::path exeDir =
        std::filesystem::weakly_canonical(std::filesystem::path(argv[0])).parent_path();
    (void)argc;

    patterns::app::Application app(exeDir);
    app.configure();
    app.run();
    return 0;
}
