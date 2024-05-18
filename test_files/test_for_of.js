let arr = [0, 1, 2, , 4, "test"]

for (let i of arr) {
  console.log(i)
}


// [LOG] 0
// [LOG] 1
// [LOG] 2
// [LOG] undefined
// [LOG] 4
// [LOG] test