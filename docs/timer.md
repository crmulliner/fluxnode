# Timer

This is a basic timer library.

The library is designed around `OnTimer()` and `Platform.setTimeout()`.
An application that uses this library should **NOT** have its own implementation
of `OnTimer()`.

The shortest timeout/interval is 10 milliseconds.

## Methods

- [OnTimer](#ontimer)
- [cancelTimer](#canceltimertimerid)
- [intervalTimeout](#intervaltimeoutfuncinterval)
- [setTimeout](#settimeoutfuncdelay)

---

## OnTimer()

`OnTimer()` function that is provided by this library.

When using this library the main application cannot contain a OnTimer() function.


```

```

## cancelTimer(timerID)

Delete timer.

- timerID

  type: timerID

  ID returned by setTimeout or intervalTimer

```
var id = setTimer(x, 1000);
cancelTimer(id);

```

## intervalTimeout(func,interval)

Call function repeatedly in the given interval (in milliseconds) until canceled.

- func

  type: function

  function to call when the timer expires

- interval

  type: uint

  interval in milliseconds

**Returns:** timer ID

```
// call function x every 5 seconds
intervalTimeout(x, 5000);

```

## setTimeout(func,delay)

Timer to call a function after a given delay (in milliseconds).

- func

  type: function

  function to call when the timer expires

- delay

  type: uint

  timeout in milliseconds

**Returns:** timer ID

```
// 5000 second timer
setTimeout(function(){print('timer\n');}, 5000);

```

