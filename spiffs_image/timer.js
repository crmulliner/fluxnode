/*
 * Copyright: Collin Mulliner
 */

/* jsondoc
{
"class": "Timer",
"longtext":"
This is a basic timer library.

The library is designed around `OnTimer()` and `Platform.setTimeout()`.
An application that uses this library should **NOT** have its own implementation
of `OnTimer()`.

The shortest timeout/interval is 10 milliseconds.
"
}
*/

__Timers = {
    timers: [],
    nextID: 1,
};


/* jsondoc
{
"name": "setTimeout",
"args": [
{"name": "func", "vtype": "function", "text": "function to call when the timer expires"},
{"name": "delay", "vtype": "uint", "text": "timeout in milliseconds"}
],
"return": "timer ID",
"text": "Timer to call a function after a given delay (in milliseconds).",
"example": "
// 5000 second timer
setTimeout(function(){print('timer\\n');}, 5000);
"
}
*/
function setTimeout(func, delay) {
    var id = __setTimeoutInternal(func, delay, false, -1);
    __installTimer();
    return id;
}

/* jsondoc
{
"name": "cancelTimer",
"args": [ {"name": "timerID", "vtype": "timerID", "text": "ID returned by setTimeout or intervalTimer"} ],
"text": "Delete timer.",
"example": "
var id = setTimer(x, 1000);
cancelTimer(id);
"
}
*/
function cancelTimer(id) {
    var idx = __timersFindIdxId(id);
    if (idx != -1) {
        __timersRemoveAt(idx);
    }
    __installTimer();
}

/* jsondoc
{
"name": "intervalTimeout",
"args": [
{"name": "func", "vtype": "function", "text": "function to call when the timer expires"},
{"name": "interval", "vtype": "uint", "text": "interval in milliseconds"}
],
"return": "timer ID",
"text": "Call function repeatedly in the given interval (in milliseconds) until canceled.",
"example": "
// call function x every 5 seconds
intervalTimeout(x, 5000);
"
}
*/
function intervalTimer(func, interval) {
    var id = __setTimeoutInternal(func, interval, true, -1);
    __installTimer();
    return id;
}

/* jsondoc
{
"name": "OnTimer",
"args": [],
"longtext": "
`OnTimer()` function that is provided by this library.

When using this library the main application cannot contain a OnTimer() function.
",
"example": "
"
}
*/
function OnTimer() {
    var expired = [];
    var d = new Date();
    var ts = d.getTime();
    for (var i = 0; i < __Timers.timers.length; i++) {
        if (ts >= __Timers.timers[i].expire) {
            expired.push(__Timers.timers[i].id);
            __Timers.timers[i].func();
        }
        if (ts < __Timers.timers[i].expire) {
            break;
        }
    }
    for (var i = 0; i < expired.length; i++) {
        var idx = __timersFindIdxId(expired[i]);
        var rt = __timersRemoveAt(idx);
        if (rt.interval) {
            __setTimeoutInternal(rt.func, rt.delay, true, rt.id);
        }
    }
    __installTimer();
}

function __installTimer() {
    if (__Timers.timers.length == 0) {
        Platform.setTimer(0);
        return;
    }
    var d = new Date();
    var ts = d.getTime();
    var diff = __Timers.timers[0].expire - ts;
    //print("diff: " + diff);
    Platform.setTimer(diff);
}

function __timersFindIdx(expire) {
    for (var i = 0; i < __Timers.timers.length; i++) {
        if (__Timers.timers[i].expire >= expire) {
            return i;
        }
    }
    return __Timers.timers.length;
}

function __timersFindIdxId(id) {
    for (var i = 0; i < __Timers.timers.length; i++) {
        if (__Timers.timers[i].id == id) {
            return i;
        }
    }
    return -1;
}

function __timersInsertAt(t, idx) {
    if (idx == 0) {
        __Timers.timers.unshift(t);
    }
    else if (idx == __Timers.timers.length) {
        __Timers.timers.push(t);
    } else {
        var ts = __Timers.timers.slice(0, idx);
        var te = __Timers.timers.slice(idx, __Timers.timers.length);
        __Timers.timers = ts;
        __Timers.timers.push(t);
        __Timers.timers = ts.concat(te);
    }
}

function __timersRemoveAt(idx) {
    if (idx == 0) {
        return __Timers.timers.shift();
    }
    else if (idx == __Timers.timers.length) {
        return __Timers.timers.pop();
    }
    var rt = __Timers.timers[idx];
    var ts = __Timers.timers.slice(0, idx);
    var te = __Timers.timers.slice(idx + 1, __Timers.timers.length);
    __Timers.timers = ts;
    __Timers.timers = ts.concat(te);
    return rt;
}

function __setTimeoutInternal(func, delay, interval, ID) {
    var d = new Date();

    var t = {
        func: func,
        expire: d.getTime() + delay ,
        delay: delay,
        id: ID != -1 ? ID : __Timers.nextID++,
        interval: interval,
    };

    var idx = __timersFindIdx(t.expire);
    __timersInsertAt(t, idx);
    return t.id;
}
