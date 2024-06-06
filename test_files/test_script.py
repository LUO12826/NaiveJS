import os

BUILD_DIR = "build"

test_ignored = [
    "jquery-1.4.2.js",
    "temp_test.js",
    "test_typescript.js",
    "test_typescript_2.0.10.js",
    "test_typescript_3.8.3.js",
    "test_string_lit.js",
    "test_number_lit.js",
    "test_try_catch.js",
    "test_regexp.js",
    "test_array_exotic.js",
    "test_array.js",
    "test_for_in.js",
    "test_hashtable.js",
    "test_defineProperty.js",
]

file_and_expected = {

    "test_for_of.js": '''
[LOG] 0
[LOG] 1
[LOG] 2
[LOG] undefined
[LOG] 4
[LOG] test
''',

    "test_label.js": '''
[LOG] 1 
[LOG] 2 
[LOG] 3 
[LOG] 4 
[LOG] 5 
[LOG] 6 
[LOG] 7 
[LOG] 8 
[LOG] 9 
[LOG] 10 
[LOG] Inner loop: 
[LOG] 0 
[LOG] 0 
[LOG] Inner loop: 
[LOG] 0 
[LOG] 1 
[LOG] Inner loop: 
[LOG] 0 
[LOG] 2 
[LOG] Inner loop: 
[LOG] 1 
[LOG] 0 
[LOG] Inner loop: 
[LOG] 2 
[LOG] 0 
[LOG] Inner loop: 
[LOG] 2 
[LOG] 1 
[LOG] Inner loop: 
[LOG] 2 
[LOG] 2 
[LOG] Inner loop: 
[LOG] 0 
[LOG] 0 
[LOG] Inner loop: 
[LOG] 0 
[LOG] 1 
[LOG] Inner loop: 
[LOG] 0 
[LOG] 2 
[LOG] Inner loop: 
[LOG] 1 
[LOG] 0 
[LOG] hello 
[LOG] 0 
[LOG] 1 
[LOG] 2 
[LOG] 1 
[LOG] This is the start 
[LOG] This is another block 
[LOG] Program terminated 
''',

    "test_iterator.js": '''
[LOG] 0
[LOG] 1
[LOG] 2
''',

    "test_switch.js": '''
[LOG] 1
[LOG] ddd
[LOG] 111
[LOG] 222
[LOG] 1
''',

    "test_bool_expr.js": '''
[LOG] true true true true true true true true true true 
[LOG] false false false
''',

    "test_closure.js": '''
[LOG] 303 
[LOG] 3 
[LOG] clo var
''',

    "test_deep_object.js": '''
[LOG] this is a string 
[LOG] this is a string 
[LOG] this is a string 
''',
    "test_this_binding.js": '''
[LOG] [ { }, { }, ]
[LOG] { prop: 12, }
[LOG] { prop: 12, }
''',

    "test_bitwise.js":
'''
[LOG] test passed: 3 AND 1 should be 1 
[LOG] test passed: 4 AND 1 should be 0 
[LOG] test passed: 3 OR 1 should be 3 
[LOG] test passed: 4 OR 1 should be 5 
[LOG] test passed: 3 XOR 1 should be 2 
[LOG] test passed: 4 XOR 1 should be 5 
[LOG] test passed: NOT 3 should be -4 
[LOG] test passed: NOT -1 should be 0 
[LOG] test passed: 3 left shifted by 1 should be 6 
[LOG] test passed: 1 left shifted by 3 should be 8 
[LOG] test passed: 8 right shifted by 1 should be 4 
[LOG] test passed: -8 right shifted by 2 should be -2 
[LOG] test passed: 8 unsigned right shifted by 1 should be 4 
[LOG] test passed: -8 unsigned right shifted by 2 should be 1073741822
''',

    "test_fibonacci.js": [
        "6765"
    ],
    "test_if_stmt.js": [
        "a and b are true",
        "Either c is true, or both d and a are true",
        "a and b are true",
        "a is false"
    ],
    "test_object.js": [
        "aaaa",
        "3"
    ],
    "test_quicksort.js": [
        "[ 254, 255, 1320, 1398, ]",
    ],
    "test_timer.js": [
        "this will be canceled after 5 times",
        "this will be canceled after 5 times",
        "hello timeout",
        "this will be canceled after 5 times",
        "this will be canceled after 5 times",
        "this will be canceled after 5 times"
    ],
    "test_var.js": [
        "0",
        "100",
        "10000",
        "233",
        "undefined"
    ],
}

def check_test_files(directory, exec_path):
    num_not_match = 0

    for file in os.listdir(directory):
        if file in test_ignored:
            continue

        if not file.endswith(".js"):
            continue

        print(">>> running " + file)
        file_path = os.path.join(directory, file)

        cmd_str = f"{exec_path} -f {file_path}"
        output_lines = os.popen(cmd_str).readlines()

        if file in file_and_expected:
            if output_match(file_and_expected[file], output_lines):
                print("   ✅ match")
            else:
                num_not_match += 1
                print_red_text("   ❌ not match")

        else:
            print("   ⛔️ expected result not provided.")

    print()
    print(f"Number of files that do not match: {num_not_match}")
    if num_not_match == 0:
        print("*** All passed ***")
        # print("\033[1m" + "*** All passed ***" + "\033[0m")


def output_match(expected: str, output: list[str]):
    if isinstance(expected, str):
        expected = expected.split('\n')
        expected = [s.removeprefix("[LOG] ").strip() for s in expected if (not s.isspace() and s != "")]
    expected_line_cnt = len(expected)
    curr_line = 0
    for line in output:
        if not line.startswith("\x1b[0m\x1b[32m[LOG] "):
            continue

        line_stripped = line.removeprefix("\x1b[0m\x1b[32m[LOG] ").removesuffix("\n").strip()
        if line_stripped == expected[curr_line]:
            curr_line += 1
        else:
            return False

    return curr_line == expected_line_cnt


def print_red_text(text):
    print(text)


if __name__ == "__main__":
    current_directory = os.path.dirname(os.path.abspath(__file__))
    njs_exec_path = os.path.join(current_directory, f"../{BUILD_DIR}/njsmain")
    print("skipped:\n  " + "\n  ".join(test_ignored))
    print()
    check_test_files(current_directory, njs_exec_path)
