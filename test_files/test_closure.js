function inc(param) {
  let clo_var = "clo var"

  function inner1() {

    param = 1

    function inner2(b) {
      param += 2
      return param + b
    }

    return inner2
  }

  function inner3() {
    console.log(param)
    return [clo_var]
  }

  return [inner1, inner3]
}


let res = inc(10)
let inner1 = res[0]
let inner3 = res[1]

console.log(inner1()(300))
console.log(inner3()[0])