function testResult(actual, expected, message) {
    if (actual === expected) {
        console.log("test passed: " + message);
    } else {
        console.log("test failed: " + message + ". expected: " + expected + ", actual: " + actual);
    }
}

// and
testResult(3 & 1, 1, "3 AND 1 should be 1");
testResult(4 & 1, 0, "4 AND 1 should be 0");

// or
testResult(3 | 1, 3, "3 OR 1 should be 3");
testResult(4 | 1, 5, "4 OR 1 should be 5");

// xor
testResult(3 ^ 1, 2, "3 XOR 1 should be 2");
testResult(4 ^ 1, 5, "4 XOR 1 should be 5");

// not
testResult(~3, -4, "NOT 3 should be -4");
testResult(~-1, 0, "NOT -1 should be 0");

// left shift
testResult(3 << 1, 6, "3 left shifted by 1 should be 6");
testResult(1 << 3, 8, "1 left shifted by 3 should be 8");

// right shift
testResult(8 >> 1, 4, "8 right shifted by 1 should be 4");
testResult(-8 >> 2, -2, "-8 right shifted by 2 should be -2");

// unsigned right shift
testResult(8 >>> 1, 4, "8 unsigned right shifted by 1 should be 4");
testResult(-8 >>> 2, 1073741822, "-8 unsigned right shifted by 2 should be 1073741822");