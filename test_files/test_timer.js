setTimeout(() => {
    console.log("hello timeout")
}, 1000)

let id1 = setTimeout(() => {
    console.log("this will be canceled")
}, 2000)

clearTimeout(id1)

let counter = 0

let id2 = setInterval(() => {
    counter += 1
    console.log("this will be canceled after 5 times")
    if (counter === 5) clearInterval(id2)
}, 2000)