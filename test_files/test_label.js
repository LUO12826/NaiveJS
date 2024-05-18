{
  let i = 0
  outer:
      while (i < 10) {
        i += 1
        console.log(i)
        for (let i in [1, 2, 3]) {
          for (let j in [1, 2, 3]) {
            for (let k in [1, 2, 3]) {
              if (k === 2) continue outer
            }
          }
        }
      }
}

{
  outer:
      for (let i = 0; i < 3; i++) {
        for (let j = 0; j < 3; j++) {
          if (i === 1 && j === 1) continue outer;
          console.log("Inner loop:");
          console.log(i);
          console.log(j);
        }
      }
}

{
  outer:
      for (let i = 0; i < 3; i++) {
        for (let j = 0; j < 3; j++) {
          if (i === 1 && j === 1) break outer;
          console.log("Inner loop:");
          console.log(i);
          console.log(j);
        }
      }
}

// useless labels
{
  foo: if (true) console.log('hello');
  bar: for (let i = 0; i < 3; i++) console.log(i);
  baz: true ? console.log(1) : console.log(2);
}

{
  start: {
    console.log('This is the start');
    anotherBlock: {
      console.log('This is another block');
      break anotherBlock;
    }

    for (let i of [1, 2]) {
      if (i === 2) break start;
    }
    console.log('This will not be reached');
  }
  console.log('Program terminated');
}
// [LOG] This is the start
// [LOG] This is another block
// [LOG] Program terminated


// should not print anything
{
  for (let i of [1, 2, 3]) {
    start : {
      break
    }
    console.log(i)
  }
}