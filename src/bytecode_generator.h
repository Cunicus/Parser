
template<typename T> void emit(T t) { byte_code.push(t); };

template<typename T>
void emit_constant(T c) { byte_code.push<byte>(Opt::Constant); byte_code.push<byte>(sizeof(T)); byte_code.push(c); }
void emit_rel_addr   (int64 add) { byte_code.push<byte>(Opt::RelativeAddress); byte_code.push(add); }
void emit_global_addr(int64 add) { byte_code.push<byte>(Opt::GlobalAddress);   byte_code.push(add); }

void emit_load(int size) { byte_code.push<byte>(Opt::Load); byte_code.push<byte>(size); }
void emit_save(int size) { byte_code.push<byte>(Opt::Save); byte_code.push<byte>(size); }

int64 emit_address() { emit_constant<int64>(-1); return byte_code.index - sizeof(int64); }
void  emit_grow(int size) { emit_constant<int>(size); byte_code.push<byte>(Opt::Grow); }

struct FunctionRef { FunctionDeclaration* decl; int64 address; };
Array<FunctionRef> function_refs;
Array<int> return_refs;


#define case(type) case_use_expression(type, node, exp)

void generate_expression(Expression* exp, bool as_address = false) {
    switch(exp->type) {
        case(Cast) 
            generate_expression(node->from);
            if(node->evaluation_type.points_to == &builtin_float) 
                emit(Opt::IntToFloat);
            else if(node->evaluation_type.points_to == &builtin_int && node->from->evaluation_type.points_to == &builtin_float)
                emit(Opt::FloatToInt);
            else {
                emit(Opt::IntToInt);
                emit(size(node->from));
                emit(size(node));
            }
        }

        case(MemberAccess)
            if(node->object->evaluation_type.pointer_depth == 1)
                generate_expression(node->object, false);
            else
                generate_expression(node->object,  true);
            emit_constant<int64>(node->member->offset);
            emit(Opt::AddI); emit<byte>(8);
            if(!as_address) emit_load(size(node->member->decl_type));
        }

        case(StringConstant ) emit_global_addr(node->global_offset); }
        case(IntConstant    ) emit_constant<int>(node->value); }
        case(FloatConstant  ) emit_constant<float>(node->value); }
        case(UnaryExpression) switch(node->op) { 
            bcase '-':
                generate_expression(node->expression);
                if(node->evaluation_type.points_to == &builtin_float) byte_code.push<byte>(Opt::NegateF);
                else {
                    emit(Opt::NegateI);
                    emit(size(node));
                }
            bcase '*': 
                generate_expression(node->expression);
                if(!as_address) emit_load(size(node));
            bcase '&': generate_expression(node->expression, true);
        }}
         
        case(BinaryExpression) 
            if(node->op == '=') {
                generate_expression(node->right);
                generate_expression(node->left, true);
                emit_save(size(node->left)); 
                return;
            }

            if(node->op == combine("[]")) {
                generate_expression(node->left);
                generate_expression(node->right);
                
                //TODO maybe make part of implicit type conversion
                if(size(node->right) != 8) {
                    emit(Opt::IntToInt);
                    emit(size(node->right));
                    emit<byte>(8);
                }
                
                emit_constant<int64>(node->left->evaluation_type.points_to->size);
                emit(Opt::MulI); emit<byte>(8);
                emit(Opt::AddI); emit<byte>(8);
                if(!as_address) emit_load(size(node));
                return;
            }

            generate_expression(node->left);
            generate_expression(node->right);

            if(node->evaluation_type.pointer_depth || node->evaluation_type.points_to == &builtin_int) {
                switch(node->op) {
                    bcase '+': emit(Opt::AddI);  
                    bcase '*': emit(Opt::MulI); 
                    bcase '-': emit(Opt::SubI); 
                    bcase '/': emit(Opt::DivI); 
                    bcase '%': emit(Opt::ModI); 
                    bcase '<': emit(Opt::Less); 
                    bcase '>': emit(Opt::Greater);

                    bcase combine("=="): emit(Opt::Equal); 
                    bcase combine("!="): emit(Opt::Nonequal); 
                    bcase combine("<="): emit(Opt::LessEqual); 
                    bcase combine(">="): emit(Opt::GreaterEqual); 
                }
                byte_code.push<byte>(size(node));
            }

            if(node->evaluation_type.points_to == &builtin_float) switch(node->op) {
                bcase '+': emit(Opt::AddF);
                bcase '*': emit(Opt::MulF);
                bcase '-': emit(Opt::SubF);
                bcase '/': emit(Opt::DivF);
                bcase '<': emit(Opt::LessF);
                bcase '>': emit(Opt::GreaterF);
                bcase combine("=="): emit(Opt::EqualF);
                bcase combine("!="): emit(Opt::NonequalF);
                bcase combine("<="): emit(Opt::LessEqualF);
                bcase combine(">="): emit(Opt::GreaterEqualF);
            }
        }

        case(VariableSpecifier)
            if(node->declaration->variable_scope == VariableScope::FunctionLocal) emit_rel_addr   (node->declaration->offset);
            if(node->declaration->variable_scope == VariableScope::Global       ) emit_global_addr(node->declaration->offset);
            if(node->declaration->variable_scope == VariableScope::ObjectLocal  ) fatal_error("TODO");
            if(!as_address) emit_load(size(node->declaration->decl_type));
        }

        case(FunctionCall)
            for(auto i = node->args.rbegin(); i != node->args.rend(); ++i) generate_expression(*i);
            if(node->declaration->statement == nullptr) {
                if(node->declaration->name == "allocate") { emit(Opt::Allocate); return; }
                if(node->declaration->name ==     "free") { emit(Opt::Free    ); return; }
                if(node->declaration->name ==    "print") { emit(Opt::Print   ); return; }
                fatal_error("unknown builtin function: \" %.*s \", on line %d", node->declaration->name.size(), node->declaration->name.start, node->declaration->line);
            } else {
                function_refs.push_back({node->declaration, emit_address()});
                byte_code.push<byte>(Opt::Call);
            }
        }
    }
}

#define case(type) case_use_statement(type, node, statement)
void generate_statement(Statement* statement) {
    switch(statement->type) {
        case(VariableDeclaration)
            if(!node->init) return; 
            generate_expression(node->init);
            if(node->variable_scope == VariableScope::FunctionLocal) emit_rel_addr   (node->offset);
            if(node->variable_scope == VariableScope::Global       ) emit_global_addr(node->offset);
            if(node->variable_scope == VariableScope::ObjectLocal) fatal_error("TODO");
            
            emit_save(size(node->decl_type));
        }

        case(FunctionDeclaration) }

        //this will fill up the stack for now (bad :C) , returns!!
        case(SingleExpression) 
            generate_expression(node->expression);
            String name =  node->expression->evaluation_type.name;
            if(node->single_type == SingleExpressionType::Return) {
                
                return_refs.push_back(emit_address());
                emit(Opt::Jump);
            } else {
                emit(Opt::Ignore);
                emit(size(node->expression));
            }
        }
        case(Block) for(auto s : node->statements) generate_statement(s); }
        case(Conditional)
            int64 start = byte_code.index;
            int64 end_pos = emit_address();
            generate_expression(node->condition);
            emit(Opt::JumpZero);
            generate_statement(node->statement);
            if(node->conditional_type == ConditionalType::While) {
                emit_constant(start); emit(Opt::Jump);
            }
            byte_code.as<int64>(end_pos) = byte_code.index;
        }
    }
}

void generate() {
    
    
    byte_code.memory.reserve(1000);
    for(auto s : global_statements) 
        generate_statement(s);
    
    int program_end = byte_code.index;
    emit(Opt::Halt);

    for(int ref : return_refs) byte_code.as<int64>(ref) = program_end;
    return_refs.clear();
    
    for(auto& node : NodeStorage<FunctionDeclaration>::data) {
        if(node.statement == nullptr) continue;
        node.byte_code_start = byte_code.index;
        emit_grow(node.scope->stack_size);
        for(auto decl : node.args) { 
            emit_rel_addr(decl->offset);
            emit_save(size(decl->decl_type));
        }
        generate_statement(node.statement);
        node.byte_code_end = byte_code.index;
        emit(Opt::Return);

        for(int ref : return_refs) byte_code.as<int64>(ref) = node.byte_code_end;
        return_refs.clear();
    }

    for(auto ref : function_refs) byte_code.as<int64>(ref.address) = ref.decl->byte_code_start;

}