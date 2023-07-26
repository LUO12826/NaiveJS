
let a = 0

let obj = {
    key: {
        key: {
            key: {
                key: "this is a string"
            }
        }
    }
}

while (a < 2000) {
    let str = obj.key.key.key.key
    // console.log()
    a = a + 1
}