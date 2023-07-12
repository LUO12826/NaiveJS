function inc(clo_var) {

    clo_var = 10
    function inner1() {

        clo_var = 1
        function inner2(b) {
            return clo_var + b
        }
        return inner2
    }
    return inner1
}



let res = inc(1)()(300)

log(res)