
//combines two chars into one int, for easier compare
constexpr int combine(char a, char b) { return (a    << 8) | b;    }
constexpr int combine(const char* a)  { return (a[0] << 8) | a[1]; }

std::pair<char, char> split(int op) { return {(char)(op >> 8), (char)(op & 0xFF)}; }
void print_op(int op) { std::cout << (char)(op >> 8) << (char)(op & 0xFF); }

//operators for every precadence, except special operators like cast and array access
enum class OperatorType { Prefix, Postfix, Infix };
Array<OperatorType> operator_types;
Array<Array<int>> operators;

//simple string view and comparison operators
struct String {
    const char *start, *end;
    int64 size() { return (int64)(end - start); }   
};

std::ostream& operator<<(std::ostream& stream, String s) { stream.write(s.start, s.end - s.start); return stream; }

bool operator==(String str, const char* literal) {
    while(str.start != str.end && *literal && *str.start++ == *literal++);
    return str.start == str.end && *literal == '\0';
}

bool operator!=(String str, const char* literal) { return !(str == literal); }

bool operator==(String left, String right) {
    while(left.start != left.end && right.start != right.end && *left.start == *right.start)
        left.start++, right.start++;
    return left.start == left.end && right.start == right.end;
}
bool operator!=(String left, String right) { return !(left == right); }

///////////
struct Token {
    int line;
    enum Type { ASCII = 127, Identifier, Text, Operator, Int, Float, Eof } type;
    union { int op; String string; int64 int_value; float float_value; };
};

///////////
const char* input_file;
int current_line = 1;

//parses the next token from the input file
void parse_token(Token& token) {
    //ignore whitespaces
    while(iswspace(*input_file)) if(*input_file++ == '\n') current_line++;

    //end of input file
    if(*input_file == '\0') { token.type = token.Eof; return; }
    
    //ignore comment lines
    if(input_file[0] == '/' && input_file[1] == '/') {
        while(*input_file++ != '\n'); parse_token(token); current_line++; return;
    } 
    
    token.line = current_line;

    //process int / float literals
    if(isdigit(*input_file)) {
        int64 int_val = *input_file - '0';
        while(isdigit(*++input_file)) (int_val *= 10) += (*input_file - '0');

        if(*input_file == '.') {
            input_file++;
            float factor = 1, float_val = int_val;
            while(isdigit(*input_file)) float_val += (factor *= 0.1) * (*input_file++ - '0');
            token.type = Token::Float;
            token.float_value = float_val;
        } else {
            token.type = Token::Int;
            token.int_value = int_val;
        }

        return;
    }

    //process operators
    for(auto& c : operators) for(int op : c) {
            //process two char operators
            if(op > Token::ASCII) {
                if(op == combine(input_file[0], input_file[1])) {
                    input_file += 2;
                    token.op = op;
                    token.type = Token::Operator; 
                    return;
                }
            //process single char operator
            } else if(op == *input_file) {
                token.op = *input_file++;
                token.type = Token::Operator;
                return;
            }
        }

    //process identifier
    if(isalpha(*input_file)) {
        token.type = Token::Identifier;
        token.string.start = input_file;
        while(isalpha(*++input_file) || isdigit(*input_file));
        token.string.end = input_file;
        return;
    }

    //TODO add escape codes
    //process string literal
    if(*input_file == '"') {
        token.type = Token::Text;
        token.string.start = ++input_file;
        while((*input_file++) != '"');
        token.string.end = input_file - 1;
        return;
    }

    //else process as char
    token.type = (Token::Type)*input_file++;
}


//parser can look at 2 tokens at a time
Token current_token, peek_token;
Token::Type get_next_token() {
    current_token = peek_token;
    parse_token(peek_token);
    return current_token.type;
}

//helper functions for parsing

[[ noreturn ]] void parser_error(const char* fmt, ...) {
    std::cout << "Parsing error on line: " << current_line << '\n';
    va_list args; va_start(args, fmt);
    vprintf(fmt, args); va_end(args);
    exit(-1);
}


bool match(int type) {
    if(current_token.type != type) return false;
    get_next_token(); return true;
}

bool match_operator(int op) {
    if(current_token.type == Token::Operator && current_token.op == op) {
        get_next_token(); return true;
    } return false;
}

bool match_identifier(const char* str) {
    if(current_token.type == Token::Identifier && current_token.string == str) {
        get_next_token(); return true;
    } return false;
}

void assert_type(int type) {
    if(current_token.type != type) parser_error("expected token %d, but got %d", type, current_token.type);
}

void expect(int type) { assert_type(type); get_next_token(); }
void expect_identifier(const char* id) 
{ assert_type(Token::Identifier); if(!(current_token.string == id)) parser_error("expected different identifier");  get_next_token(); }

int get_next_token_int() 
{ expect(Token::Int); return current_token.int_value; }

int get_current_token_int() 
{ assert_type(Token::Int       ); int   i = current_token.int_value;   get_next_token(); return i; }

float get_current_token_float() 
{ assert_type(Token::Float     ); float f = current_token.float_value; get_next_token(); return f; }

String get_current_token_string()
{ assert_type(Token::Identifier); String s = current_token.string;     get_next_token(); return s; }

String get_current_token_text()
{ assert_type(Token::Text      ); String s = current_token.string;     get_next_token(); return s; }


//print tokens of input stream
void print_lexer_test(const char* file_path) {
    std::string file = load_file(file_path);
    input_file = file.c_str();
    Token token;
    while(true) { parse_token(token); switch(token.type) {
        bcase Token::Int       : std::cout << "[Int: "   << token.int_value   << ']';
        bcase Token::Float     : std::cout << "[Float: " << token.float_value << ']';
        bcase Token::Operator  : std::cout << "[Operator: "; print_op(token.op); std::cout << ']';
        bcase Token::Identifier: std::cout << "[ID: ";     std::cout.write(token.string.start, token.string.end - token.string.start); std::cout << ']';
        bcase Token::Text      : std::cout << "[String: "; std::cout.write(token.string.start, token.string.end - token.string.start); std::cout << ']';
        bcase Token::Eof       : return;
        break; default         : std::cout << '[' << (char)token.type << ']';
    }}
}

