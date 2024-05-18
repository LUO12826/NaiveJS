let arr = [0, 1, 2, , 4]

arr["a"] = 1
arr["b"] = 2
arr[1.234] = 3
arr[Symbol.iterator] = undefined

for (let i in arr) {
    console.log(i)
}


// [LOG] 0
// [LOG] 1
// [LOG] 2
// [LOG] 4
// [LOG] a
// [LOG] b
// [LOG] 1.234