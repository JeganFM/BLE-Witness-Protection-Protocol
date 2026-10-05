# Synchronized Identity Rotation for BLE Devices

Technical report for the BLE Witness Protection Protocol project.

## Summary

Bluetooth Low Energy (BLE) devices advertise an identifier that any nearby scanner can read. If that identifier never changes, the device can be followed. BLE already lets devices change their address over time, but the changes are not coordinated, and the timing and signal strength of a device can still give it away. This project asks a narrow question: if several devices that belong together change their identity at the same moment, can they hide among each other?

I built a small testbed of five nRF52840 boards, wrote firmware for five identity-rotation strategies, ran each one while a passive observer recorded what an outsider could see, and compared the results. The main finding is that coordinating the change works: on a shared sync signal the three listeners switched identity together at every rotation I measured. The main limitation is that the devices rotated their advertised name instead of their Bluetooth address, so the results show the coordination mechanism but are not a full privacy guarantee.

## 1. Motivation

BLE is everywhere: fitness trackers, earbuds, medical sensors, car keys, door locks, asset tags. More than three billion BLE devices shipped in 2022 alone. Almost all of them broadcast something to the world, and any ordinary scanner can log it.

If the broadcast identifier is fixed, a scanner can build a record of where a device has been and when. The device's owner never has to connect to anything or agree to anything. That is the tracking problem, and it is the reason the Bluetooth specification introduced Resolvable Private Addresses (RPAs): an address that looks random to outsiders, changes regularly, and can still be recognised by a trusted peer.

RPAs help, but published research has shown that they are not enough on their own:

- Devices often keep the same advertising interval, payload or signal-strength pattern across an address change, so an observer can match the old and new address.
- If the central and the peripheral change addresses at different moments, the gap between the two changes can be used to link them.
- Many cheap or old devices never rotate at all.

### Why "witness protection"

The name comes from witness protection, where a person at risk is given a new name and a new place so that people who knew the old identity cannot find them. The protocol does the same for a device: it takes a new name and stops using the old one.

The comparison also shows the weak spot. A new name does not help if the person stays in the same house, because anyone watching can still see who comes and goes. For a BLE device, the house is its position, which a scanner can estimate from signal strength. Test 4 showed exactly this: the names changed together, but signal strength still linked each new name to the old one. Hiding position as well as name is the next problem.

### Why coordination

Most of the work on coordinating address changes looks at a single pair of devices. Real setups are not pairs. A phone talks to a watch, earbuds and a tracker at once, and a car has many sensors. So the question I wanted to test on real hardware was how a group of peripherals can change identity together, and what it costs when they do. I chose to build and measure it rather than simulate it, because timing and signal strength are exactly the parts that simulations tend to get wrong.

## 2. Background and the math

### 2.1 How an RPA is built

A Resolvable Private Address is 48 bits long. It is made of a 24-bit part called `prand` and a 24-bit hash:

$$\text{RPA} = \text{prand} \mathbin{\Vert} \text{hash}, \qquad \text{hash} = \text{ah}(\text{IRK},\, \text{prand})$$

Here $\Vert$ means joining two bit strings end to end. `prand` has its top two bits fixed to `01`, which marks the address as resolvable, so 22 of its 24 bits are random. The hash function `ah` is one AES-128 encryption (written $e$) of `prand`, padded with 104 zero bits ($0^{104}$) to fill a 128-bit block, under the 128-bit Identity Resolving Key (IRK). Only the lowest 24 bits of the result are kept:

$$\text{ah}(k, r) = e\big(k,\; 0^{104} \mathbin{\Vert} r\big) \bmod 2^{24}$$

A peer that holds the IRK recomputes the hash and compares. A device that does not hold the IRK cannot do this, so to it the address is just 48 random-looking bits. A wrong IRK matches by chance with probability $2^{-24}$. The specification suggests changing the address about every 15 minutes by default.

### 2.2 Why rotation alone leaks

An observer sees a stream of (time, identifier, signal strength) records. If a device rotates every $T_0$ seconds on a fixed schedule, then once the observer has seen two rotations it knows every future rotation time. The uncertainty about the next rotation time, measured as entropy in bits (roughly, how many yes/no questions it takes to pin the time down), is zero:

$$H(\text{next rotation}) = 0 \quad \text{when the interval is fixed.}$$

When an identifier disappears at time $t$ and a new one appears at $t+\varepsilon$, the two are easy to link if $\varepsilon$ is small. Received signal strength (RSSI) is a second clue, because it falls with distance. The standard log-distance path-loss model is:

$$\text{RSSI}(d) = \text{RSSI}(d_0) - 10\,\gamma\,\log_{10}\!\left(\frac{d}{d_0}\right) + \eta$$

where $d$ is the distance, $d_0$ a reference distance, $\gamma$ the path-loss exponent of the environment and $\eta$ random noise. I did not fit this model to the data. It is here to explain why two devices at similar distances from an observer have similar RSSI traces, which is what the Test 1 graph shows.

### 2.3 The effect of rotating together

Suppose $N$ devices rotate at the same instant. The observer sees $N$ old identities vanish and $N$ new ones appear. If the identifiers say nothing about which device they belong to, and the observer has only the timing of the changes, every pairing of old to new is equally likely, and a guess for any one device is right with probability:

$$P(\text{correct link}) = \frac{1}{N}$$

If the devices rotate at different moments, the observer can pair each old identity with the new one that follows it, and the probability rises towards 1. That is the reason for synchronising.

The $1/N$ figure is an ideal bound, and it rests on two assumptions: identifiers that reveal nothing about the device, and no other side information. The experiments below break both. The names in this testbed keep a per-device prefix such as `Iris9_`, and signal strength tells the devices apart.

## 3. Approach: rotate the advertised name

Changing the Bluetooth address itself would have meant changing how Zephyr's Bluetooth stack manages its built-in privacy features. That code is tightly integrated with the stack and the specification, and modifying it risks breaking things that are hard to debug. Instead the devices rotate the advertised device name, which is a field in the advertising packet that can be changed safely at run time.

A name such as `Iris9_3` then plays the role of the address: a fixed prefix joined to a number, $\text{ID}_k = \text{prefix} \mathbin{\Vert} n_k$. In the test firmware the prefix is fixed for each device (`Iris6_`, `Iris7_`, `Iris9_`), so the name alone still tells an observer which device it is. The experiments therefore test the timing and coordination of the changes, not whether the names are unlinkable. That is what I wanted to measure, and it also means the results do not transfer one-for-one to address rotation. I come back to that in the limitations.

## 4. Testbed

Five Raspberry Pis, each driving an nRF52840 development kit over a SEGGER J-Link, running Zephyr RTOS on nRF Connect SDK v2.7.0:

| Node | Role |
|------|------|
| Iris3 | Broadcaster. Sends a periodic `SYNC` advertisement carrying a counter. |
| Iris6, Iris7, Iris9 | Listeners. Rotate their name, scan for the `SYNC` and for each other. |
| Iris5 | Passive observer. Records everything it can hear, as an outsider would. |

The broadcaster and observer firmware started from the nRF Connect SDK Bluetooth sample applications. Serial output from every board was captured with `screen`.

![Sync and rotation sequence](diagrams/sync_rotation_sequence_diagram.png)

## 5. The five strategies

| Test | Rule | Rotation time |
|------|------|---------------|
| 1 | No rotation (baseline) | never |
| 2 | Fixed interval | $t_k = kT_0$, with $T_0 = 60$ s |
| 3 | Random interval | the interval $X$ between rotations is uniform on 60 to 300 s |
| 4 | On the broadcaster's `SYNC` | listener $i$ rotates at $t_k = s_k + \delta_i$ (defined below) |
| 5 | Hybrid: sync, autonomous timer, peer observation | see below |

### Test 3: random intervals

The firmware draws each interval as `MIN + rand() % (MAX - MIN)` in milliseconds, with `MIN` = 60 000 and `MAX` = 300 000. Let $X$ be the interval, uniform over $[T_{\min}, T_{\max})$ with $T_{\min} = 60$ s and $T_{\max} = 300$ s (the firmware's version is discrete, in whole milliseconds, which changes none of the numbers below). The expected interval and its spread are:

$$E[X] = \frac{T_{\min} + T_{\max}}{2} = 180\ \text{s}$$

$$\sigma_X = \frac{T_{\max} - T_{\min}}{\sqrt{12}} \approx 69.3\ \text{s}$$

The uncertainty about the next rotation time, at millisecond resolution, is:

$$H = \log_2(240\,000) \approx 17.9\ \text{bits}$$

At one-second resolution it is $H = \log_2 240 \approx 7.9$ bits. Treat these as per-draw figures for an ideal random source. The firmware seeds `rand()` with `k_uptime_get_32()`, the time since boot, which is nearly the same on every start, so the real unpredictability is lower than 17.9 bits.

### Test 4: rotation on a sync signal

The broadcaster increments a one-byte counter once per broadcast, every 60 s in the Test 4 firmware, and advertises it:

$$n_{k+1} = (n_k + 1) \bmod 255$$

The code uses `% UINT8_MAX`, so the counter takes the values 0 to 254 and never sends 255.

Let $s_k$ be the time the $k$-th `SYNC` is broadcast. A listener that accepts it rotates to its next name. In the final Test 4 listener the number in the name is a local index that goes up by one for each accepted `SYNC` (`Iris6_1`, `Iris6_2`, ...). The broadcaster's counter is used only to decide whether a `SYNC` is new. Each listener's rotation time is $t_k = s_k + \delta_i$, where $\delta_i$ is that listener's own detection and processing delay, so the spread between listeners is:

$$\Delta = \max_i \delta_i - \min_i \delta_i$$

A debounce rule decides whether to accept a `SYNC` with counter $n$ at time $t$. The listener accepts it if the counter has changed or enough time has passed:

$$n \neq n_{\text{last}} \quad \text{or} \quad t - t_{\text{last}} \ge D$$

The minimum gap $D$ is 300 s in the final Test 4 listener (a comment in the code still says 60 s) and 600 s in the Test 5 listener. An earlier Test 4 version used 50 s. Without the rule, duplicate copies of the same `SYNC` could trigger repeated rotations. The Test 5 listener also defines a 5 s `DEBOUNCE_INTERVAL_MS` that is not used.

### Test 5: hybrid

Test 5 combines three rotation triggers, and the code kept in this repository implements two of them.

1. **Sync.** The broadcaster sends a `SYNC` every 10 minutes whose counter is XOR-obfuscated. The listener decrypts it and, if the debounce rule accepts it, takes the decrypted counter as the number in its name.
2. **Autonomous timer.** Every 5 minutes the listener rotates to the next counter, wrapping at 256. This timer is not reset by a `SYNC`, so it runs regardless and adds rotations on top of the synchronised ones. It is not a fallback that only starts when the `SYNC` is missing.
3. **Peer observation.** The design has a listener rotate when it sees another listener rotate, and the Test 5 graph shows such rotations. The listener source kept here only logs the peers it sees, so the code for that trigger is not among the files in this repository.

The obfuscation in the Test 5 broadcaster is a one-byte XOR with a fixed key:

$$c_k = n_k \oplus K, \qquad n_k = c_k \oplus K, \qquad K = \mathtt{0x5A}$$

This is a stand-in for real encryption. A single-byte key has $2^8$ possibilities and the counter starts at a known value, so an observer can recover $K$ immediately. It shows the flow (encrypt, broadcast, decrypt, validate) but does not provide security. AES in counter mode is the obvious replacement and is listed under future work.

There is also a bug in how the Test 5 broadcaster builds its counter. It adds one to the previously *encrypted* byte, not to the plaintext counter:

$$n_{k+1} = (c_k + 1) \bmod 255, \qquad c_{k+1} = n_{k+1} \oplus K$$

So the decrypted counter does not count up by one. It goes 1, 92, 7, 94, 5, 96, 59, and so on. That is why the Test 5 graph has names such as `Iris9_92` and `Iris9_7` instead of a clean sequence. The listener decrypts each value correctly, so synchronisation still works, but the numbers in the names are not a simple count.

## 6. Results

### Measured timing

I measured the intervals from the listeners' own logs:

| Test | What was measured | Expected | Measured |
|------|-------------------|----------|----------|
| 2 | Interval between rotations, 226 intervals from three listeners | exactly 60 s | min 60, max 60, mean 60.0, sd 0 |
| 3 | Interval between rotations, 67 intervals from three listeners | uniform on [60, 300), mean 180 s, sd 69.3 s | min 61, max 299, mean 171.7, sd 71.1 |
| 4 | Gap between detected `SYNC` signals, about 41 per listener | 60 s | min 54, max 61, mean about 60 |

Test 3 matches the uniform model well. The sample mean of 171.7 s is about one standard error ($\sigma_X / \sqrt{67} \approx 8.5$ s) below 180 s, which is ordinary sampling noise. In Test 4 the shorter gap at 54 s is the first one, between the listener starting up and the next broadcast.

### What the observer saw

These come from the observer's capture, which is not published (see [DATA.md](../DATA.md)); the graphs are.

- **Test 1.** A static name was detected for the whole run. The RSSI traces separate the devices clearly, and Iris6 and Iris7 follow each other because they sat close together, while Iris9, the furthest away, does not. Static identity plus signal strength is enough to tell the devices apart and to infer which are near each other.

  ![Test 1](../results/test1/graphs/Test_1_All_Devices_No_Rotation.png)

- **Test 2.** Names changed every 60 seconds, which breaks the static identity. But the schedule is perfectly regular, so the next change can be predicted exactly.

  ![Test 2](../results/test2/graphs/Test2_All_Devices.png)

- **Test 3.** Random intervals remove the predictability of when the next change happens. The name sequence (`Iris6_1`, `Iris6_2`, ...) is still easy to follow.

  ![Test 3](../results/test3/graphs/Test3_All_Devices.png)

- **Test 4.** All three listeners changed together on each `SYNC`. In the observer capture, the first sighting of each listener's new name falls within 1 second of the others at every one of the six rotations I measured (rotations at about 69, 129, 189, 249, 309 and 370 s, roughly 60 s apart). The observer's timestamps have a resolution of 1 second, so $\Delta$ is at most about a second, and could be smaller. So the timing of the changes alone does not tell the three devices apart. That is not the same as the $1/N$ bound, though. The names kept their per-device prefixes (`Iris6_`, `Iris7_`, `Iris9_`), which identify the device whatever the timing, and signal strength continued to tie each new name to the old one. The devices could also recognise each other, because each kept broadcasting its current name while it rotated.

  ![Test 4](../results/test4/graphs/Test4_All_Devices.png)

- **Test 5.** Over a 30-minute run the three triggers were visible separately: synchronised changes at the start and at 10 minutes, autonomous changes every 5 minutes, and peer-triggered changes in between. No rapid back-to-back rotations appear, which fits the debounce rule. The peer-triggered rotations are in the data, but as noted in section 5 the code for that trigger is not among the files kept in this repository. In the graph below, circles are sync-signal rotations, diamonds are peer-observed rotations and squares are autonomous rotations.

  ![Test 5](../results/plotly-exports/newplot-20241208-060345.png)

### Problems found along the way

- The broadcaster is a single point of failure. If it is out of range, the listeners stop receiving `SYNC` and drift apart. Test 5 adds an autonomous timer so the listeners keep rotating even when no `SYNC` arrives.
- Radio is noisy. The same `SYNC` is often heard more than once, and corrupted copies appear (for example `SYNC_1000` when `SYNC_100` was sent). The debounce rule and a small name-normalising function were needed to keep listeners from acting on duplicates and garbage.

## 7. Limitations

- **Name, not address.** The devices rotated the advertised name. The Bluetooth address, which is what real trackers read, did not change in these experiments. The results show that the coordination works, not that an address-level scheme would hold up the same way.
- **Linkable names.** Every device kept a fixed name prefix, so the names themselves identify the device. The $1/N$ argument in section 2.3 is an ideal bound that these experiments do not reach.
- **Weak encryption.** The Test 5 counter is protected only by a single-byte XOR with a fixed key, which is obfuscation, not protection against a real attacker.
- **Weak randomness.** The Test 3 intervals come from `rand()` seeded with the time since boot, so their unpredictability is lower than the ideal 17.9 bits.
- **Test 5 counter.** The decrypted counter does not count up by one, because of how the broadcaster builds it (section 5).
- **Missing source.** The code for peer-triggered rotation in Test 5 is not among the files kept in this repository.
- **Observational evaluation.** The conclusions come from logs and RSSI graphs. I did not implement a tracking attack and measure how often it succeeds.
- **Small, controlled setup.** Five nodes, one location, ambient radio noise from whatever happened to be nearby. Larger or moving scenarios are untested.
- **Signal strength.** As the Test 4 results show, rotating together does not hide devices from an observer who uses RSSI. A scheme that also varies transmit power or timing would be needed to address that.

## 8. Future work

- Rotate the real address by modifying or replacing the stack's privacy handling.
- Replace the XOR counter with AES in counter mode.
- Combine the five tests into one system that can move between strategies.
- Test in larger and moving environments.
- Add a live visualisation of rotations and sync events for debugging.

## 9. Where things are in this repository

| What | Where |
|------|-------|
| Firmware for every test and iteration | [`firmware/`](../firmware) (see its [README](../firmware/README.md)) |
| Log parsing and graphing scripts | [`analysis/`](../analysis) |
| Logs from the project's own nodes, parsed data and graphs | [`results/`](../results) |
| What is and is not published, and why | [`DATA.md`](../DATA.md) |

## 10. Selected prior work

Listed by title, as examples of the research this project builds on.

- Saving private addresses: an analysis of privacy issues in the Bluetooth-Low-Energy advertising mechanism.
- When good becomes evil: tracking Bluetooth Low Energy devices via allowlist-based side channel and its countermeasure.
- Associating the randomized Bluetooth MAC addresses of a device.
- Device discovery and tracing in the Bluetooth Low Energy domain. Computer Communications, vol. 202.
- Device identification in BLE packets from moving devices with randomized MAC addresses. ICCE-Asia 2023.
- A study of MAC address randomization in mobile devices and when it fails. arXiv:1703.02874.
- Privacy challenges with protecting live vehicular location context. IEEE Access, vol. 8.
- DeTagTive: linking MACs to protect against malicious BLE trackers.
- Securing Bluetooth Low Energy networking: an overview of security procedures and threats. Computer Networks, vol. 211.
- Bluetooth Core Specification, including the Randomized RPA Updates amendment.
- Nordic Semiconductor and Zephyr Project: nRF Connect SDK Bluetooth Broadcaster and Observer samples.
