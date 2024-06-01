function Constructor() {}

Object.defineProperty(Constructor.prototype, 'myProp', {
  writable: false,
  value: 'aaa'
});

const obj = new Constructor();

obj.myProp = 10

console.log(obj.myProp); // "value"

Object.defineProperty(obj, 'myProp', {
  writable: false,
  value: 'new value'
});
console.log(obj.myProp); // "new value"

obj.myProp = 'another value';
console.log(obj.myProp); // "new value"，because it's not writable

console.log(Constructor.prototype.myProp); // "value"

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