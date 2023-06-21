#include <iostream>
#include <fstream>
#include <string>
#include <codecvt>
#include <cstdio>
#include <chrono>

// #define DEBUG

#include <njs/parser/lexer.h>
#include <njs/parser/token.h>
#include <njs/parser/parser.h>
#include "njs/utils/helper.h"

using namespace njs;
using std::string;
using std::u16string;


std::u16string read_file(const std::string& file_path) {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::ifstream::failure("Cannot open the file: " + file_path);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return converter.from_bytes(content);
}

int main(int argc, char *argv[]) {

  string file_path = "/Users/luohuizhou/Desktop/js-test-file.js";

  try {
    u16string source_code = read_file(file_path);
    auto startTime = std::chrono::steady_clock::now();

    // Lexer lexer(source_code);
    // Token token = Token::none;

    // while (token.type != EOS) {
    //   token = lexer.next();

    //   printf("%s\n", token.to_string().c_str());

    //   if (token.type == ILLEGAL) {
    //     printf("illegal token encountered at line %u.\n", token.line + 1);
    //     break;
    //   }
    // }

    Parser parser(source_code);
    ASTNode* ast = parser.parse_program();

    // ast->print_tree(0);

    if (ast->is_illegal()) {
    	std::cout << "illegal program at: " << test::to_utf8_string(ast->source_ref())
      << ", line: " << ast->get_line_start() + 1 << ", start: " << ast->start_pos()
      << ", end: " << ast->end_pos() << std::endl;
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    std::cout << "parsed in " << duration.count() << " ms." << std::endl;
  }
  catch (const std::ifstream::failure& e) {
    fprintf(stderr, "%s\n", e.what());
  }
}