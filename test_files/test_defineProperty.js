let o = {}

Object.defineProperty(o, "p", {
  // value: 233,
  enumerable: true,
  // writable: true,
  configurable: false,
  get() {
    return 100
  },
  set(v) {
    this.aaa = 1000
  }
})

Object.defineProperty(o, "p", {
  value: 1,
})

console.log(o)
console.log(o.p)
o.p = 2
console.log(o.p)
console.log(o.aaa)
console.log(o.hasOwnProperty("aaa"))