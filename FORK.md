# What this fork changes

A fork of [pvvx/BLETHR](https://github.com/pvvx/BLETHR) v1.2, kept for a handful of repeaters that
sit on freezer doors and have to survive months without a visit. Everything here came out of
running them, so each change below says what went wrong first and how the fix was checked.

Builds from this fork report `V1.2+a`, `V1.2+b` and so on in the Software Revision String, so a
device can be told apart from one running the release it is based on. `SW_VERSION` itself is
deliberately untouched, for the reason under **The version rule** below.

## The display never worked on half the hardware

`init_lcd()` probes the B1.4 / B1.7 / B2.0 display controller, stores its address, and returns
without ever sending the initialisation sequence. The panel stays blank from the first power-on
while the radio and the beacon work normally. After an OTA it appears to work until the first
power cycle, because the previous firmware had already initialised the controller.

The block was switched off because the condition it was copied with from ATC_MiThermometer tests
`cfg.flg2.screen_off`, and this firmware's `dev_cfg_t` has no `flg2` field, so it could not
compile. Ten lines below, the B1.9 branch meets the same obstacle and is handled the other way
round: only the condition is commented out and the init calls survive in a bare block. This gives
the B1.4 branch the same treatment.

In a build of the untouched source the linker discards `lcd_init_cmd_b14` and `lcd_init_clr_b14`
entirely, as nothing references them, so the released binary does not merely skip the
initialisation, it does not contain it.

Verified on two LYWSD03MMC boards reporting Hardware Revision B1.7: picture present, and still
present after a power cycle.

## A device that lost its source stayed lost

The search listened for 125 ms out of every 1.28 s and counted failures until 255, then cleared
`wrk.scan_enable` and never scanned again. Only a power cycle or a BLE disconnect brought a
device back, so a source that went quiet for about five and a half minutes, because a door closed
on it or a battery was changed slowly, cost a site visit.

It was also a lottery. A 125 ms window against a source that beacons every 5 s catches it with
probability 0.025, so 255 attempts leave a 1 in 600 chance of hearing nothing at all on a perfect
link, and every 25th search of a 10 s source ended in the permanent stop with no fault present.

A continuous window longer than the source's beacon period turns that into a certainty and
costs less: 10.5 s spans every period the firmware can be configured for, and a reception ends
the sweep the moment it arrives, so the length is only spent proving a source is absent. Two such
sweeps make a search, because at the slowest configurable source a single sweep holds exactly one
beacon, and then one lost packet is a failed search. That was measured the hard way: with one
sweep both bench devices parked within minutes of a 10 s source being set.

Failure now parks the device rather than ending it. It keeps beaconing at the stack's longest
advertising interval and searches again after 2 minutes, then 4, 8, 16, 32 and 60, holding there;
a successful sync starts the ladder over, and so does the end of any connection, because someone
who has just changed the source expects the device to go and look. Doubling rather than
quadrupling is deliberate: a failed search does not only mean the source has gone, and on a
marginal link four unlucky sweeps reached the hour cap with the source sitting right there.
A source that has genuinely gone costs about 0.7 mAh a day in the steady state, roughly three
hundred days on a CR2032, in exchange for recovering by itself whenever it comes back.

A source that is heard but not locked onto is a separate case, and the worst one for power.
There are two ways it happens. Its period can disagree with the configured one, which fails every
time, or its next beacon can simply not arrive, which on a marginal link fails now and then. Both
went through a path that resets the error count, so upstream sweeps and half-syncs for as long as
it has a battery, at roughly a third of full duty, looking healthy the whole time because every
search reception refreshes the beacon and the display. Five such failures in a row now park the
device. Five rather than three because of the second kind: at the third of all windows this bench
loses, five in a row is a quarter of a percent per attempt, while a source that has really gone
fails every attempt and still parks inside a minute.

What does not count is an ordinary missed window in low power mode. That is what the error count
and its own limit escalate, and counting it here parked a healthy device after its third missed
beacon. Worth writing down, because this fork did exactly that for two revisions and it looked
like a bad radio link rather than a counting mistake.

Measured on hardware: receptions landed 3055, 5134 and 5169 ms into a 5300 ms sweep, spread
rather than clustered at the start, which is what says the radio listens for the whole length
rather than stopping early.

Measured on hardware afterwards, with a 10 s source and the firmware's default receive windows:
seventeen minutes, every advertisement heard carrying the source's reading, not one park, and the
time from the start of a scan to the packet that ended it running 4 to 12 ms with a median of 8.
The same setup parked every couple of minutes before the counting above was corrected.

## A parked device kept broadcasting a reading that was no longer true

`bthome_data_beacon()` only runs when a packet has arrived, so when nothing arrives the
advertisement keeps its last contents, the same temperature and the same packet id, for as long
as the device has power. A consumer sees a device that is present and answering with a plausible
reading, and has no way to tell that the reading stopped being current hours ago.

A parked device now advertises its own fields only: packet id, voltage and the error count. The
source's temperature and humidity are absent, and their absence is the signal.

Be careful about what that buys on the consumer side. Home Assistant keeps a device's entities
available for as long as the device is advertising anything at all, and does not expire them per
field, so the relayed temperature does not vanish: it freezes at the last value received while the
voltage and the error count carry on moving. Telling the two apart is therefore something the
consumer has to do, by noticing that one field has stopped while others have not, and the
integration here does exactly that. What the firmware guarantees is only that it stops asserting a
reading it no longer has.

The battery percentage still describes the source, as upstream documents, and this device's own
cell is still the voltage. That is not a compromise but the only layout that works: a consumer
postfixes every occurrence of a measurement type that appears more than once in a payload, so two
battery objects produce two indexed keys and strand the existing one, and there is exactly one
battery percentage type in BTHome to go around.

## Smaller corrections

**The search stage ignored its own sentinel.** `start_tik` is documented in scaning.h as zero
meaning "not scanning, sleep allowed", and `start_adv_scanning()` enforces that with
`clock_time() | 1` so a real timestamp can never be zero. The sleep decision and the LP branch
both test it; the search branch did not, and computed an elapsed time from a free running counter
instead. It fired on nearly every main loop pass in which no scan was running, which roughly
halved the real search budget and made the error count on the display overstate the misses.

**The display stuck on the error screen.** `send_task()` only redraws when the incoming value
differs from the one it drew last, using `measured_data.temp` as the cache, and the error screens
write straight over exactly those digits. When the source came back with the same reading it had
before the outage, nothing differed, nothing was redrawn, and the panel kept showing an error
while the beacon already carried good data. A stable reading is the normal case in a freezer. The
cache could not simply be invalidated instead, because the advertisement is built from the same
two fields.

**The battery service always said zero.** The characteristic behind GATT 0x2A19 is published and
notified on a timer, and the variable behind it was never written.

**The battery was measured under the radio's load.** battery.h states the calibration the
thresholds assume, about 3100 mV unloaded and about 2950 mV during a measurement, the difference
being the ADC's own 0.4 mA. But `check_battery()` runs from the main loop on a timer, and the main
loop keeps running while a scan window is open, so roughly a tenth of measurements read the cell
under the radio instead.

Skipping a measurement while a scan is open is not enough here, and this fork made that worse
rather than better. A search sweep holds the radio on for 10.5 s at a stretch, and the first main
loop pass after it closes takes the reading, on a cell that has just been delivering receive
current. What that reading decides is a two minute deep sleep, and the check that runs after the
reset it causes happens before the radio starts, so a cold cell which is perfectly healthy unloaded
can end up in a sleep and reboot loop that never advertises again. It is the only path here by which
a powered, undamaged device falls completely silent. The measurement therefore waits until the radio
has been off for three seconds.

Worth knowing alongside it: the voltage in the advertisement is a 32 sample running average that
lags by eight to sixteen minutes, while the cut off compares the instantaneous reading. A healthy
looking voltage in a beacon does not rule the threshold out.

**A connection left open blinded the device.** Scanning stops for as long as a client is
connected, and only the disconnect brings it back, so a client that never disconnects leaves the
repeater relaying nothing, silently. The supervision timeout does not cover it: at four seconds
it is short, but it only ends a link whose peer has stopped answering. Connections now end after
five minutes, with OTA exempt.

**Elapsed time was measured against a clock a client can set.** `wrk.utc_time_sec` is the wall
clock and command 0x23 writes it directly. After a cold start it counts from zero, so the first
client to write a real time jumps it forward by decades and the connection it is using is torn
down immediately for having lasted too long. A separate counter, stepped in the same catch-up
loop and never written from outside, now measures every interval.

## Deliberately not changed

**The watchdog stays off.** Nothing in this firmware has a reachable hang: the I2C and UART waits
all terminate on their own, the master runs without clock stretching, and the flash driver feeds
the watchdog itself. Nothing in the SDK sleep path touches it either, and whether the counter
stops while the part is suspended cannot be settled from the headers, so turning it on risks a
device that does nothing but reboot, in a place nobody can reach. Parking and the backoff already
recover from the failure that actually happens. Revisit only on evidence of a real hang in the
field, and then in a release of its own with a long soak test of the parked state.

**The battery thresholds stay where they are.** Lowering them looks tempting on a device that
dies cold, but the failure we actually saw was at 2.633 V, far above any of them: it was the
cell's internal resistance under load, not the threshold. `END_VBAT_MV` is 2000 because flash
writes below 2 V are not safe, and this firmware writes flash on every configuration command.
Upstream's own answer is the right one: a CR2032 is not recommended in this role.

## The version rule

`flash_supported_eep_ver(0x1010, 0x1000 + SW_VERSION)` erases the four configuration sectors at
0x7C000..0x7FFFF, and with them the source address, the interval, the bind key and the clock
correction, whenever the version found in flash is missing or below its first argument.

Two rules follow, and breaking either costs the user those settings:

* `SW_VERSION` must stay at 0x10 or above, so the version this build stores is not rejected by a
  later one.
* the minimum must stay at 0x1010, so this firmware does not wipe a device arriving from a stock
  1.x release.

Since this build stores 0x1012, exactly what stock 1.2 stores, a device can be moved between the
two in either direction without losing its settings. This is why the fork revision lives in the
Software Revision String and not in `SW_VERSION`.

## Tools

`tools/telink_ota.py` flashes a device over BLE from a command line, which the vendor's Web
Bluetooth page cannot be scripted to do. It refuses an image without the Telink magic at offset
eight, retries the connection because a device busy scanning turns the first attempts away, and
reads the Software Revision String back afterwards, so a run reports what is now on the device
rather than that the writes were sent.

`tools/blethr.py` reads and writes a running device's configuration: which thermometer it
repeats, the interval, the windows, the radio power. Writing reads the current configuration
first and refuses when it cannot be read, because the device takes the configuration as one
struct and sending a zero for the interval switches scanning off, which looks like a healthy
device that quietly repeats nothing.

Both need `bleak` and an adapter the host's Bluetooth stack supports.

## The binaries in this repository are upstream's

`ATC_bthr_v12.bin`, `LKTMZL02_bthr_v12.bin` and `ZYZTH02P_bthr_v12.bin` are the images that came
with v1.2 and none of the work here is in them. Flashing one of those gets a stock device, not
this fork. Build from source, or take a binary from a release of this fork if there is one.

## Building

As upstream: the Telink TC32 toolchain, and

    make PROJECT_NAME=ATC_bthr_v12 POJECT_DEF="-DDEVICE_TYPE=DEVICE_LYWSD03MMC"

with the other two targets as in `MakeAll.cmd`. All three build clean.

`SCAN_DEBUG_TIM` in app_config.h is a temporary diagnostic that puts the milliseconds between the
start of a scan and the packet that ended it into the advertisement. It answers what the headers
cannot, which is whether a long sweep really listens throughout, and it is off in a release.
