var BlockIntrinsics = (function () {
  function BlockIntrinsics() {
    this.prototype = undefined;
    this.toString = undefined;
    this.toLocaleString = undefined;
    this.valueOf = undefined;
    this.hasOwnProperty = undefined;
    this.propertyIsEnumerable = undefined;
    this.isPrototypeOf = undefined;
    this["constructor"] = undefined;
  }
  return BlockIntrinsics;
})();

var StringHashTable = (function () {
  function StringHashTable() {
    this.itemCount = 0;
    this.table = (new BlockIntrinsics());
  }
  StringHashTable.prototype.getAllKeys = function () {
    var result = [];
    for(var k in this.table) {
      if(this.table[k] != undefined) {
        result[result.length] = k;
      }
    }
    return result;
  };
  StringHashTable.prototype.add = function (key, data) {
    if(this.table[key] != undefined) {
      return false;
    }
    this.table[key] = data;
    this.itemCount++;
    return true;
  };
  StringHashTable.prototype.addOrUpdate = function (key, data) {
    if(this.table[key] != undefined) {
      this.table[key] = data;
      return false;
    }
    this.table[key] = data;
    this.itemCount++;
    return true;
  };
  StringHashTable.prototype.map = function (fn, context) {
    for(var k in this.table) {
      var data = this.table[k];
      if(data != undefined) {
        fn(k, this.table[k], context);
      }
    }
  };
  StringHashTable.prototype.every = function (fn, context) {
    for(var k in this.table) {
      var data = this.table[k];
      if(data != undefined) {
        if(!fn(k, this.table[k], context)) {
          return false;
        }
      }
    }
    return true;
  };
  StringHashTable.prototype.some = function (fn, context) {
    for(var k in this.table) {
      var data = this.table[k];
      if(data != undefined) {
        if(fn(k, this.table[k], context)) {
          return true;
        }
      }
    }
    return false;
  };
  StringHashTable.prototype.count = function () {
    return this.itemCount;
  };
  StringHashTable.prototype.lookup = function (key) {
    var data = this.table[key];
    if(data != undefined) {
      return data;
    } else {
      return (null);
    }
  };
  return StringHashTable;
})();

let tb = new StringHashTable
if (!tb.add("void", 2)) {
  console.log("test failed")
}
