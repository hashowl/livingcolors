const livingcolors = require('livingcolors');

function main() {

    console.log("running initialization script...");

    // registering callbacks
    console.log("registering callbacks...");
    onStop(function () {
        console.log("running onStop...");
        livingcolors.stop();
        console.log("onStop completed");
    }, 3000);

    // livingcolors native setup
    console.log("starting livingcolors bridge service...");
    livingcolors.setup(
        function (msg) {
            console.log(msg);
        },
        function (msg) {
            console.log(msg);
        }
    );

    console.log("initialization script completed");
}

main();