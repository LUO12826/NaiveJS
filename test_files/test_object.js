let obj = {
    a: 1,
    b: {
        c: 2,
        d: {
            str: "this is a string",
            num: add(2, 3)
        }
    }
}

function add(a, b) {
    return a + b
}

obj.b.d.num = "aaaa"
log(obj.b.d.num)

obj.b.d.str = 3
log(obj.b.d.str)