let arr  =[
    {
        a: 233,
        b: 466
    },
    {
        c: 855,
        d: 939
    },
    [1, 2, 3, 4, 5],
    13367816628,
]

console.log(arr[2][0])
console.log(arr[0].a)

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