let array = [
    {
        arr: [
            function() {
                console.log(this)
            },
            function() {
                this.prop = 12
            }
        ],
        func: function() {
            console.log(new this.arr[1])
        }
    }
]

array[0].arr[0]()
array[0].func()
console.log(new array[0].arr[1]())

// expected:
// [LOG] [ { }, { }, ]
// [LOG] { prop: 12, }
// [LOG] { prop: 12, }