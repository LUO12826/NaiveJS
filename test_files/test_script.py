import os

def call_njs_for_js_files(directory):
    for file in os.listdir(directory):
        if file == "jquery-1.4.2.js" or file == "temp_test.js":
            continue
        if file.endswith(".js"):
            file_path = os.path.join(directory, file)
            print(file_path)
            os.system(f"./build/njs -f {file_path}")

if __name__ == "__main__":
    current_directory = os.path.dirname(os.path.abspath(__file__))
    call_njs_for_js_files(current_directory)
