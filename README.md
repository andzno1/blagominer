# Blagominer
| <img src="img/warning.png" width="36" align="left" alt="Warning"> There have been some major changes in the configuration file `miner.conf`. If coming from another Blagominer version, please make sure to download the new configuration file and change it according to your needs! |
| --- |

## Features
This version of Blagominer adds the following features:

* **Collision free dual mining** of Burstcoin and Bitcoin HD (configurable via priority).
    * Whenever a high priority coin interrupts the mining process of a coin with lower priority, the interrupted mining process will be resumed after the high priority coin has finished its mining process. Files that have completely been read before the interruption will not be scanned again.
    ![Example of console output when a mining process is being interrupted](img/interruptedMining.png?raw=true "Collision free dual mining")
    * Whenever there is new mining information for a coin with lower priority, the mining process of said coin will be started after the high priority coin has finished its mining process:
    ![Example of console output when a coin is being queued](img/queue.png?raw=true "Collision free dual mining")
* Tracking (and displaying) of **possibly corrupted plot files**. The data can be shown in the miner window and is also being logged to csv files. The following data is collected per plot file during mining:
    * Number of calculated deadlines that did not match with the wallet's/pool's calculation (`-DLs` column).
    * Number of calculated deadlines that have been confirmed (`+DLs` column).
    * Number of input/output errors that have been recorded (`I/O` colum).
    ![Statistics for possibly corrupted plot files](img/corrupted.png?raw=true "Statistics for possibly corrupted plot files")
* The miner can run as dedicated proxy. No mining needed.
* Notification for updates.

## Changes in the configuration file
Due to the new dual mining ability some fields in the configuration file have been moved to a different "section" and some have been added. The following table shows all the added fields.

| Field         | Explanation |
| ------------- | ------------- |
| `"Burst"."Enable"` and `"BHD"."Enable"` | `true` enables the mining of the corresponding coin, `false` disables it.  |
| `"Burst"."Priority"` and `"BHD"."Priority"` | A higher priority coin (lower value) interrupts the mining process of a coin with lower priority (higher value). This value must be `>= 0`. If both coins have the same priority, there won't be any interruptions and there will be a mining queue.  |
| `"Burst"."SubmitTimeout"` and `"BHD"."SubmitTimeout"` | Timeout in milliseconds after a deadline submission is regarded as failed. Increase this value if deadlines are being resent too many times.  |
| `"Burst"."ProxyUpdateInterval"` and `"BHD"."ProxyUpdateInterval"` | Interval in milliseconds for the proxy checking for new client requests.  |
| `"ShowCorruptedPlotFiles"`  | If you want to see possibly corrupted plot files in the console windows, set this to `true`, `false` otherwise.  |
| `"IgnoreSuspectedFastBlocks"`  | `true`: When tracking possibly corrupted plotfiles, mismatching deadlines that may have been caused by a fast block (there is no guarantee for correct fast block detection) will be ignored. `false`: Said deadlines will not be ignored.  |
| `"CheckForUpdateIntervalInDays"`  | Check for new versions every x days. Set it to `0` to disable update check.  |
| `"Logging"."logAllGetMiningInfos"`  | Set it to `true` if you want to log every received mining information and all updates sent by the proxies, even if there is no new information. This increases the log file size quite fast. Set it to `false` otherwise. |
| `"Logging"."EnableCsv"`  | Set it to `true` if you want to enable logging of mining statistics to csv files. Set it to `false` otherwise. |
| `"LockWindowSize"`  | Set it to `false` if you want to be able to resize the miner window. Set it to `true` otherwise. |

## Dependencies
If you want to compile Blagominer yourself you have to install and include [PDCurses](https://pdcurses.org/). The simplest way to do so is to install [vcpkg](https://github.com/Microsoft/vcpkg) and hook up user-wide integration (`vcpkg integrate install`).

Afterwards install PDCurses via this command:

    PS> .\vcpkg install pdcurses:x64-windows

For detailed information see the vcpkg [readme](https://github.com/Microsoft/vcpkg#vcpkg) and [FAQ](https://github.com/Microsoft/vcpkg/blob/master/docs/about/faq.md#frequently-asked-questions)