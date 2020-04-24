const livingcolors = require('livingcolors');

console.log("starting livingcolors bridge service...");

livingcolors.setup(
    function (msg) {
        console.log(msg);
    },
    function (msg) {
        console.log(msg);
    }
);

console.log("initialization script compelted");
