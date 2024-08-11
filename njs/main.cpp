#include <codecvt>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/resource.h>

#include "njs/utils/Timer.h"
#include "njs/common/Defer.h"
#include "njs/global_var.h"
#include "njs/parser/Lexer.h"
#include "njs/parser/Parser.h"
#include "njs/codegen/CodegenVisitor.h"
#include "njs/vm/NjsVM.h"

using namespace njs;
using std::string;
using std::u16string;

void set_stack_size(size_t size_mb);
u16string read_file(const string & path);
void print_tokens(u16string& source_code);
void read_options(int argc, char *argv[]);

static string file_path = "../test_files/temp_test.js";
static bool show_ast = false;
static bool show_tokens = false;

int main(int argc, char *argv[]) {
  set_stack_size(48);
  read_options(argc, argv);

  try {
    u16string source_code = read_file(file_path);

    // show tokens
    if (show_tokens) {
      print_tokens(source_code);
    }

    Timer parser_timer("parsed");

    Parser parser(std::move(source_code));
    ASTNode *ast = parser.parse_program();
    defer { delete ast; };

    parser_timer.end();

    if (parser.get_errors().size() > 0) {
      printf("Njs: terminated due to parsing errors in program.\n");
      exit(EXIT_FAILURE);
    }

    // show AST
    if (show_ast) ast->print_tree(0);

    if (ast->is_illegal()) {
      std::cout << "illegal program at: " << to_u8string(ast->get_source())
                << ", line: " << ast->source_start().line
                << ", col: " << ast->source_start().col
                << '\n';
      return 1;
    }

    // codegen
    Timer codegen_timer("code generated");
    CodegenVisitor visitor;
    visitor.codegen(static_cast<ProgramOrFunctionBody *>(ast));
    codegen_timer.end();

    if (visitor.get_errors().size() > 0) {
      printf("Njs: terminated due to codegen errors in program.\n");
      exit(EXIT_FAILURE);
    }

    // execute bytecode
    Timer exec_timer("executed");
    NjsVM vm(visitor);
    vm.setup();
    vm.run();
    exec_timer.end();
    
  }
  catch (const std::ifstream::failure &e) {
    fprintf(stderr, "%s\n", e.what());
  }
}

void read_options(int argc, char *argv[]) {
  int option;
  while ((option = getopt(argc, argv, "bgativlos:f:")) != -1) {
    switch (option) {
      case 'b':
        Global::show_codegen_result = true;
        break;
      case 'g':
        Global::show_gc_statistics = true;
        break;
      case 'v':
        Global::show_vm_exec_steps = true;
        break;
      case 'i':
        Global::show_vm_stats = true;
        break;
      case 'a':
        show_ast = true;
        break;
      case 't':
        show_tokens = true;
        break;
      case 'l':
        Global::show_log_buffer = true;
        break;
      case 'o':
        Global::enable_optimization = true;
        break;
      case 's':
        set_stack_size(atoi(optarg));
        break;
      case 'f':
        file_path = string(optarg);
        break;
      case '?':
        std::cerr << "Unknown option: " << static_cast<char>(optopt) << '\n';
        break;
      default:
        break;
    }
  }
}

void set_stack_size(size_t size_mb) {
  const rlim_t stack_size = size_mb * 1024 * 1024;
  struct rlimit rl;
  int result;

  result = getrlimit(RLIMIT_STACK, &rl);
  if (result == 0) {
    if (rl.rlim_cur < stack_size) {
      rl.rlim_cur = stack_size;
      result = setrlimit(RLIMIT_STACK, &rl);
      if (result != 0) {
        fprintf(stderr, "setrlimit return error\n");
      }
    }
  }
}

void print_tokens(u16string& source_code) {
  Lexer lexer(source_code);
  Token token = Token::none;

  while (token.type != TokenType::EOS) {
    token = lexer.next();
    printf("%s\n", token.to_string().c_str());

    if (token.type == TokenType::ILLEGAL) {
      printf("illegal token encountered at line %u.\n", token.line);
      break;
    }
  }
}

u16string read_file(const string & path) {
  std::ifstream file(path);
  if (!file.is_open()) { throw std::ifstream::failure("Cannot open the file: " + path); }

  string content(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
  return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().from_bytes(content);
}