#!/usr/bin/env python3
"""Tests of tools/ir_convert.py. Run:  python tools/test_ir_convert.py"""
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ir_convert as c  # noqa: E402


def pulse_distance(header, bit_mark, one, zero, value, bits):
    out = list(header)
    for i in range(bits):
        out += [bit_mark, one if (value >> i) & 1 else zero]
    return out + [bit_mark]


def nec_frame(address, command):
    value = address | ((~address & 0xFF) << 8) | (command << 16) | ((~command & 0xFF) << 24)
    return pulse_distance((9000, 4500), 560, 1690, 560, value, 32)


HEADER = "Filetype: IR signals file\nVersion: 1\n"


class Decoding(unittest.TestCase):
    def test_nec_family(self):
        self.assertEqual(c.decode_timings(nec_frame(0x04, 0x08)), ("NEC", 0x04, 0x08))
        ext = pulse_distance((9000, 4500), 560, 1690, 560, 0x08081234, 32)
        self.assertEqual(c.decode_timings(ext), ("NECext", 0x1234, 0x0808))
        # a capture is never exact, and a held key adds repeat marks
        noisy = [d + (37 if i % 2 else -41) for i, d in enumerate(nec_frame(0x04, 0x08))]
        self.assertEqual(c.decode_timings(noisy + [40000, 9000, 2250, 560]), ("NEC", 0x04, 0x08))

    def test_kaseikyo_panasonic_power(self):
        # bytes 02 20 80 00 3D BD: the well-known Panasonic TV power code 40 04 01 00 BC BD
        frame = pulse_distance((3456, 1728), 432, 1296, 432, 0xBD3D00802002, 48)
        self.assertEqual(c.decode_timings(frame), ("Kaseikyo", 0x200280, 0x3D0))

    def test_rc6_same_frame_as_the_firmware_test(self):
        frame = [2666, 889, 444, 888, 444, 444, 444, 444, 444, 888, 888] + [444, 444] * 11
        frame += [444, 888, 444, 444, 888, 444, 444, 444]
        self.assertEqual(c.decode_timings(frame), ("RC6", 0x00, 0x0C))

    def test_rc5(self):
        # S1 1, S2 1, toggle 0, address 00101, command 110101, as half bits (1 = mark); the first space is not seen
        bits = [1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1]
        levels = []
        for b in bits:
            levels += [0, 1] if b else [1, 0]
        levels = levels[1:]
        frame, last = [], None
        for lv in levels:
            if lv == last:
                frame[-1] += 889
            else:
                frame.append(889)
            last = lv
        if len(frame) % 2 == 0:
            frame.pop()
        self.assertEqual(c.decode_timings(frame), ("RC5", 0x05, 0x35))

    def test_sony_three_copies(self):
        def sirc(value, bits):
            out = [2400]
            for i in range(bits):
                out += [600, 1200 if (value >> i) & 1 else 600]
            return out
        frame = sirc(0x15 | (0x01 << 7), 12)
        self.assertEqual(c.decode_timings(frame + [25000] + frame + [25000] + frame), ("SIRC", 0x01, 0x15))

    def test_unknown_timings_stay_raw(self):
        signal = c.make_raw(38000, [1000, 3000, 1000, 500, 300, 700, 1000])
        self.assertEqual(signal[0], "raw")
        # two different messages one after the other are not one parsed signal
        both = nec_frame(0x04, 0x08) + [40000] + nec_frame(0x04, 0x09)
        self.assertEqual(c.make_raw(38000, both)[0], "raw")


class Readers(unittest.TestCase):
    def test_pronto_learned_code(self):
        def word(us):
            return "%04X" % round(us * 38029 / 1e6)
        body = [word(d) for d in nec_frame(0x04, 0x08)] + ["05F0"]
        text = "Power: 0000 006D %04X 0000 %s\n" % (len(body) // 2, " ".join(body))
        text += "Vol up\n6000 0073 0000 0001 0000 0010\n"
        problems = []
        remotes = c.read_pronto(text, "tele", problems)
        self.assertEqual(problems, [])
        self.assertEqual(remotes, [("tele", [("Power", ("parsed", "NEC", 0x04, 0x08)), ("Vol up", ("parsed", "RC6", 0, 0x10))])])

    def test_lirc_space_enc(self):
        text = """
begin remote
  name  LG_TEST
  bits           16
  flags SPACE_ENC|CONST_LENGTH
  header       9000  4500
  one           560  1690
  zero          560   560
  ptrail        560
  pre_data_bits   16
  pre_data       0x20DF
  gap          108000
  begin codes
      KEY_POWER                0x10EF
      KEY_VOLUMEUP             0x40BF
      Input                    0xD02F
  end codes
end remote
"""
        problems = []
        remotes = c.read_lirc(text, "lg", problems)
        self.assertEqual(problems, [])
        self.assertEqual(remotes[0][0], "LG_TEST")
        self.assertEqual(remotes[0][1], [("Power", ("parsed", "NEC", 0x04, 0x08)), ("Vol_up", ("parsed", "NEC", 0x04, 0x02)),
                                         ("Input", ("parsed", "NEC", 0x04, 0x0B))])

    def test_lirc_rc5_and_raw_and_unsupported(self):
        text = """
begin remote
  name philips
  bits 13
  flags RC5|CONST_LENGTH
  one 889 889
  zero 889 889
  plead 889
  frequency 36000
  begin codes
    KEY_POWER 0x100C
  end codes
end remote
begin remote
  name crudo
  flags RAW_CODES
  frequency 38000
  begin raw_codes
    name uno
      1000 3000 1000 500
      300 700 1000
  end raw_codes
end remote
begin remote
  name otro
  flags RCMM
  begin codes
    KEY_POWER 0x1
  end codes
end remote
"""
        problems = []
        remotes = dict(c.read_lirc(text, "f", problems))
        self.assertEqual(remotes["philips"], [("Power", ("parsed", "RC5", 0, 0x0C))])
        self.assertEqual(remotes["crudo"], [("uno", ("raw", 38000, [1000, 3000, 1000, 500, 300, 700, 1000]))])
        self.assertNotIn("otro", remotes)
        self.assertEqual(len(problems), 1)
        self.assertIn("RCMM", problems[0])


class Complement(unittest.TestCase):
    def test_a_remote_the_database_has_is_not_written_and_a_new_one_is(self):
        with tempfile.TemporaryDirectory() as tmp:
            irdb, src, out = (os.path.join(tmp, n) for n in ("irdb", "src", "out"))
            os.makedirs(os.path.join(irdb, "TVs", "LG"))
            os.makedirs(src)
            with open(os.path.join(irdb, "TVs", "LG", "LG_TV.ir"), "w") as f:
                f.write(HEADER + "#\nname: Power\ntype: parsed\nprotocol: NEC\naddress: 04 00 00 00\ncommand: 08 00 00 00\n"
                        "#\nname: Odd\ntype: raw\nfrequency: 38000\nduty_cycle: 0.33\ndata: 1000 3000 1000 500 300 700 1000\n")
            # the same two signals as a capture would give them: other names, timings a little off
            raw_nec = " ".join(str(d + 30) for d in nec_frame(0x04, 0x08))
            with open(os.path.join(src, "mine.ir"), "w") as f:
                f.write(HEADER + "#\nname: On\ntype: raw\nfrequency: 38000\nduty_cycle: 0.33\ndata: " + raw_nec + "\n"
                        "#\nname: Other\ntype: raw\nfrequency: 38000\nduty_cycle: 0.33\ndata: 1040 2950 980 520 310 690 1010\n")
            with open(os.path.join(src, "new.ir"), "w") as f:
                f.write(HEADER + "#\nname: Power\ntype: parsed\nprotocol: NEC\naddress: 04 00 00 00\ncommand: 08 00 00 00\n"
                        "#\nname: Menu\ntype: parsed\nprotocol: NEC\naddress: 40 00 00 00\ncommand: 12 00 00 00\n")
            self.assertEqual(c.main([src, "-o", out, "--irdb", irdb]), 0)
            self.assertFalse(os.path.exists(os.path.join(out, "mine.ir")))
            text = c.read_text(os.path.join(out, "new.ir"))
            self.assertIn("name: Menu", text)
            self.assertIn("address: 40 00 00 00", text)
            self.assertEqual(c.main([src, "-o", out, "--irdb", irdb, "--keep-known"]), 0)
            self.assertTrue(os.path.exists(os.path.join(out, "mine.ir")))
            self.assertIn("protocol: NEC", c.read_text(os.path.join(out, "mine.ir")))  # the raw capture became parsed


if __name__ == "__main__":
    unittest.main()
