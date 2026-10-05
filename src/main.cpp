// Entry point. The game is assembled in Core/Application; see CLAUDE.md for the layout.
#include "Core/Application.h"
#include "Core/LaunchOptions.h"

int main(int argc, char** argv) {
    Application application(LaunchOptions::Parse(argc, argv));
    return application.Run();
}
