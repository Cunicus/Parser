

enum class StatementType { VariableDeclaration, StructDeclaration, FunctionDeclaration, Conditional, SingleExpression, Block, Count };
enum class ExpressionType { BinaryExpression, UnaryExpression, IntConstant, FloatConstant, StringConstant, VariableSpecifier, FunctionCall, Cast, MemberAccess, Count };

#define Stat(name) struct name : Statement  { static constexpr StatementType type_index = StatementType::name;
#define Expr(name) struct name : Expression { static constexpr ExpressionType type_index = ExpressionType::name;

struct StructDeclaration;

enum class ActualTypeType { Void, Integer, Float, Struct };
struct ActualType { ActualTypeType type; byte size = 0; StructDeclaration* from = nullptr; };

struct Type {
    String name;
    ActualType* points_to = nullptr;
    int pointer_depth = 0;
};

struct Scope;

struct Statement  { StatementType  type; int line;             };
struct Expression { ExpressionType type; Type evaluation_type; };

enum class VariableScope { Global, FunctionLocal, ObjectLocal };

Stat(VariableDeclaration) 
    String name;        Type decl_type;
    Scope* scope;       Expression* init = nullptr;
    int offset;         VariableScope variable_scope = VariableScope::FunctionLocal;
};

Stat(StructDeclaration)
    String name; Scope* scope;
    Array<VariableDeclaration*> variable_declarations;
    ActualType actual_type; 
};

Stat(FunctionDeclaration)
    String name;        Type return_type;
    Scope* scope;       Statement* statement;
    
    Array<VariableDeclaration*> args;
    int byte_code_start, byte_code_end;
};


enum class SingleExpressionType { Return, SingleExpression };
Stat(SingleExpression)
    SingleExpressionType single_type;
    Expression* expression;
};

Stat(Block) Array<Statement*> statements; };

enum class ConditionalType { If, While };
Stat(Conditional)
    ConditionalType conditional_type;
    Expression* condition; Statement* statement;
};

Expr(BinaryExpression)
    int op; Expression *left, *right;
};

Expr(UnaryExpression) 
    int op; Expression* expression;
};

Expr(StringConstant) union {
    String string; int64 global_offset;
};};

Expr(IntConstant)   int   value; };
Expr(FloatConstant) float value; };

Expr(VariableSpecifier) union {
    String name; VariableDeclaration* declaration;
};};

Expr(FunctionCall) 
    // union { String name; FunctionDeclaration* declaration; }; 
    String name; FunctionDeclaration* declaration; 
    Array<Expression*> args;
};

Expr(Cast) Expression* from; };

Expr(MemberAccess) 
    Expression* object;
    String member_name;
    VariableDeclaration* member;
};

struct Scope {
    Array<VariableDeclaration*> variables;
    Array<FunctionDeclaration*> functions;

    Array<StructDeclaration*> structs;
    Array<VariableSpecifier*> variable_specifiers;
    Array<FunctionCall*     > function_calls;

    int stack_size = 0;
    Scope* parent = nullptr;
};

#undef Expr
#undef Stat

#define case_use_expression(type, name, expression) break; case ExpressionType::type: { auto name = (type*) expression;
#define case_use_statement(type, name, statement) break; case StatementType::type: { auto name = (type*) statement;

//Node storage automatically provides a constant memory address container for every node type
template<typename T> struct NodeStorage { inline static ConstAddress<T, 32> data; };
template<typename T> void allocate(T*& t) { t = NodeStorage<T>::data.alloc(); }
template<typename T> void free(T* t) { NodeStorage<T>::data.free(t); }

//allocates a enum tagged node
template<typename Type> Type* allocate() {
    Type* data; allocate(data); 
    data->type = Type::type_index; return data;
}

Scope* global_scope; Scope* current_scope;
Array<Statement*> global_statements;

void push_scope(Scope* scope) { scope->parent = current_scope; current_scope = scope; }
void pop_scope() { current_scope = current_scope->parent; }
