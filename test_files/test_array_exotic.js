// array with a hole
let arr = [0, 1, 2, , 4]
console.log(arr)

// resize to make it longer (new elements are `undefined`)
arr.length = 10
console.log(arr)

// will not print the index of the hole, but will print the index of the new elements
for (let e of arr) {
  console.log(e)
}

// resize to make it shorter
arr.length = 4
console.log(arr)

//  will not print the index of the removed elements
for (let e of arr) {
  console.log(e)
}

// this will create many holes
arr[10] = 10
console.log(arr)

// will not print the index of the holes
for (let e of arr) {
  console.log(e)
}

// these two work the same
console.log(arr.length)
console.log(arr["length"])

// both can work
arr.length = 9
arr.length = "8"
arr.length = Object("7")
arr.length = Object(6)
console.log(arr.length)

// these are not allowed
// arr.length = "aaa"
// arr.length = 8.5
// arr.length = "8.5"

console.log(arr.__proto__)