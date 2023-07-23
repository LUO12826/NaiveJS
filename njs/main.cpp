#include <chrono>
#include <codecvt>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

#include "njs/utils/Timer.h"
#include "njs/global_var.h"
#include "njs/parser/lexer.h"
#include "njs/parser/parser.h"
#include "njs/codegen/CodegenVisitor.h"
#include "njs/vm/NjsVM.h"

using namespace njs;
using std::string;
using std::u16string;

std::u16string read_file(const std::string &file_path);
void print_tokens(u16string& source_code);

int main(int argc, char *argv[]) {

  string file_path = "../test_files/temp_test.js";
  bool show_ast = false;
  bool show_tokens = false;

  int option;
  while ((option = getopt(argc, argv, "bgatf:")) != -1) {
    switch (option) {
    case 'b':
      Global::dump_bytecode = true;
      break;
    case 'g':
      Global::show_gc_statistics = true;
      break;
    case 'a':
      show_ast = true;
      break;
    case 't':
      show_tokens = true;
      break;
    case 'f':
      file_path = string(optarg);
      break;
    case '?':
      std::cerr << "Unknown option: " << static_cast<char>(optopt) << std::endl;
      break;
    }
  }

  try {
    u16string source_code = read_file(file_path);

    // show tokens
    if (show_tokens) {
      print_tokens(source_code);
    }

    Timer parser_timer("parsed");

    Parser parser(std::move(source_code));
    ASTNode *ast = parser.parse_program();

    parser_timer.end();

    // show AST
    if (show_ast) ast->print_tree(0);

    if (ast->is_illegal()) {
      std::cout << "illegal program at: " << to_utf8_string(ast->get_source())
                << ", line: " << ast->get_line_start() + 1 << ", start: " << ast->start_pos()
                << ", end: " << ast->end_pos() << std::endl;
      return 1;
    }

    // codegen
    Timer codegen_timer("code generated");
    CodegenVisitor visitor;
    visitor.codegen(static_cast<ProgramOrFunctionBody *>(ast));
    codegen_timer.end();

    // execute bytecode
    Timer exec_timer("executed");
    NjsVM vm(visitor);
    vm.add_native_func_impl(u"log", InternalFunctions::log);
    vm.add_native_func_impl(u"$gc", InternalFunctions::js_gc);
    vm.add_builtin_object(u"console", [] (GCHeap& heap, StringPool& str_pool) {
      JSObject *obj = heap.new_object<JSObject>();

      JSFunctionMeta log_meta {
          .is_anonymous = true,
          .is_native = true,
          .param_count = 0,
          .local_var_count = 0,
          .native_func = InternalFunctions::log,
      };
      JSFunction *log_func = heap.new_object<JSFunction>(log_meta);

      JSValue key(JSValue::JS_ATOM);
      key.val.as_int64 = str_pool.add_string(u"log");

      obj->add_prop(key, JSValue(log_func));
      return obj;
    });
    vm.run();

    exec_timer.end();
    
  }
  catch (const std::ifstream::failure &e) {
    fprintf(stderr, "%s\n", e.what());
  }
}

void print_tokens(u16string& source_code) {
  Lexer lexer(source_code);
  Token token = Token::none;

  while (token.type != TokenType::EOS) {
    token = lexer.next();

    printf("%s\n", token.to_string().c_str());

    if (token.type == TokenType::ILLEGAL) {
      printf("illegal token encountered at line %u.\n", token.line + 1);
      break;
    }
  }
}

std::u16string read_file(const std::string &file_path) {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;

  std::ifstream file(file_path);
  if (!file.is_open()) { throw std::ifstream::failure("Cannot open the file: " + file_path); }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return converter.from_bytes(content);
}