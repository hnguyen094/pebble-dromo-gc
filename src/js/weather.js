
// https://gist.githubusercontent.com/stellasphere/9490c195ed2b53c707087c8c2db4ec0c/raw/descriptions.json
const wmoCodes = require('./wmoCodes_stellasphere.json');
const LOCATION_KEY = 'LOCATION_KEY';
const CONFIG_KEY = 'CONFIG_KEY';

const TempUnit = ["celsius", "fahrenheit"];
const WindUnit = ["kmh", "ms", "mph", "kn"];
const PrecipUnit = ["mm", "inch"];

var watchID = null;
var location = localStorage.getItem(LOCATION_KEY);
if (location === null) {
    location = [-1000, -1000];
} else {
    location = JSON.parse(location);
}
var cachedConfig = localStorage.getItem(CONFIG_KEY);
if (cachedConfig !== null) {
    cachedConfig = JSON.parse(cachedConfig);
}

// https://stackoverflow.com/a/4000907 using ~1mi heuristic
function past_threshold(a, b) {
    const lat_threshold = 0.018519;
    const lon_threshold = 0.014472;
    if (Math.abs(a[0] - b[0]) > lat_threshold || Math.abs(a[1] - b[1]) > lon_threshold) {
        return true;
    }
    return false;
}

const jsonParser = function (k,v) {
    return (typeof v === "object" || isNaN(v)) ? v : Math.round(v);
}

const options = {
    enableHighAccuracy: false,
    maximumAge: 86400000 // 1 day. This doesn't seem to be working correctly, so possibly causes the phone to use location much more than necessary.
};

const updateConfig = function(config) {
    localStorage.setItem(CONFIG_KEY, JSON.stringify(config));
    cachedConfig = config;
};

const min = function(arr, startIndex, ahead) {
    var best = Infinity;
    for (let i = startIndex; i < startIndex + ahead; i++) {
        if (arr[i] < best) best = arr[i];
    }
    return best;
}

const max = function(arr, startIndex, ahead) {
    var best = -Infinity;
    for (let i = startIndex; i < startIndex + ahead; i++) {
        if (arr[i] > best) best = arr[i];
    }
    return best;
}

const mostFreq = function(arr, startIndex, ahead) {
    var counter = {'0':0, '1':0};
    var best = [0, 0];
    for (let i = startIndex; i < startIndex + ahead; i++) {
        counter[arr[i]] = 1;
        if (counter[arr[i]] > best[0]) {
            best = [counter[arr[i]], arr[i]];
        }
    }
    return best[1];
}

const firstFutureIndex = function(arr, cur) {
    for (let i = 0; i < arr.length; i++) {
        if (arr[i] > cur) return i;
    }
    return arr.length-1;
}

const getWeather = function() {
    return new Promise(function(resolve, reject) {
        if (location === null) {
            return reject("Coding error. No location.");
        } else if (cachedConfig === null) {
            return reject("Error. No config.");
        }
        lat = location[0];
        lon = location[1];

        const url = `https://api.open-meteo.com/v1/forecast?latitude=${location[0]}&longitude=${location[1]}&hourly=temperature_2m,precipitation_probability,wind_speed_10m,weather_code,is_day&current=temperature_2m,precipitation,weather_code,is_day,wind_speed_10m&timezone=auto&forecast_days=2&timeformat=unixtime&wind_speed_unit=${WindUnit[cachedConfig.WEATHER_WIND_UNIT]}&temperature_unit=${TempUnit[cachedConfig.WEATHER_TEMP_UNIT]}&precipitation_unit=${PrecipUnit[cachedConfig.WEATHER_PRECIP_UNIT]}`;

        var xhr = new XMLHttpRequest();
        xhr.onload = function () {
            const response = JSON.parse(this.responseText, jsonParser);
            const index = firstFutureIndex(response.hourly.time, response.current.time);
            const ahead = cachedConfig.WEATHER_FORECAST_RANGE;
            const keys = require('message_keys');
            const result = {
                [keys.CURRENT_TEMP]: response.current.temperature_2m,
                [keys.CURRENT_COND]: wmoCodes[response.current.weather_code][response.current.is_day ? "day" : "night"].description,
                [keys.CURRENT_PRECIP]: response.hourly.precipitation_probability[index],
                [keys.CURRENT_WIND]: response.current.wind_speed_10m,
                [keys.FUTURE_COND]: wmoCodes[max(response.hourly.weather_code, index-1, ahead+1)][mostFreq(response.hourly.is_day, index-1, ahead+1) ? "day" : "night"].description,
                [keys.FUTURE_MIN]: min(response.hourly.temperature_2m, index-1, ahead+1),
                [keys.FUTURE_MAX]: max(response.hourly.temperature_2m, index-1, ahead+1),
                [keys.FUTURE_PRECIP]: max(response.hourly.precipitation_probability, index, ahead),
                [keys.FUTURE_WIND]: max(response.hourly.wind_speed_10m, index-1, ahead+1)
            };
            resolve(result);
        };
        xhr.onerror = reject;
        xhr.open('GET', url);
        xhr.send();

        /* geocoding
         `https://geocode.arcgis.com/arcgis/rest/services/World/GeocodeServer/reverseGeocode?location={location[1]},{location[0]}&forStorage=false&outFields=City,RegionAbbr&f=json`
         */
    });
};

const subscribe = function(resolve, reject) {
    try {
        if (watchID !== null) {
            navigator.geolocation.clearWatch(watchID);
            watchID = null;
        }
        watchID = navigator.geolocation.watchPosition(function (pos) {
            const newlocation = [pos.coords.latitude, pos.coords.longitude];
            if (past_threshold(location, newlocation)) {
                localStorage.setItem(LOCATION_KEY, JSON.stringify(newlocation));
                location = newlocation;
                getWeather(cachedConfig).then(resolve);
            }
        }, function (e) {
            reject("Location update failed.");
        }, options);
    } catch (e) {
        reject(e);
    }
};

module.exports = {
    updateConfig: updateConfig,
    subscribe: subscribe,
    get: getWeather
};
