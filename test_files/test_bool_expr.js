const expr1 = true && (false || true) && !(false && true);
const expr2 = !(true && false) || (true || false) && !(false && true);
const expr3 = !(false || (true && (false || true)));

const expr4 = true || false && false && true || false;
const expr5 = true && true || false && true || false && false;

const expr6 = ((true || false) && (true && (false || true))) || false;
const expr7 = (true && (true && (false || (true && false)))) || true;

const a = true;
const b = false;
function foo() {
  return true;
}
function bar() {
  return false;
}
const expr12 = foo() || bar();
const expr13 = a && foo() && (b || bar());

const obj = {
  prop1: true,
  prop2: false,
  prop3: true,
  prop4: false
};
const expr14 = obj.prop1 && (obj.prop2 || obj.prop3);
const expr15 = obj.prop1 || obj.prop2 && (obj.prop3 || obj.prop4);

const expr16 = !((false && !(true || false)) || (true && (false || true)));
const expr17 = (true || (false && true) && !(true || (false && true))) && !(true && false);



// log(expr1, expr2, expr3, expr4, expr5, expr6, expr7, expr12, expr13, expr14, expr15, expr16, expr17)
console.log(expr1, expr2, expr3, expr4, expr5, expr6, expr7, expr12, expr13, expr14, expr15, expr16, expr17)
