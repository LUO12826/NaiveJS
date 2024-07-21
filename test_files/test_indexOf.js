const paragraph = "I think Ruth's dog is cuter than your dog!";
const searchTerm = 'dog';
console.log(paragraph.lastIndexOf(searchTerm)); // 38

console.log("canal".lastIndexOf("a")); // return 3
console.log("canal".lastIndexOf("a", 2)); // return 1
console.log("canal".lastIndexOf("a", 0)); // return -1
console.log("canal".lastIndexOf("x")); // return -1
console.log("canal".lastIndexOf("c", -5)); // return 0
console.log("canal".lastIndexOf("c", 0)); // return 0
console.log("canal".lastIndexOf("")); // return 5
console.log("canal".lastIndexOf("", 2)); // return 2

console.log("hello world".indexOf("")); // return 0
console.log("hello world".indexOf("", 0)); // return 0
console.log("hello world".indexOf("", 3)); // return 3
console.log("hello world".indexOf("", 8)); // return 8

console.log("hello world".indexOf("", 11)); // return 11
console.log("hello world".indexOf("", 13)); // return 11
console.log("hello world".indexOf("", 22)); // return 11


console.log("Blue Whale".indexOf("Blue")); // return  0
console.log("Blue Whale".indexOf("Blute")); // return -1
console.log("Blue Whale".indexOf("Whale", 0)); // return  5
console.log("Blue Whale".indexOf("Whale", 5)); // return  5
console.log("Blue Whale".indexOf("Whale", 7)); // return -1
console.log("Blue Whale".indexOf("")); // return  0
console.log("Blue Whale".indexOf("", 9)); // return  9
console.log("Blue Whale".indexOf("", 10)); // return 10
console.log("Blue Whale".indexOf("", 11)); // return 10