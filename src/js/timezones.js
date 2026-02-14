const TZ_KEY = 'TZ_KEY';

var timezones = localStorage.getItem(TZ_KEY);
if (timezones !== null) {
    timezones = JSON.parse(timezones);
}

const indexOf = function(timezone) {
    return new Promise(function(resolve, reject) {
        var xhr = new XMLHttpRequest();
        xhr.onload = function () {
            let newTimezones = JSON.parse(this.responseText);
            newTimezones = newTimezones.sort();
            localStorage.set(TZ_KEY, JSON.stringify(newTimezones));
            timezones = newTimezones;
            resolve(timezones.indexOf(timezone));
        }
        xhr.onerror = reject;
        xhr.open('GET', `https://time.now/developer/api/timezone`);
        xhr.send();
    });
}

const getTimezoneOffset = function(timezone) {
    return new Promise(function(resolve, reject) {
        const url = `https://time.now/developer/api/timezone/${timezone}`;
        var xhr = new XMLHttpRequest();
        xhr.onload = function () {
            if (xhr.status != 200) {
                reject(new Error(`Timezone GET failed with error ${xhr.status} ${xhr.statusText}.`));
            } else {
                const response = JSON.parse(this.responseText);
                const keys = require('message_keys');
                const code = '+-'.includes(response.abbreviation[0])
                    ? `UTC${response.abbreviation}` : response.abbreviation;
                resolve({
                    [keys.TZ_OFFSET]: response.raw_offset + response.dst_offset,
                    [keys.TZ_CODE]: code
                });
            }
        }
        xhr.onerror = () => reject(new Error("Failed to make timezone GET request."));
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
