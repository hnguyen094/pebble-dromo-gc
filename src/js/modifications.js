module.exports = function(minified) {
    const config = this;

    const toggle = function(condition, keys, ids = []) {
        for(let i = 0; i < keys.length; i++) {
            if (condition) config.getItemByMessageKey(keys[i]).show();
            else config.getItemByMessageKey(keys[i]).hide();
        }
        for(let i = 0; i < ids.length; i++) {
            if (condition) config.getItemById(ids[i]).show();
            else config.getItemById(ids[i]).hide();
        }
    }

    const Toggle = function (keys, ids = [], callback = null) {
        return {
            state: 0,
            keys: keys,
            ids: ids,
            callback: callback,
            toggle:  function() {
                toggle(this.state, keys, ids);
                this.state = 1-this.state;
                if (callback != null) callback(this.state);
            }
        };
    };

    const visualToggle = Toggle(["INVERTED_TIMEBOX", "ACCENT_COLOR", "BATTERY_PERCENTAGE", "HOUR_MODE"]);
    const basicToggle = Toggle(["TICK_UNITS", "SUBSCRIBE_TO_DATA", "WRIST_FLICK", "HEALTH_MODE", "WEATHER_MODE"]);
    const weatherToggle = Toggle(["WEATHER_REFRESH_RATE", "WEATHER_FORECAST_RANGE", "WEATHER_TEMP_UNIT", "WEATHER_WIND_UNIT"], ["WEATHER_DESC"]);
    const wakeupToggle = Toggle(["TAP_WAKEUP_DURATION", "WAKEUP_LIGHT", "WAKEUP_TICK_UNITS"]);
    const batteryToggle = Toggle(["BAT_MODE", "BAT_MODE_LB", "BAT_MODE_ST_START", "BAT_MODE_ST_END", "QTM_TICK_UNITS"], [], function(state) {
        const item = config.getItemByMessageKey("BAT_MODE");
        toggle(item.get()[0] && !state, ["BAT_MODE_LB"]);
        toggle(item.get()[1] && !state, ["BAT_MODE_ST_START", "BAT_MODE_ST_END"]);
    });

    const toggleWakeUp = function() {
        toggle(this.get() > 0, [], ["WAKEUP_HEADER"]);
        toggle(this.get() > 0 && !wakeupToggle.state, wakeupToggle.keys, wakeupToggle.ids);
    }

    const toggleWeather = function() {
        toggle(this.get() > 0, [], ["WEATHER_HEADER"]);
        toggle(this.get() > 0 && !weatherToggle.state, weatherToggle.keys, weatherToggle.ids);
    }

    const toggleBattery = function() {
        toggle(this.get()[0] && !batteryToggle.state, ["BAT_MODE_LB"]);
        toggle(this.get()[1] && !batteryToggle.state, ["BAT_MODE_ST_START", "BAT_MODE_ST_END"]);
    }

    config.on(config.EVENTS.AFTER_BUILD, function () {
        var item = null;

        item = config.getItemById("VISUAL_HEADER");
        visualToggle.toggle();
        item.on('click', () => visualToggle.toggle() );

        item = config.getItemById("BASIC_HEADER");
        basicToggle.toggle();
        item.on('click', () => basicToggle.toggle() );

        item = config.getItemById("WEATHER_HEADER");
        weatherToggle.toggle();
        item.on('click', () => weatherToggle.toggle() );

        item = config.getItemById("WAKEUP_HEADER");
        wakeupToggle.toggle();
        item.on('click', () => wakeupToggle.toggle() );

        item = config.getItemById("BATTERY_HEADER");
        batteryToggle.toggle();
        item.on('click', () => batteryToggle.toggle() );

        item = config.getItemByMessageKey("BAT_MODE");
        toggleBattery.call(item);
        item.on('change', toggleBattery);

        item = config.getItemByMessageKey("WRIST_FLICK");
        toggleWakeUp.call(item);
        item.on('change', toggleWakeUp);

        item = config.getItemByMessageKey("WEATHER_MODE");
        toggleWeather.call(item);
        item.on('change', toggleWeather);

        item = config.getItemByMessageKey("WEATHER_PRECIP_UNIT");
        item.hide();
    });
}
