const Clay = require('pebble-clay');
const clayConfig = require('./config.json');
const weather = require('./weather');
const modifications = require('./modifications');
var clay = new Clay(clayConfig, modifications, { autoHandleEvents: false });

const messageKeys = require('message_keys');
const messageKeysLookup = Object.fromEntries(Object.entries(messageKeys).map(([k, v]) => [v, k]));

const dbmsg = obj =>

clay.registerComponent(require('./drawing'));
clay.registerComponent(require('./test'));

Pebble.addEventListener('showConfiguration', function(e) {
    Pebble.openURL(clay.generateUrl());
});

const sendAppMessage = function(msg) {
    if (msg === null) {
        console.error("Coding error: trying to send a null message.");
        return;
    }
    try {
        Pebble.sendAppMessage(msg, function(e) {
            const readable = Object.fromEntries(Object.entries(msg).map(([k, v]) => [messageKeysLookup[k] + ` (${k})`, v]));
            console.debug("Successfully sent message to watch:", JSON.stringify(readable, null, 2));
        }, console.error);
    } catch(e) {
        console.error("Pebble.sendAppMessage failed.", e);
    }
}

Pebble.addEventListener('webviewclosed', function(e) {
    if (e && !e.response) return;

    var settings = clay.getSettings(e.response);

    var tickkey = messageKeys.TICK_UNITS;
    settings[tickkey] = ~((1 << settings[tickkey])-1);
    tickkey = messageKeys.QTM_TICK_UNITS;
    settings[tickkey] = ~((1 << settings[tickkey])-1);
    tickkey = messageKeys.WAKEUP_TICK_UNITS;
    settings[tickkey] = ~((1 << settings[tickkey])-1);

    console.log(settings);

    const weatherkey = messageKeys.WEATHER_REFRESH_RATE;
    settings[weatherkey] *= 360;

    const tempunitkey = messageKeys.WEATHER_TEMP_UNIT;
    const windunitkey = messageKeys.WEATHER_WIND_UNIT;
    const precipunitkey = messageKeys.WEATHER_PRECIP_UNIT;
    const weathermodekey = messageKeys.WEATHER_MODE;
    const healthmodekey = messageKeys.HEALTH_MODE;
    const hourmodekey = messageKeys.HOUR_MODE;

    settings[tempunitkey] = parseInt(settings[tempunitkey]);
    settings[windunitkey] = parseInt(settings[windunitkey]);
    settings[precipunitkey] = parseInt(settings[precipunitkey]);
    settings[weathermodekey] = parseInt(settings[weathermodekey]);
    settings[healthmodekey] = parseInt(settings[healthmodekey]);
    settings[hourmodekey] = parseInt(settings[hourmodekey]);

    const batmodekey = messageKeys.BAT_MODE;
    var batmode = 0;
    for (let i = 0; i < 3; i++) {
        if (settings[batmodekey + i]) {
            batmode |= (1 << i);
        }
        delete settings[batmodekey + i];
    }
    settings[batmodekey] = batmode;

    sendAppMessage(settings);
});

Pebble.addEventListener('ready', function(e) {
    weather.subscribe(sendAppMessage, console.error);
});

Pebble.addEventListener('appmessage', function(e) {
    console.debug("Received app message:", JSON.stringify(e.payload, null, 2));
    if ("REQ_WEATHER" in e.payload) {
        weather.updateConfig(e.payload);
        weather.get().then(sendAppMessage).catch(console.error);
    }
});
