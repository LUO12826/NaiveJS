
function test() {
  try {
    let a = 10
    throw 2
  } catch (error) {
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
  outer()
} catch (error) {
  throw 54
}

try {
  throw "fafafa"
} catch (error) {
  console.log(error)
}
