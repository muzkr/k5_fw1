from serial import Serial
from datetime import datetime
import msg as mm

DUMP_CONFIG = 1
DUMP_CALIB = 2
DUMP_ALL = 0xFF


class EepromDump:

    def __init__(self, ser: Serial, dump_what: int, dump_file: str):
        self._ser = ser
        self._dump_what = dump_what
        self._dump_file = dump_file
        self._state = _Init(self)
        # self._dev_info = None

    def loop(self) -> bool:
        next = self._state.loop()
        if isinstance(next, bool):
            return next
        elif next:
            self._state = next

        return True


class _DevInfo:

    def __init__(self):
        self.ver = None
        self.has_AES_key = False
        self.lock_screen = False
        self.AES_challenge = None


class _State:
    def __init__(self, dump: EepromDump):
        self.dump = dump
        self.ser = dump._ser
        self.rx_buf = bytearray(256)
        self.msg_buf = bytearray()

    def loop(self) -> bool | object:
        raise NotImplementedError()

    def send_msg(self, msg: mm.Msg):
        pack = mm.make_packet(msg.buf)
        ser = self.dump._ser
        ser.write(pack)
        ser.flush()

    def recv_msg(self) -> mm.Msg:
        self._rx()
        return mm.fetch(self.msg_buf)

    def _rx(self) -> int:

        len1 = 0
        buf = self.rx_buf
        while True:
            len2 = self.ser.readinto(buf)
            if len2 > 0:
                self.msg_buf.extend(memoryview(buf)[:len2])
                len1 += len2
            if len2 < len(buf):
                break

        return len1


class _Init(_State):
    def __init__(self, dump):
        super().__init__(dump)

    def loop(self) -> _State:
        if self._rx():
            print(".", end="")
            return self

        print()
        return _DeviceInfo(self.dump)

    def _rx(self) -> int:
        return self.ser.readinto(self.rx_buf)


class _DeviceInfo(_State):

    def __init__(self, dump):
        super().__init__(dump)
        self.expect_resp = False
        self.timestamp = 0

    def loop(self) -> _State:

        if not self.expect_resp:
            print("Examing device info..")
            self.send_request()
            self.expect_resp = True
            return

        msg = self.recv_msg()
        if not msg:
            return

        if 0x0515 != msg.get_msg_type():
            return

        # version string
        end = msg.buf.find(b"\0", 4, 20)
        if -1 == end:
            end = 20
        ver = msg.buf[4:end].decode("ascii")

        has_AES_key = msg.buf[20]
        lock_screen = msg.buf[21]
        AES_challenge = (
            msg.get_word_LE(24),
            msg.get_word_LE(28),
            msg.get_word_LE(32),
            msg.get_word_LE(36),
        )

        dev_info = _DevInfo()
        dev_info.ver = ver
        dev_info.has_AES_key = has_AES_key
        dev_info.lock_screen = lock_screen
        dev_info.AES_challenge = AES_challenge
        # self.dump._dev_info = dev_info

        print(
            f"Device info: version = '{ver}', AES key = {has_AES_key}, lock screen = {lock_screen}"
        )
        print(
            f"AES challenge: {AES_challenge[0]:08x} {AES_challenge[1]:08x} {AES_challenge[2]:08x} {AES_challenge[3]:08x}"
        )

        return _AccessRequest(self.dump, dev_info, self.timestamp)

    def send_request(self):

        ts = int(datetime.now().timestamp()) & 0xFFFFFFFF
        self.timestamp = ts

        msg = mm.Msg(8)
        msg.set_msg_type(0x0514)
        msg.set_word_LE(4, ts)
        self.send_msg(msg)


class _AccessRequest(_State):

    def __init__(self, dump, dev_info: _DevInfo, timestamp: int):
        super().__init__(dump)
        self.dev_info = dev_info
        self.timestamp = timestamp
        self.expect_resp = False

    def loop(self) -> _State | bool:

        if not self.expect_resp:
            print("Obtaining access permission..")
            # TODO: AES challenge ..
            AES_resp = [0, 0, 0, 0]
            self.send_request(AES_resp)
            self.expect_resp = True
            return

        msg = self.recv_msg()
        if not msg:
            return

        if 0x052E != msg.get_msg_type():
            return

        err = msg.buf[4]
        if err:
            print("Access rejected")
            return False

        print("Access granted")

        try:
            return _DumpEeprom(self.dump, self.timestamp)
        except:
            #
            return False

    def send_request(self, AES_resp):
        msg = mm.Msg(20)
        msg.set_msg_type(0x052D)
        msg.set_word_LE(4, AES_resp[0])
        msg.set_word_LE(8, AES_resp[1])
        msg.set_word_LE(12, AES_resp[2])
        msg.set_word_LE(16, AES_resp[3])
        self.send_msg(msg)


class _DumpEeprom(_State):

    def __init__(self, dump: EepromDump, timestamp: int):
        super().__init__(dump)
        self.timestamp = timestamp

        what = dump._dump_what
        if DUMP_CONFIG == what:
            off = 0
            size = 0x1E00
        elif DUMP_CALIB == what:
            off = 0x1E00
            size = 0x2000 - 0x1E00
        else:
            off = 0
            size = 0x2000

        self.offset = off
        self.size = size

        data = bytearray()
        self.data = data

        file = dump._dump_file
        try:
            data1 = open(file, "rb").read()
        except Exception as e:
            print("Error loading dump file: " + str(e))
            raise OSError()

        if len(data1) != size:
            print(
                "Dump file size error: expect {} actually {}".format(size, len(data1))
            )
            raise OSError()

        data.extend(data1)

        self.expect_resp = False
        self.AES_key = None

    def loop(self) -> bool | _State:

        if not self.expect_resp:

            # AES key
            if 0 == self.size and self.AES_key is not None:
                print("Writting data.. 100%")
                self.send_request(0x0F30, self.AES_key)
                self.expect_resp = True
                return

            off1 = len(self.data) - self.size

            # Check AES key
            if 0x0F30 == self.offset:
                self.AES_key = self.data[off1 : off1 + 16]
                self.offset += 16
                self.size -= 16
                return
            else:
                per = off1 * 100 // len(self.data)
                print(f"Writting data.. {per}%")
                self.send_request(self.offset, self.data[off1 : off1 + 16])
                self.expect_resp = True
                return

        # Receive resposne ----------

        msg = self.recv_msg()
        if not msg:
            return

        if 0x051E != msg.get_msg_type():
            return

        off = msg.get_hw_LE(4)

        # AES key
        if 0 == self.size and (self.AES_key is not None):
            if off != 0x0F30:
                print("Invalid response. Retry..")
                # print(f"{off:04x}")
                self.expect_resp = False
                return
            else:
                # Mark as done
                self.AES_key = None
        else:
            if off != self.offset:
                print("Invalid response. Retry..")
                self.expect_resp = False
                return
            else:
                self.offset += 16
                self.size -= 16

        self.expect_resp = False

        if self.size > 0 or self.AES_key is not None:
            return

        # Finished ------

        print("Done")
        return _Reboot(self.dump)

    def send_request(self, off: int, data: bytes):

        msg = mm.Msg(28)
        msg.set_msg_type(0x051D)
        msg.set_hw_LE(4, off)
        msg.set_hw_LE(6, 16)  # size
        msg.buf[7] = 1  # allow password
        msg.set_word_LE(8, self.timestamp)
        msg.buf[12:28] = data[:16]
        self.send_msg(msg)


class _Reboot(_State):

    def __init__(self, dump):
        super().__init__(dump)

    def loop(self) -> bool:

        print("Rebooting device..")

        msg = mm.Msg(4)
        msg.set_msg_type(0x05DD)
        self.send_msg(msg)
        return False
