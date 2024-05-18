function complexIfStatement(a, b, c, d) {
  let result = '';

  if (a && b) {
    result = 'a and b are true';
  } else if (c || (d && a)) {
    result = 'Either c is true, or both d and a are true';
  } else if ((a && c) || (b && d)) {
    result = 'Either a and c are true, or b and d are true';
  } else {
    if (a) {
      if (b) {
        if (c) {
          if (d) {
            result = 'a, b, c, and d are all true';
          } else {
            result = 'a, b, and c are true, but d is false';
          }
        } else {
          result = 'a and b are true, but c is false';
        }
      } else {
        result = 'a is true, but b is false';
      }
    } else {
      result = 'a is false';
    }
  }

  return result;
}

console.log(complexIfStatement(true, true, false, false));
console.log(complexIfStatement(true, false, false, true));
console.log(complexIfStatement(true, true, true, true));
console.log(complexIfStatement(false, false, false, false));
