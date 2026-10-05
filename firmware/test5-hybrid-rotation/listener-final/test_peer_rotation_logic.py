# Model of the peer-rotation debounce check in src/main.c (device_found).
# No nRF Connect SDK / hardware here, so this checks the decision logic
# only, not the firmware build.
#
# if (now - last_rotation_time >= DEBOUNCE_INTERVAL_MS) { rotate(); }

DEBOUNCE_INTERVAL_MS = 5000


class ListenerModel:
    def __init__(self):
        self.last_rotation_time = 0
        self.counter = 0
        self.rotations = []

    def peer_seen(self, now_ms):
        if now_ms - self.last_rotation_time >= DEBOUNCE_INTERVAL_MS:
            self.counter = (self.counter + 1) % 256
            self.last_rotation_time = now_ms
            self.rotations.append((now_ms, self.counter))
            return True
        return False


def demo():
    # a burst of peers within the debounce window should rotate once, not three times
    m = ListenerModel()
    m.last_rotation_time = -DEBOUNCE_INTERVAL_MS
    assert m.peer_seen(0) is True
    assert m.peer_seen(1000) is False
    assert m.peer_seen(2000) is False
    assert m.rotations == [(0, 1)], m.rotations

    # next sighting after the window rotates again
    assert m.peer_seen(4999) is False
    assert m.peer_seen(5000) is True
    assert m.rotations == [(0, 1), (5000, 2)], m.rotations

    # counter wraps at 256
    m2 = ListenerModel()
    m2.counter = 255
    m2.last_rotation_time = -DEBOUNCE_INTERVAL_MS
    assert m2.peer_seen(0) is True
    assert m2.counter == 0, m2.counter

    print("ok")


if __name__ == "__main__":
    demo()
