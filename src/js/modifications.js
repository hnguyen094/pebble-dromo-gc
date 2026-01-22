module.exports = function(minified) {
    const config = this;
    const $ = minified.$;
    const HTML = minified.HTML;

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

    const visualToggle = Toggle(["INVERTED_TIMEBOX", "ACCENT_COLOR", "BATTERY_PERCENTAGE", "HOUR_MODE", "TZ_MODE"]);
    const basicToggle = Toggle(["TICK_UNITS", "SUBSCRIBE_TO_DATA", "WRIST_FLICK", "HEALTH_MODE", "WEATHER_MODE"]);
    const weatherToggle = Toggle(["WEATHER_REFRESH_RATE", "WEATHER_FORECAST_RANGE", "WEATHER_TEMP_UNIT", "WEATHER_WIND_UNIT"], ["WEATHER_DESC"]);
    const wakeupToggle = Toggle(["TAP_WAKEUP_DURATION", "WAKEUP_LIGHT", "WAKEUP_TICK_UNITS", "TZ_ID"]);
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

    var timezonesJSON = 0; // 0 is unset, null is failed fetch. yuck.
    var built = false;
    var tzDebug = "Loading...";

    const postBuild = function(wasError) {
        const item = config.getItemByMessageKey("TZ_ID");
        const idstate = config.getItemByMessageKey("TZ_ID_STATE");
        if (wasError && idstate.get()) {
            item.$manipulatorTarget.add(HTML('<option value="{{this}}" class="item-select-option">{{this}}</option>', idstate.get()));
        }
        const debug = config.getItemById("TZ_DEBUG");
        if (wasError) debug.show();
        debug.set(`${tzDebug}.<br>Try exiting and re-entering the configuration page if needed.`);
        item.set(idstate.get());
        item.on('change', function() {
            idstate.set(item.get());
        });
    }

    const loadTimezones = function(timezonesJSON) {
        const item = config.getItemByMessageKey("TZ_ID");
        const debug = config.getItemById("TZ_DEBUG");
        const idstate = config.getItemByMessageKey("TZ_ID_STATE");
        try {
            const timezones = JSON.parse(timezonesJSON);
            item.$manipulatorTarget.add(HTML('{{each}}<option value="{{this}}" class="item-select-option">{{this}}</option>{{/each}}', timezones));
            /* implements optgroups
            var groups = [];
            var group = {label: "", values:[]};
            for (timezone of timezones) {
                const tokens = timezone.split(/[/](.*)/);
                if (group && group.label !== tokens[0]) {
                    groups.push(group);
                    group = {values:[]};
                }
                group.label = tokens[0];
                group.values.push({ tz:timezone, label:(tokens.length > 1 ? `${tokens[1]} (${tokens[0]})` : timezone) });
            }
            groups.push(group);
            for (group of groups) {
                item.$manipulatorTarget.add(HTML('<optgroup label={{label}}> {{each values}}<option value="{{this.tz}}" class="item-select-option">{{this.label}}</option>{{/each}}</optgroup>', group));
            }
            */
            tzDebug = `Loaded ${timezones.length} timezones.`;
            postBuild(false);
        } catch (e) {
            tzDebug = `Failed to load timezones.<br>${e}`;
            postBuild(true);
        }
        item.set(idstate.get());
        item.on('change', function() {
            idstate.set(item.get());
        });
    }

    const getTimezones = function() {
        $.request('get', 'http://worldtimeapi.org/api/timezone')
            .then(function(tzJSON) {
                if (built) loadTimezones(tzJSON);
                timezonesJSON = tzJSON;
            })
            .error(function(status, text, xhr) {
                tzDebug = `Failed to fetch timezones. (${status})`;
                timezonesJSON = null;
                if (built) {
                    postBuild(true);
                }
            });
    }

    // config.on(config.EVENTS.BEFORE_BUILD, getTimezones);

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

        item = config.getItemByMessageKey("TZ_ID_STATE");
        item.hide();

        item = config.getItemById("TZ_DEBUG");
        item.hide();

        item = config.getItemById("TZ_BUTTON");
        item.on('click', getTimezones);
        item.hide();

        built = true;
        if (timezonesJSON === null) {
            postBuild(true);
        } else if (timezonesJSON !== 0) {
            loadTimezones(timezonesJSON);
        }
        getTimezones();
    });
}
