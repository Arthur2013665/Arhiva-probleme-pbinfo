#include "script.h"

#include <iostream>
#include <map>
#include <string>

int main(int argc, char* argv[]) {
    ArchiveContext ctx;
    scriptInitArchive(ctx);

    std::map<std::string, ScriptHandler> routes;
    scriptRegisterAllCommands(routes);

    if (argc > 1) {
        std::string line;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) line += ' ';
            line += argv[i];
        }
        scriptHandleInput(ctx, line, routes);
        return 0;
    }

    std::cout << "Bine ai venit la arhiva problemelor rezolvate de pe pbinfo\n";
    std::cout << "Folder arhiva: " << ctx.pbinfoMain.string() << '\n';
    std::cout << "Probleme gasite: " << ctx.problemIds.size() << '\n';
    std::cout << "Id numeric = afiseaza solutia | 99 = lista | 0 = iesire\n";
    std::cout << "Comenzi extinse: --help | --help-modules | ex: --analyze-complexity 1\n\n";

    while (true) {
        std::cout << "> ";
        std::string input;
        if (!std::getline(std::cin, input)) break;
        if (input.empty()) continue;

        if (scriptHandleInput(ctx, input, routes)) continue;

        int choice = 0;
        try {
            choice = std::stoi(input);
        } catch (...) {
            std::cout << "Introdu un id, 99, 0, sau o comanda --help\n";
            continue;
        }

        if (choice == 0) {
            std::cout << "La revedere!\n";
            break;
        }
        if (choice == 99) {
            scriptPrintAvailableProblems(ctx, false);
            continue;
        }
        if (choice < 0) {
            std::cout << "Id invalid.\n";
            continue;
        }

        ctx.activeProblemId = choice;
        ctx.problemTimerStart = std::chrono::steady_clock::now();
        ctx.problemTimerRunning = true;
        scriptShowSolution(ctx, choice);
    }

    return 0;
}
