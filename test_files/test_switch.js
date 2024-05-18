let a = 5

switch (a) {
  default:
    function test() {
      console.log(1)
    }
    test()
    console.log("ddd")
  case 1:
    console.log("111")
  case 2:
    console.log("222")
    break
  case 2 + 2:
    console.log("222")
}

test()