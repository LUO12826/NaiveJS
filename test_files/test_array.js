let arr  = [
    {
        a: 233,
        b: 466
    },
    {
        c: 855,
        d: 939
    },
    [1, 2, 3, 4, 5],
    12345678,
    "string in an array",
    undefined,
    null
]

let len = arr.length
let cnt = 0
while (cnt < len) {
    console.log(arr[cnt])
    cnt += 1
}

console.log(arr[0].a)
console.log(arr[2][0])


arr[0] = {
    c: 5,
    d: 6
}

function test_obj(obj) {
    obj.b = "this is a string"
    function inner() {
        return obj
    }
    return inner
}

let func = test_obj(arr[0])
console.log(func())