
function test() {
  try {

    try {
      let a = 10
      throw 2
    } catch (error) {
      throw "test_inner_catch_throw"
    }

    let a = 10
    throw 2
  } catch (error) {
    console.log(error)
    throw "test_catch_throw"
  }

  try {
    throw 2
  } catch (error) {
    throw 344
  }
}

function outer() {
  try {
    test()
  }
  catch (e) {
    console.log(e)
    throw e
  }
}


try {
  let itv = setInterval(() => {
    clearInterval(itv)
    throw 4
  }, 2000)
  outer()
} catch (error) {
  throw 54
}

try {
  throw "fafafa"
} catch (error) {
  console.log(error)
}

// expected:
// [LOG] test_inner_catch_throw
// [LOG] test_catch_throw
// Unhandled throw: 54
// Unhandled throw: 4