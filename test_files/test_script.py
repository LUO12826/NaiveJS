import os

file_and_expected = {
    "test_array.js": [
        "{ Atom(13): 233, Atom(10): 466, }",
        "{ Atom(14): 855, Atom(15): 939, }",
        "Array[ 1, 2, 3, 4, 5, ] props: { Atom(0): 5, }",
        "1.33678e+10",
        "string in an array",
        "undefined",
        "null",
        "233",
        "1",
        "{ Atom(14): 5, Atom(15): 6, Atom(10): this is a string, }"
    ],
    "test_bool_expr.js": [
        "true true true true true true true true true true",
        "false false false"
    ],
    "test_closure.js": [
        "303",
        "3",
        "clo var"
    ],
    "test_deep_object.js": [
        "this is a string",
        "this is a string",
        "this is a string"
    ],
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
        "Array[ 254, 255, 1320, 1398, ] props: { Atom(0): 4, }",
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


def check_test_files(directory):
    num_not_match = 0

    for file in os.listdir(directory):
        if file in ["jquery-1.4.2.js", "temp_test.js"]:
            continue

        if not file.endswith(".js"):
            continue

        file_path = os.path.join(directory, file)

        cmd_str = f"./cmake-build-debug/njs -f {file_path}"
        output_lines = os.popen(cmd_str).readlines()

        if file in file_and_expected:
            if output_match(file_and_expected[file], output_lines):
                print(file + " match")
            else:
                num_not_match += 1
                print_red_text("!!!" + file + " not match")

        else:
            print(file + ": expected result not provided.")

    print()
    print(f"Number of files that do not match: {num_not_match}")


def output_match(expected: list[str], output: list[str]):
    expected_line_cnt = len(expected)
    curr_line = 0
    for line in output:
        if not line.startswith("\x1b[0m\x1b[32m[LOG] "):
            continue

        if line.removeprefix("\x1b[0m\x1b[32m[LOG] ").removesuffix("\n").strip() == expected[curr_line]:
            curr_line += 1
        else:
            return False

    return curr_line == expected_line_cnt


def print_red_text(text):
    print(text)


if __name__ == "__main__":
    current_directory = os.path.dirname(os.path.abspath(__file__))
    check_test_files(current_directory)
