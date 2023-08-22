import os

BUILD_DIR = "build"

file_and_expected = {
    "test_array.js": '''
[LOG] { a: 233, b: 466, } 
[LOG] { c: 855, d: 939, } 
[LOG] [ 1, 2, 3, 4, 5, ] 
[LOG] 13367816628 
[LOG] string in an array 
[LOG] undefined 
[LOG] null 
[LOG] 233 
[LOG] 1 
[LOG] { c: 5, d: 6, b: 'this is a string', }
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
        if file in ["jquery-1.4.2.js", "temp_test.js"]:
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
    njs_exec_path = os.path.join(current_directory, f"../{BUILD_DIR}/njs")
    check_test_files(current_directory, njs_exec_path)
