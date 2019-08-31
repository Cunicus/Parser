constexpr bool print_debug_information = false;

#include "include.h"
#include "lexer.h"
#include "parser_data.h"
#include "parser.h"
#include "stack_interpreter.h"
#include "bytecode_generator.h"

void add_precadance(const Array<int>& ops, OperatorType typ) {
    operators.push_back(ops); operator_types.push_back(typ);
}

void add_operators() {
    add_precadance({'-', '&', '*', '!', '~'}, OperatorType::Prefix);
    add_precadance({combine("as")}, OperatorType::Postfix);
    add_precadance({'*', '/', '%', '&', combine("<<"), combine(">>")}, OperatorType::Infix);
    add_precadance({'+', '-', '|'}, OperatorType::Infix);
    add_precadance({combine("<="), combine(">="), '<', '>', combine("=="), combine("!=")}, OperatorType::Infix);
    add_precadance({combine("&&")}, OperatorType::Infix);
    add_precadance({combine("||"), '='}, OperatorType::Infix);
}

int main() {

    add_operators();
    
    if(print_debug_information) print_lexer_test("test.ae");
    
    parse("test.ae");
    
    if(print_debug_information) {
        std::cout << "\n\n";
        print_ast();
    }
    
    generate();
    
    std::cout << "Result:\n" << run() << '\n';

    return 0;
}

