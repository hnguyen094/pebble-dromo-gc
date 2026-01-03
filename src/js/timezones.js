const TZ_KEY = 'TZ_KEY';

var timezones = localStorage.getItem(TZ_KEY);
if (timezones !== null) {
    timezones = JSON.parse(timezones);
}

const indexOf = function(timezone) {
    return new Promise(function(resolve, reject) {
        var xhr = new XMLHttpRequest();
        xhr.onload = function () {
            const newTimezones = JSON.parse(this.responseText);
            localStorage.set(TZ_KEY, this.responseText);
            timezones = newTimezones;
            resolve(timezones.indexOf(timezone));
        }
        xhr.onerror = reject;
        xhr.open('GET', `http://worldtimeapi.org/api/timezone`);
        xhr.send();
    });
}

const getTimezoneOffset = function(timezone) {
    return new Promise(function(resolve, reject) {
        const url = `http://worldtimeapi.org/api/timezone/${timezone}`;
        var xhr = new XMLHttpRequest();
        xhr.onload = function () {
            const response = JSON.parse(this.responseText);
            const keys = require('message_keys');
            const code = '+-'.includes(response.abbreviation[0])
                ? `UTC${response.abbreviation}` : response.abbreviation;
            resolve({
                [keys.TZ_OFFSET]: response.raw_offset + response.dst_offset,
                [keys.TZ_CODE]: code
            });
        }
        xhr.onerror = reject;
        xhr.open('GET', url);
        xhr.send();
    });
}

const getTimezoneOffsetFromIndex = function(index) {
    getTimezoneOffset(timezones[index]);
}

module.exports = {
    indexOf: indexOf,
    get: getTimezoneOffset,
    getFromIndex: getTimezoneOffsetFromIndex
};
