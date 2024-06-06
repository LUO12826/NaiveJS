# NaiveJS

NaiveJS is a personal experimental JavaScript engine. My primary goal is to learn the implementation of well-known features in JavaScript such as closures, event loops, objects, etc., rather than implementing the language strictly according to the ECMA specification. Having said that, I do hope to bring NaiveJS closer to the specification as time goes on.

### Update (May 2024)

NaiveJS can now run the Typescript 2.0.10 transpiler ([test_typescript_2.0.10.js](/test_files/test_typescript_2.0.10.js)) ! Although it is still not a rigorous ES5 implementation, it at least produces the same results as QuickJS produces in my tests. 

Running time comparison of running the Typescript transpiler (input is the same piece of Typescript code):

- Node.js v18.15.0 : 0.80 s
- QuickJS (CMake "Release" build) : 2.73 s
- NaiveJS (CMake "Release" build) : 3.35 s

This comparison is just for fun. If NaiveJS were implemented strictly according to the ECMA specification, it would be expected to run even slower.

### Language Feature Checklist

- [x] Lexer, parser, and AST (adapted from [this work](https://github.com/zhuzilin/es))
- [x] Static scope chain
- [x] Bytecode generator
- [x] Bytecode virtual machine
- [x] Basic math operators and relational operators
- [x] Logical operators (short-circuit evaluation)
- [x] Member access expression (dot)
- [x] Indexed access expression (subscript)
- [x] If statement
- [x] While loop, break, continue
- [x] For loop (regular, for-in, for-of)
- [x] Iterator
- [x] `JSValue` and basic JavaScript data types
- [x] Array literals, object literals
- [x] Closures and automatic variable capture
- [x] Copy-based GC
- [x] Event loop and `setInterval`, `setTimeout` (using kqueue)
- [x] Third-party libraries for HTTP requests
- [x] Runtime exception and error handling (try-catch-finally)
- [x] `new` operator
- [x] Prototype chain
- [x] Global scope (that works like real JS)
- [x] Object property attributes, Object.defineProperty
- [x] Regular expression
- [ ] Template literals
- [ ] Spread syntax
- [ ] Destructuring assignment

### Other To Do

- [ ] Make eventloop work on Linux (using epoll)
- [ ] Reorganize the parser code
- [ ] Compactly stored bytecode
- [ ] Eliminates redundant jump instructions
- [ ] Improve error handling in the parser and code generator
- [ ] Use smart pointers instead of raw pointers in AST nodes. (This was previously found to cause performance degradation on macOS with Clang)
- [ ] GC and object system optimization: allowing GC to move objects safely
- [ ] Atom strings GC
- [ ] fully managed memory: all memory is allocated on the GC Heap
- [ ] Utilizing the "label as value" technique in the VM
- [ ] JIT

### Acknowledgement

- [QuickJS](https://bellard.org/quickjs/)

  A great Javascript engine, small and fast. It supports the [ES2023](https://tc39.github.io/ecma262/2023) specification.

- [zhuzilin/es](https://github.com/zhuzilin/es)

  An ES5 engine written from scratch.

- [libregexp](https://github.com/bellard/quickjs/blob/master/libregexp.h)

  A regular expression library created by the QuickJS project.

- [martinus/robin-hood-hashing](https://github.com/martinus/robin-hood-hashing)

- [bshoshany/thread-pool](https://github.com/bshoshany/thread-pool)

- [yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib)
