let obj = {}
obj[Symbol.iterator] = () => {
  let i = 0;
  return {
    next: () => {
      if (i === 3) {
        return {
          done: true,
          value: i
        }
      }
      return {
        done: false,
        value: i++
      }
    }
  }
}

for (let o of obj) {
  console.log(o)
}