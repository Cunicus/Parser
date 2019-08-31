//-1 int float var call ()
//0  as
//1 -unary &ref *deref ! ~
//2 * / %  << >> &
//3 + - |
//4 < > == (!= >= <=)
//5 &&
//6 || =

//Print Ast to console
//TODO more readable print and support for special operators like '[]'

void print_expression(Expression* exp) {
    switch(exp->type) {
        case_use_expression(BinaryExpression, node, exp) 
            std::cout << '('; print_expression(node->left); print_op(node->op); print_expression(node->right); std::cout << ')'; 
        }

        case_use_expression(UnaryExpression, node, exp)
            print_op(node->op); print_expression(node->expression);
        }

        case_use_expression(Cast, node, exp) 
            std::cout << node->evaluation_type.name; print_expression(node->from);
        }

        case_use_expression(IntConstant,       node, exp) std::cout << node->value; }
        case_use_expression(FloatConstant,     node, exp) std::cout << node->value; }
        case_use_expression(VariableSpecifier, node, exp) std::cout << node->name; }
        case_use_expression(FunctionCall,      node, exp) 
            std::cout << node->declaration->name << '(';
            for(auto arg : node->args) { print_expression(arg); std::cout << ','; }
            std::cout << ')';
        }
    }
}

void print_statement(Statement* stat, int indent) {
    std::cout << std::string(indent, ' ');
    switch(stat->type) {
        case_use_statement(FunctionDeclaration, node, stat) 
            std::cout << node->name << ' ' <<  node->return_type.name << '(';
            for(auto arg : node->args) print_statement(arg, 0);
            std::cout << ")\n";
            if(node->statement) print_statement(node->statement, indent + 2);
        }

        case_use_statement(VariableDeclaration, node, stat) 
            std::cout << node->name << ' ' << node->decl_type.name << '=';
            if(node->init) print_expression(node->init);
        }

        case_use_statement(Block, node, stat) 
            std::cout << "{\n"; 
            for(auto stat: node->statements) {
                print_statement(stat, indent + 2);
                std::cout << '\n'; 
            }
            std::cout << std::string(indent, ' ');
            std::cout << '}';
        }

        case_use_statement(SingleExpression, node, stat) 
            print_expression(node->expression);
        }

        case_use_statement(Conditional, node, stat)
            print_expression(node->condition); std::cout << '\n';
            print_statement(node->statement, indent + 2);
        }
    }
}

void print_ast() {
    std::cout << "\nAst:\n"; 
    for(auto stat : global_statements) { print_statement(stat, 0); std::cout << '\n'; }
}

//parses a type or pointer to type
Type parse_type() {
    Type type; type.name = get_current_token_string();
    while(match_operator('*')) ++type.pointer_depth;
    return type;
}

Expression* parse_expression();
Expression* parse_expression(int precedence) {
    //parse atoms
    if(precedence == -1) {
        if(current_token.type == Token::Int) {
            auto exp = allocate<IntConstant>();
            exp->value = get_current_token_int();
            return exp;
        }

        if(current_token.type == Token::Float) {
            auto exp = allocate<FloatConstant>();
            exp->value = get_current_token_float();
            return exp;
        }

        if(current_token.type == Token::Text) {
            auto exp = allocate<StringConstant>();
            exp->string = get_current_token_text();
            return exp;
        }

        if(current_token.type == Token::Identifier) {
            //parse function call
            if(peek_token.type == '(') {
                auto exp = allocate<FunctionCall>();
                exp->name = get_current_token_string();
                get_next_token();
                if(current_token.type != ')') while(true) {
                    exp->args.push_back(parse_expression());
                    if(current_token.type == ')') break;
                    expect(',');
                }
                get_next_token();
                current_scope->function_calls.push_back(exp);
                return exp;
            }

            //parse variable specifier
            auto exp = allocate<VariableSpecifier>();
            exp->name = get_current_token_string();
            current_scope->variable_specifiers.push_back(exp);
            return exp;
        }

        //parse (expr)
        expect('(');
        auto* exp = parse_expression();
        expect(')');
        return exp;
    }

    if(operator_types[precedence] == OperatorType::Prefix) {

        if(current_token.type == Token::Operator) for(int op : operators[precedence]) {
            if(current_token.op != op) continue;
            auto* exp = allocate<UnaryExpression>();
            exp->op = op; get_next_token();
            exp->expression = parse_expression(precedence);
            return exp;
        }

        return parse_expression(precedence - 1);
    }

    if(operator_types[precedence] == OperatorType::Postfix) {
        auto expr = parse_expression(precedence - 1);
        if(current_token.type == Token::Operator) for(int op : operators[precedence]) {
            if(current_token.op != op) continue;
            
            //special case cast
            if(op == combine("as")) {
                get_next_token();
                auto exp = allocate<Cast>();
                exp->from = expr;
                exp->evaluation_type = parse_type();
                return exp;
            }
        }

        //special case .
        while(match('.')) {
            auto mem = allocate<MemberAccess>();
            mem->object = expr;
            mem->member_name = get_current_token_string();
            expr = mem;
        }

        //special case '[]'
        while(match('[')) {
            auto array = allocate<BinaryExpression>();
            array->op = combine("[]");
            array->left = expr;
            array->right = parse_expression();
            expect(']');
            expr = array;
        }

        return expr;
    }

    //parse binary operators
    Expression* left = parse_expression(precedence - 1);

    redo:
    if(current_token.type == Token::Operator)
        for(int op : operators[precedence]) {
            if(current_token.op != op) continue;

            auto exp = allocate<BinaryExpression>();
            exp->op = op; get_next_token();
            exp->left = left;
            exp->right = parse_expression(precedence - 1);
            left = exp;
            goto redo;
        }

    return left;
}

Expression* parse_expression() { return parse_expression(operators.size() - 1); }


Statement* parse_statement();

VariableDeclaration* parse_variable_declaration() {
    auto var = allocate<VariableDeclaration>();
    var->name = get_current_token_string();
    var->decl_type = parse_type();

    if(match_operator('=')) var->init = parse_expression();

    current_scope->variables.push_back(var);
    return var;
}

StructDeclaration* parse_struct_declaration() {
    auto str = allocate<StructDeclaration>();
    str->name = get_current_token_string();
    str->actual_type.from = str;
    expect_identifier("struct"); expect('{');
    
    allocate(str->scope); push_scope(str->scope);
    while(!match('}')) {
        str->variable_declarations.push_back(parse_variable_declaration());
        str->variable_declarations.back()->variable_scope = VariableScope::ObjectLocal;
        match(','); match(';');
    }
    pop_scope();

    current_scope->structs.push_back(str);

    return str;

}

FunctionDeclaration* parse_function_declaration() {
    auto func = allocate<FunctionDeclaration>();
    func->name = get_current_token_string();

    current_scope->functions.push_back(func);
    allocate(func->scope); push_scope(func->scope);

    expect('('); if(!match(')')) while(true) {
        func->args.push_back(parse_variable_declaration());
        if(match(')')) break;
        expect(',');
    }

    func->return_type = parse_type();

    if(match_identifier("builtin")) func->statement = nullptr;
    else func->statement = parse_statement();

    pop_scope();
    return func;
}

Statement* parse_statement_impl() {
        
    if(current_token.type == Token::Identifier) {
        if(current_token.string == "return") {
            get_current_token_string();
            auto exp = allocate<SingleExpression>();
            exp->single_type = SingleExpressionType::Return;
            exp->expression = parse_expression();
            expect(';'); return exp;
        }

        if(current_token.string == "if" || current_token.string == "while") {
            auto con = allocate<Conditional>();
            con->conditional_type = get_current_token_string() == "if" ? ConditionalType::If : ConditionalType::While;
            expect('('); con->condition = parse_expression(); expect(')');
            con->statement = parse_statement();
            return con;
        }

        if(peek_token.type == Token::Identifier) {
            if(peek_token.string == "struct") return parse_struct_declaration();

            auto decl = parse_variable_declaration(); expect(';');
            return decl;
        }

        if(peek_token.type == '(') {
            //!! how to differ from single expression function call
            return parse_function_declaration();
        }
    }

    if(match('{')) {
        auto block = allocate<Block>();
        while(!match('}')) block->statements.push_back(parse_statement());
        // std::cout << "\n blocksize: " << block->statements.size();
        return block;
    }

    auto exp = allocate<SingleExpression>();
    exp->single_type = SingleExpressionType::SingleExpression;
    exp->expression = parse_expression();
    expect(';'); return exp;
}

Statement* parse_statement() {
    Statement* stat = parse_statement_impl();
    stat->line = current_line;
    return stat;
}


void resolve_symbol(Scope& scope, VariableSpecifier& specifier) {
    for(auto decl : scope.variables) {
        if(!(decl->name == specifier.name)) continue;
        specifier.declaration = decl;
        return;
    }

    resolve_symbol(*global_scope, specifier);
}

void resolve_symbol(Scope& scope, FunctionCall& call) {
    for(auto func : scope.functions) {
        if(!(func->name == call.name)) continue;
        call.declaration = func;
        return;
    }

    if(scope.parent == nullptr) fatal_error("call to unknown funktion: %.*s", call.name.size(), call.name.start);
    resolve_symbol(*scope.parent, call);
}

void resolve_symbols() {
    for(Scope& scope : NodeStorage<Scope>::data) {
        for(auto var : scope.variable_specifiers) resolve_symbol(scope, *var);
        for(auto fun : scope.function_calls     ) resolve_symbol(scope, *fun);
    }
}

ActualType builtin_void  {ActualTypeType::Void   , 0};
ActualType builtin_byte  {ActualTypeType::Integer, 1};
ActualType builtin_short {ActualTypeType::Integer, 2};
ActualType builtin_int   {ActualTypeType::Integer, 4};
ActualType builtin_int64 {ActualTypeType::Integer, 8};
ActualType builtin_float {ActualTypeType::Float  , 4};

void evaluates_non_ptr(Expression* e, ActualType& type) {
    e->evaluation_type.pointer_depth = 0;
    e->evaluation_type.points_to = &type;
}


void evaluates_pointer    (Expression* exp, const Type& to) { exp->evaluation_type = to; exp->evaluation_type.pointer_depth++; }
void evaluates_dereference(Expression* exp, const Type& to) { exp->evaluation_type = to; exp->evaluation_type.pointer_depth--; }

void resolve_struct_type(Scope& scope, Type& type) {
    
    for(auto str : scope.structs) {
        if(!(type.name == str->name)) continue;
        type.points_to = &str->actual_type;
        return;
    }

    if(!scope.parent) fatal_error("tried to reolve unknown type: %.*s", type.name.end - type.name.start, type.name.start);
    resolve_struct_type(*scope.parent, type);
}

bool resolve_builtin_type(Type& type) {
    if(type.name ==  "void") { type.points_to = &builtin_void ; return true; }
    if(type.name ==  "byte") { type.points_to = &builtin_byte ; return true; }
    if(type.name == "int16") { type.points_to = &builtin_short; return true; }
    if(type.name == "int32") { type.points_to = &builtin_int  ; return true; }
    if(type.name ==   "int") { type.points_to = &builtin_int  ; return true; }
    if(type.name == "int64") { type.points_to = &builtin_int64; return true; }
    if(type.name == "float") { type.points_to = &builtin_float; return true; }
    return false;
}

void resolve_type(Scope& scope, Type& type) {
    
    if(resolve_builtin_type(type)) return;
    
    // for(auto str : global_scope->structs) {
    //     if(!(type.name == str->name)) continue;
    //      type.points_to = &str->actual_type;
    //     return;
    // }

    if(!type.points_to) resolve_struct_type(scope, type);

}

void evaluate_type(Expression* exp);

void evaluate_type(UnaryExpression* unary) {
    if(unary->evaluation_type.points_to) return;
    evaluate_type(unary->expression);
    switch(unary->op) {
        bcase '&': evaluates_pointer    (unary, unary->expression->evaluation_type);
        bcase '*': evaluates_dereference(unary, unary->expression->evaluation_type);
        bcase '!': evaluates_non_ptr    (unary, builtin_int);
        bcase '-': unary->evaluation_type = unary->expression->evaluation_type;
        break; default: fatal_error("evaluate type for unknown unary operator: %d", unary->op);
    };
}

void evaluate_type(BinaryExpression* binary) {
    if(binary->evaluation_type.points_to) return;
    evaluate_type(binary->left); evaluate_type(binary->right);
    switch(binary->op) {
        bcase '=': evaluates_non_ptr(binary, builtin_void);
        
        bcase '+': case '-': case '*': case '/':
            binary->evaluation_type = binary->left->evaluation_type;
            if(binary->left->evaluation_type.pointer_depth == 0 && binary->right->evaluation_type.pointer_depth == 0  && binary->left->evaluation_type.points_to != binary->right->evaluation_type.points_to) 
                fatal_error("Type mismatch on both sides of an operator");
        
        bcase '%': case '&': case '|': case combine("=="): case '<': case '>': 
            evaluates_non_ptr(binary, builtin_int);
        
        bcase combine("[]"): evaluates_dereference(binary, binary->left->evaluation_type);
        break; default: fatal_error("evaluate type for unknown binary operator: %d", binary->op);
    }
}


void evaluate_type(MemberAccess* mem) {
    if(mem->evaluation_type.points_to) return;
    evaluate_type(mem->object);
    for(auto var : mem->object->evaluation_type.points_to->from->variable_declarations) {
        if(!(var->name == mem->member_name)) continue;
        mem->member = var; mem->evaluation_type = var->decl_type;
        return;
    }
}

void evaluate_type(Expression* exp) { switch(exp->type) {
        bcase ExpressionType::UnaryExpression : evaluate_type((UnaryExpression *)exp);
        bcase ExpressionType::BinaryExpression: evaluate_type((BinaryExpression*)exp);
        bcase ExpressionType::MemberAccess    : evaluate_type((MemberAccess    *)exp);
        break; default: if(!exp->evaluation_type.points_to) 
            fatal_error("Expression type should already be evaluated: %d", (int)exp->type);
}}

void evaluate_types() {
    //probably need to go through each scope, since struct types are scope dependent
    for(auto& scope : NodeStorage<Scope>::data) {
        for(auto var : scope.variables) resolve_type(scope, var->decl_type);
        for(auto var : scope.functions) resolve_type(scope, var->return_type);
    }

    // for(auto& decl : NodeStorage<VariableDeclaration>::data) resolve_type(decl.decl_type);
    // for(auto& decl : NodeStorage<FunctionDeclaration>::data) resolve_type(decl.return_type);

    for(auto& node : NodeStorage<IntConstant        >::data) evaluates_non_ptr(&node, builtin_int  ); 
    for(auto& node : NodeStorage<FloatConstant      >::data) evaluates_non_ptr(&node, builtin_float);
    for(auto& node : NodeStorage<StringConstant     >::data) evaluates_non_ptr(&node, builtin_int64);
    for(auto& node : NodeStorage<VariableSpecifier  >::data) node.evaluation_type = node.declaration->decl_type;
    for(auto& node : NodeStorage<FunctionCall       >::data) node.evaluation_type = node.declaration->return_type;
    
    //TODO support scoped types for cast not only builtin types
    for(auto& node : NodeStorage<Cast               >::data) resolve_builtin_type(node.evaluation_type);

    for(auto& node : NodeStorage<UnaryExpression    >::data) evaluate_type(&node);
    for(auto& node : NodeStorage<BinaryExpression   >::data) evaluate_type(&node);
    for(auto& node : NodeStorage<MemberAccess       >::data) evaluate_type(&node);
}

byte size(Type& type) {
    if(type.pointer_depth) return 8;
    return type.points_to->size;
}

byte size(Expression* exp) { return size(exp->evaluation_type); }

void construct_struct_memory(StructDeclaration* decl) {
    if(!decl || decl->actual_type.size != 0) return;
    for(auto var : decl->variable_declarations) {
        construct_struct_memory(var->decl_type.points_to->from);
        var->offset = decl->actual_type.size;
        decl->actual_type.size += size(var->decl_type);
    }
}

std::vector<String> string_literals;
void construct_memory() {

    for(auto& decl : NodeStorage<StructDeclaration>::data) 
        construct_struct_memory(&decl);

    for(auto& global : NodeStorage<StringConstant>::data) {
        int64 pos = global_scope->stack_size;
        global_scope->stack_size += global.string.size() + 1;
        string_literals.push_back(global.string);
        global.global_offset = pos;
    }

    for(auto& scope : NodeStorage<Scope>::data)
        for(auto var : scope.variables) {
            var->offset = scope.stack_size;
            scope.stack_size += size(var->decl_type);
        }

    for(auto var : global_scope->variables) 
        var->variable_scope = VariableScope::Global;
}

void parse(const char* file_name) {
    std::string file = load_file(file_name);
    input_file = file.c_str();

    get_next_token(); get_next_token();

    allocate(global_scope);
    current_scope = global_scope;

    while(current_token.type != Token::Eof) 
        global_statements.push_back(parse_statement());
    
    resolve_symbols();
    evaluate_types();
    construct_memory();
}