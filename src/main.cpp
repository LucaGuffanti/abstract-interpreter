#include <fstream>
#include <sstream>

#include "parser.hpp"
#include "ast.hpp"
#include "interpreter.hpp" 

int main(int argc, char** argv) {
    if(argc != 2) {
        std::cout << "usage: " << argv[0] << " tests/00.c" << std::endl;
    }
    std::ifstream f(argv[1]);
    if (!f.is_open()){
        std::cerr << "[ERROR] cannot open the test file `" << argv[1] << "`." << std::endl;
        return 1;
    }
    std::ostringstream buffer;
    buffer << f.rdbuf();
    std::string input = buffer.str();
    f.close();

    AbstractInterpreter<int64_t> AI(input);
    // AI.print();
    std::cout << "Analyzing program `" << argv[1] << "`..." << std::endl;
    AI.run();
    std::cout << "respects all preconditions and postconditions." << std::endl;
    return 0;
}