# NaiveJS

NaiveJS is a personal experimental JavaScript engine. My primary goal is to learn the implementation of well-known features in JavaScript such as closures, event loops, objects, etc., rather than implementing the language strictly according to the ECMA specification. Having said that, I do hope to bring NaiveJS closer to the specification as time goes on.

### Language Feature Checklist

- [x] Lexer, parser, and AST (adapted from [this work](https://github.com/zhuzilin/es))
- [x] Static scope chain
- [x] Bytecode generator
- [x] Bytecode virtual machine
- [x] Basic math operators and relational operators
- [x] Logical operators (short-circuit evaluation)
- [x] Member access expression (dot)
- [x] Indexed access expression (subscript)
- [x] While loop, break, continue
- [x] If statement
- [x] `JSValue` and basic JavaScript data types
- [x] Array literals, object literals
- [x] Closures and automatic variable capture
- [x] Copy-based GC
- [x] Event loop and `setInterval`, `setTimeout` (using kqueue)
- [x] Third-party libraries for HTTP requests
- [x] Runtime exception and error handling (try-catch)
- [ ] `finally` block
- [ ] Global scope (that works correctly)
- [ ] For loop
- [ ] Regular expression
- [ ] `new` operator
- [ ] Prototype chain
- [ ] Object property attributes

### Other To Do

- [ ] Reorganize the parser code
- [ ] Compactly stored bytecode
- [ ] Eliminates redundant jump instructions
- [ ] Improve error handling in the parser and code generator
- [ ] Use smart pointers instead of raw pointers in AST nodes. (This was previously found to cause performance degradation on macOS with Clang)
- [ ] GC and object system optimization: allowing GC to move objects safely
- [ ] fully managed memory: all memory is allocated on the GC Heap
- [ ] Utilizing the "label as value" technique in the VM

### Acknowledgement

- [zhuzilin/es](https://github.com/zhuzilin/es)
- [martinus/robin-hood-hashing](https://github.com/martinus/robin-hood-hashing)
- [bshoshany/thread-pool](https://github.com/bshoshany/thread-pool)
- [yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib)
