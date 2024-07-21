
let pr = new Promise((res, rej) => {
    rej("`rej` message")
})


let prom = new Promise((res, rej) => {
    res(pr)
})

prom.then((d) => {
    console.log("res", d)
}, (e) => {
    console.log("err", e)
})

prom.finally(()=> {
    console.log("finally")
}).then(() => {
    console.log("finally then")
})

prom.then((d)=> {
    console.log("then", d)
}).then(() => {
    console.log("then then")
})