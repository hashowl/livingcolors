const livingcolors = require('livingcolors');

const LC_COMMAND_HSV = 0x03;
const LC_COMMAND_ON = 0x05;
const LC_COMMAND_OFF = 0x07;

const LC_OFFSET_ACK = 0x01;

let pending = [false, false, false];
let pwr_changed = [false, false, false];

console.log("running initialization script...");

// registering callbacks
console.log("registering callbacks...");

onStop(function () {
    console.log("running onStop...");
    livingcolors.stop();
    console.log("onStop completed");
}, 3000);

on({ id: /^0_userdata\.0\.livingcolors\.\d\.on$/, change: "ne", ack: false }, function (obj) {
    let lamp = get_lamp(obj.id);
    pwr_changed[lamp] = true;
    set_pending(lamp);
});

on({ id: /^0_userdata\.0\.livingcolors\.\d\.(hue|saturation|value)$/, change: "ne", ack: false }, function (obj) {
    let lamp = get_lamp(obj.id);
    set_pending(lamp);
});

// livingcolors native setup
console.log("starting livingcolors bridge service...");

livingcolors.setup(
    function (msg) {
        console.log(msg);
    },
    function (sc) {
        ack(sc);
    }
);

console.log("initialization script completed");

function set_pending(lamp) {
    if (!pending[lamp]) {
        pending[lamp] = true;
        setTimeout(function () {
            cmd(lamp);
        }, 10);
    }
}

function get_lamp(state_id) {
    return parseInt(state_id.match(/(?<=^0_userdata\.0\.livingcolors\.)\d+(?=\.(?:on|hue|saturation|value)$)/)[0]);
}

function cmd(lamp) {
    pending[lamp] = false;
    let sc = { lamp: lamp };
    if (pwr_changed[lamp]) {
        pwr_changed[lamp] = false;
        let on = getState("0_userdata.0.livingcolors." + lamp + ".on").val;
        sc.command = on ? LC_COMMAND_ON : LC_COMMAND_OFF;
    }
    else {
        sc.command = LC_COMMAND_HSV;
    }
    sc.hue = Math.round(getState("0_userdata.0.livingcolors." + lamp + ".hue").val);
    sc.saturation = Math.round(getState("0_userdata.0.livingcolors." + lamp + ".saturation").val);
    sc.value = Math.round(getState("0_userdata.0.livingcolors." + lamp + ".value").val);
    livingcolors.cmd(sc);
}

function ack(sc) {
    let on;
    if (sc.command == LC_COMMAND_HSV + LC_OFFSET_ACK) {
        on = getState("0_userdata.0.livingcolors." + sc.lamp + ".on").val;
    }
    else {
        on = sc.command == LC_COMMAND_ON + LC_OFFSET_ACK;
    }
    setState("0_userdata.0.livingcolors." + sc.lamp + ".on", on, true);
    setState("0_userdata.0.livingcolors." + sc.lamp + ".hue", sc.hue, true);
    setState("0_userdata.0.livingcolors." + sc.lamp + ".saturation", sc.saturation, true);
    setState("0_userdata.0.livingcolors." + sc.lamp + ".value", sc.value, true);
}