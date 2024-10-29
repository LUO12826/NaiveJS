let dateString = "2024-05-25";

let regex = /(\d{4})-(\d{2})-(\d{2})/g;

let match = regex.exec(dateString);

if (match) {
  console.log("Year: " + match[1]);
  console.log("Month: " + match[2]);
  console.log("Day: " + match[3]);
} else {
  console.log("No match found.");
}

console.log(regex.test(dateString))

const logString = "Error: 2024-05-31 14:23:45 - System failure";
regex = /(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2}) (?<hour>\d{2}):(?<minute>\d{2}):(?<second>\d{2})/;

match = logString.match(regex);

if (match) {
    const year = match.groups.year;
    const month = match.groups.month;
    const day = match.groups.day;
    const hour = match.groups.hour;
    const minute = match.groups.minute;
    const second = match.groups.second;

    console.log("year: " + year + ", month: " + month + ", day: " + day);
    console.log("time: " + hour + ":" + minute + ":" + second);
}
