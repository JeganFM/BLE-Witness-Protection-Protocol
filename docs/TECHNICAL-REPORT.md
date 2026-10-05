# Synchronized Identity Rotation for BLE Devices

Technical report for the BLE Witness Protection Protocol project.

## Summary

Bluetooth Low Energy (BLE) devices advertise an identifier that any nearby scanner can read. If that identifier never changes, the device can be followed. BLE already lets devices change their address over time, but the changes are not coordinated, and the timing and signal strength of a device can still give it away. This project asks a narrow question: if several devices that belong together change their identity at the same moment, can they hide among each other?

I built a small testbed of five nRF52840 boards, wrote firmware for five identity-rotation strategies, ran each one while a passive observer recorded what an outsider could see, and compared the results. The main finding is that coordinating the change works: on a shared sync signal the three listeners switched identity together, every time. The main limitation is that the devices rotated their advertised name instead of their Bluetooth address, so the results show the coordination mechanism but are not a full privacy guarantee.

## 1. Motivation

BLE is everywhere: fitness trackers, earbuds, medical sensors, car keys, door locks, asset tags. More than three billion BLE devices shipped in 2022 alone. Almost all of them broadcast something to the world, and any ordinary scanner can log it.

If the broadcast identifier is fixed, a scanner can build a record of where a device has been and when. The device's owner never has to connect to anything or agree to anything. That is the tracking problem, and it is the reason the Bluetooth specification introduced Resolvable Private Addresses (RPAs): an address that looks random to outsiders, changes regularly, and can still be recognised by a trusted peer.

RPAs help, but published research has shown that they are not enough on their own:

- Devices often keep the same advertising interval, payload or signal-strength pattern across an address change, so an observer can match the old and new address.
- If the central and the peripheral change addresses at different moments, the gap between the two changes can be used to link them.
- Many cheap or old devices never rotate at all.

### Why "witness protection"

The name comes from the real thing. When a person has to disappear, they are given a new name and moved somewhere new, so that anyone who knew the old identity can no longer find them. The idea here is the same: a device that must not be followed gets a new name, and the old one stops being used.

The comparison also predicted one of the results. A new name does not help much if the person keeps living at the same address, because anyone watching the house can see who walks in and out. For a BLE device the equivalent of the address is where it is, which an observer can estimate from signal strength. That is what Test 4 showed: the names changed together, but signal strength still tied each new name to the old one. Changing identity and location together is the harder part, and it is the direction I would take next.

### Why coordination

Most of the work on coordinating address changes looks at a single pair of devices. Real setups are not pairs. A phone talks to a watch, earbuds and a tracker at once, and a car has many sensors. So the question I wanted to test on real hardware was how a group of peripherals can change identity together, and what it costs when they do. I chose to build and measure it rather than simulate it, because timing and signal strength are exactly the parts that simulations tend to get wrong.

## 2. Background and the math

### 2.1 How an RPA is built

A Resolvable Private Address is 48 bits long. It is made of a 24-bit random part and a 24-bit hash:

$$\text{RPA} = \text{prand} \,\|\, \text{hash}, \qquad \text{hash} = \text{ah}(\text{IRK},\, \text{prand})$$

`prand` has its top two bits fixed (to `01`, which marks the address as resolvable), so 22 bits are random. The hash function `ah` is one AES-128 encryption of the padded `prand` under the 128-bit Identity Resolving Key (IRK), keeping the lowest 24 bits:

$$\text{ah}(k, r) = e\big(k,\; 0^{104} \,\|\, r\big) \bmod 2^{24}$$

A peer that holds the IRK recomputes the hash and compares. A device that does not hold the IRK cannot do this, so to it the address is just 48 random-looking bits. A wrong IRK matches by chance with probability $2^{-24}$. The specification suggests changing the address about every 15 minutes by default.

### 2.2 Why rotation alone leaks

An observer sees a stream of (time, identifier, signal strength) records. If a device rotates every $T$ seconds on a fixed schedule, then after one observed rotation the observer knows every future rotation time. The uncertainty about the next rotation time, in bits, is zero:

$$H(\text{next rotation}) = 0 \quad \text{when the interval is fixed.}$$

When an identifier disappears at time $t$ and a new one appears at $t+\varepsilon$ with a similar signal strength, linking them is easy for small $\varepsilon$. The same is true of the received signal strength, which falls with distance. The standard log-distance path-loss model is:

$$\text{RSSI}(d) = \text{RSSI}(d_0) - 10\,n\,\log_{10}\!\left(\frac{d}{d_0}\right) + X_\sigma$$

where $d$ is the distance, $d_0$ a reference distance, $n$ the path-loss exponent of the environment and $X_\sigma$ random noise. I did not fit this model to the data. It is here to explain why two devices at similar distances from an observer have similar RSSI traces, which is exactly what the Test 1 graph shows.

### 2.3 The effect of rotating together

Suppose $N$ devices rotate at the same instant, and the observer uses only the timing of the changes. The observer sees $N$ old identities vanish and $N$ new ones appear. There are $N!$ ways to pair them, so a blind guess for one device is right with probability:

$$P(\text{correct link}) = \frac{1}{N}$$

If the devices rotate at different moments, the observer can pair each old identity with the new one that follows it, and the probability goes to 1. This is the reason for synchronising. Signal strength is the side information that weakens this argument, and the experiments below show that it does.

## 3. Approach: rotate the advertised name

Changing the Bluetooth address itself would have meant changing how Zephyr's Bluetooth stack manages its built-in privacy features. That code is tightly integrated with the stack and the specification, and modifying it risks breaking things that are hard to debug. Instead the devices rotate the advertised device name, which is a field in the advertising packet that can be changed safely at run time.

A name such as `Iris9_3` then plays the role of the address. The name sequence is $\text{ID}_k = \text{prefix} \,\|\, n_k$ for a counter $n_k$. This keeps the experiment about the coordination logic, which is what I wanted to measure. It also means the results do not transfer one-for-one to address rotation, and I come back to that in the limitations.

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
| 2 | Fixed interval | $t_k = kT$, $T = 60$ s |
| 3 | Random interval | $t_{k+1} - t_k \sim U[60, 300)$ s |
| 4 | On the broadcaster's `SYNC` | $t_k = s_k + \delta_i$ for listener $i$ |
| 5 | Hybrid: sync, autonomous fallback, peer observation | see below |

### Test 3: random intervals

The firmware draws each interval as `MIN + rand() % (MAX - MIN)` in milliseconds, with `MIN` = 60 000 and `MAX` = 300 000. That is a discrete uniform distribution, so the expected interval, its spread and the uncertainty about the next rotation are:

$$E[X] = \frac{T_{\min} + T_{\max}}{2} = 180\ \text{s}, \qquad \sigma = \frac{T_{\max} - T_{\min}}{\sqrt{12}} \approx 69.3\ \text{s}, \qquad H = \log_2(240\,000) \approx 17.9\ \text{bits}$$

(at millisecond resolution; at one-second resolution $H = \log_2 240 \approx 7.9$ bits).

### Test 4: rotation on a sync signal

The broadcaster increments a one-byte counter each period $P$ and advertises it:

$$n_{k+1} = (n_k + 1) \bmod 255$$

A listener that decodes a valid `SYNC` rotates to $\text{prefix} \,\|\, n_k$. Each listener's rotation time is the broadcast time plus its own detection and processing delay $\delta_i$, so the spread between listeners is:

$$\Delta = \max_i \delta_i - \min_i \delta_i$$

A debounce rule drops a repeated `SYNC` that arrives within $D$ seconds of the last accepted one (accept if $t - t_{\text{last}} \ge D$, with $D = 5$ s). Without it, duplicate copies of the same `SYNC` can trigger repeated rotations.

### Test 5: hybrid

Test 5 has three rotation triggers. The broadcaster sends a `SYNC` every 10 minutes whose counter is encrypted. A listener that has heard no `SYNC` rotates by itself after a fallback interval (5 minutes in the final version). And a listener that sees another listener rotate adjusts its own sequence to match. The encryption in the Test 5 broadcaster is a one-byte XOR:

$$c_k = n_k \oplus K, \qquad n_k = c_k \oplus K, \qquad K = \texttt{0x5A}$$

This is a stand-in for real encryption. A single-byte key has $2^8$ possibilities and the counter starts at a known value, so an observer can recover $K$ immediately. It shows the flow (encrypt, broadcast, decrypt, validate) but does not provide security. AES in counter mode is the obvious replacement and is listed under future work.

## 6. Results

### Measured timing

I measured the intervals from the listeners' own logs:

| Test | What was measured | Expected | Measured |
|------|-------------------|----------|----------|
| 2 | Interval between rotations, 226 intervals from three listeners | exactly 60 s | min 60, max 60, mean 60.0, sd 0 |
| 3 | Interval between rotations, 67 intervals from three listeners | uniform on [60, 300), mean 180 s, sd 69.3 s | min 61, max 299, mean 171.7, sd 71.1 |
| 4 | Gap between detected `SYNC` signals, about 41 per listener | 60 s | min 54, max 61, mean about 60 |

Test 3 matches the uniform model well, with the sample mean slightly below 180 s as expected for 67 draws. In Test 4 the shorter gap at 54 s is the first one, between the listener starting up and the next broadcast.

### What the observer saw

These come from the observer's capture, which is not published (see [DATA.md](../DATA.md)); the graphs are.

- **Test 1.** A static name was detected for the whole run. The RSSI traces separate the devices clearly, and Iris6 and Iris7 follow each other because they sat close together, while Iris9, the furthest away, does not. Static identity plus signal strength is enough to tell the devices apart and to infer which are near each other.

  ![Test 1](../results/test1/graphs/Test_1_All_Devices_No_Rotation.png)

- **Test 2.** Names changed every 60 seconds, which breaks the static identity. But the schedule is perfectly regular, so the next change can be predicted exactly.

  ![Test 2](../results/test2/graphs/Test2_All_Devices.png)

- **Test 3.** Random intervals remove the predictability of when the next change happens. The name sequence (`Iris6_1`, `Iris6_2`, ...) is still easy to follow.

  ![Test 3](../results/test3/graphs/Test3_All_Devices.png)

- **Test 4.** All three listeners changed together on each `SYNC`. In the observer capture, the first sighting of each listener's new name falls within 1 second of the others at every one of the six rotations I measured (rotations at about 69, 129, 189, 249, 309 and 370 s, roughly 60 s apart). The observer's timestamps have a resolution of 1 second, so $\Delta$ is at most about a second, and could be smaller. That makes the timing-only guess close to $1/N$. But the devices could still recognise each other, because each kept broadcasting its current name while it rotated, and signal strength continued to tie each new name to the old one.

  ![Test 4](../results/test4/graphs/Test4_All_Devices.png)

- **Test 5.** Over a 30-minute run the three triggers were visible separately: synchronised changes at the start and at 10 minutes, autonomous changes every 5 minutes, and peer-triggered changes in between. The debounce rule suppressed duplicate `SYNC` signals, so no rapid back-to-back rotations appear.

### Problems found along the way

- The broadcaster is a single point of failure. If it is out of range, the listeners stop receiving `SYNC` and drift apart. The autonomous fallback in Test 5 was added for this reason.
- Radio is noisy. The same `SYNC` is often heard more than once, and corrupted copies appear (for example `SYNC_1000` when `SYNC_100` was sent). The debounce rule and a small name-normalising function were needed to keep listeners from acting on duplicates and garbage.

## 7. Limitations

- **Name, not address.** The devices rotated the advertised name. The Bluetooth address, which is what real trackers read, did not change in these experiments. The results show that the coordination works, not that an address-level scheme would hold up the same way.
- **Weak encryption.** The Test 5 counter is protected only by a single-byte XOR.
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
