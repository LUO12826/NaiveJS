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

while (a < 3) {
  let str = obj.key.key.key.key
  console.log(str)
  a = a + 1
}