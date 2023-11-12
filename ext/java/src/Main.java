import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

public class Main {
// 3b - 24bit: 1b - len; 2b - back offset-code to prev. decoded / len<0 - not encoded part, -1..-128
    public static int MAX_OFFSET = (1 << 16) + 1; // 0..0xFFFF; 0 - means 1
    public static int getLen(int dOff, final byte[] data, int dOff2) {
        int len = 0;
        int i = dOff;
        int j = dOff2;
        while ( i < data.length && j < dOff && data[i++] == data[j++] ) {
            len += 1;
            if (len == 127) {
                return 127;
            }
        }
        return len;
    }

    public static int get3bCodeLen(final byte[] data, final int dOff, final Dir2LZW.LZWEncoderDescriptor desc) {
        final List<Byte> encoded = desc.encoded;
        final int lastEncodedIdx = encoded.size() - 1;
        if (lastEncodedIdx < 0) {
            desc.eLenIdx = 0;
            encoded.add((byte)-1);
            encoded.add(data[dOff]);
            encoded.add(data[dOff+1]);
            return 2;
        }
        final int leftDoff = Math.max(0, dOff - MAX_OFFSET);
        final byte startByte = data[dOff];
        int len = 0;
        int dOffBest = -1;
        for (int dOff2 = leftDoff; dOff2 < dOff; ++dOff2) {
            if (data[dOff] == startByte) {
                int lenCand = getLen(dOff, data, dOff2);
                if (lenCand >= len) {
                    len = lenCand;
                    dOffBest = dOff2;
                }
            }
        }
        if (len > 3) {
            desc.eLenIdx = -1;
            encoded.add((byte)len);
            final int reverseOffset = dOff - dOffBest;
            encoded.add((byte)(reverseOffset & 0xFF));
            encoded.add((byte)((reverseOffset >> 8) & 0xFF));
            return len;
        } else if (dOff < data.length - 1) {
            if (desc.eLenIdx < 0) {
                desc.eLenIdx = encoded.size();
                encoded.add((byte)-1);
                encoded.add(data[dOff]);
                encoded.add(data[dOff+1]);
                return 2;
            } else {
                final byte pLen = encoded.get(desc.eLenIdx);
                if (pLen == -128) {
                    desc.eLenIdx = encoded.size();
                    encoded.add((byte)-1);
                    encoded.add(data[dOff]);
                    encoded.add(data[dOff+1]);
                    return 2;
                }
                encoded.set(desc.eLenIdx, (byte)(pLen - 1));
                encoded.add(data[dOff]);
                return 1;
            }
        }
        desc.eLenIdx = encoded.size();
        encoded.add((byte)0);
        encoded.add(data[dOff]);
        return 1;
    }
    public static void main(final String[] args) {
        try {
            final byte[] data = Files.readAllBytes(Paths.get(args[0]));
            System.out.printf("%s; sz: %d\n", args[0], data.length);
            final Dir2LZW.LZWEncoderDescriptor desc = new Dir2LZW.LZWEncoderDescriptor();
            desc.encoded = new ArrayList<>();
            for (int i = 0; i < data.length; ) {
                int incSize = get3bCodeLen(data, i, desc);
                i += incSize;
            }
            System.out.printf(" compressed:%d; remainsDecompressed: %d\n", desc.encoded.size(), data.length);
            byte[] arr = new byte[desc.encoded.size()];
            for (int i = 0; i < desc.encoded.size(); ++i) {
                arr[i] = desc.encoded.get(i);
            }
            try (var outputStream = Files.newOutputStream(Paths.get("nes.lzw"))) {
                outputStream.write(arr);
            }
            // TEST:
            {
                final ArrayList<Byte> recovered = printBytes(arr);
                byte[] arr2 = new byte[recovered.size()];
                for (int i = 0; i < recovered.size(); ++i) {
                    arr2[i] = recovered.get(i);
                }
                try (var outputStream = Files.newOutputStream(Paths.get("recovered.nes"))) {
                    outputStream.write(arr2);
                }
            }
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
    static ArrayList<Byte> printBytes(byte[] arr) {
        final ArrayList<Byte> recovered = new ArrayList<>();
        int page = 1;
        for (int i = 0; i < arr.length; ) {
            byte token = arr[i++];
            System.out.printf("token: 0x%02X: ", token);
            if (token <= 0) {
                final int len = Math.abs(token) + 1;
                System.out.printf("len: %d -> ", len);
                for (int j = 0; j < len; ++j) {
                    final var b = arr[i++];
                    System.out.printf("0x%02X ", b);
                    System.out.printf("ro(%d) ", recovered.size());
                    recovered.add(b);
                }
                System.out.println();
            } else {
                System.out.printf("len: %d -> ", token);
                final var nextId = recovered.size();
                int backOff = arr[i++] & 0xFF;
                backOff |= (arr[i++] & 0xFF) << 8;
                if (nextId - backOff < 0) {
                    System.out.printf(" 0x%04X (%d) nextId %d ", backOff, backOff, nextId);
                    return recovered; // TODO: ???
                }
                System.out.printf("0x%04X (%d) -> ", backOff, backOff);

                int j = nextId - backOff;
                for (int k = 0; k < token & j < nextId; ++j, ++k) {
                    System.out.printf("(%d) ", j);
                    byte b  = recovered.get(j);
                    System.out.printf("0x%02X ", b);
                    System.out.printf("ro(%d) ", recovered.size());
                    recovered.add(b);
                }
                System.out.println();
            }
            if (recovered.size() > page * 16*1024) {
                printROM(recovered, page);
                page++;
            }
        }
        System.out.println("");
        return recovered;
    }

    private static void printROM(ArrayList<Byte> recovered, int page) {
        final int len = 16*1024;
        final int romOff = (page - 1) * len;
        for (int i = romOff; i < romOff + len; ++i) {
            if (i % 16 == 0) {
                System.out.printf("\n%d: ", i);
            }
            if (i == 0) {
                System.out.print("  ");
            } else {
                System.out.print(", ");
            }
            if (i >= recovered.size()) {
                System.out.print("EOD");
            } else {
                System.out.printf("0x%02X", recovered.get(i));
            }
        }
        System.out.print("\n");
    }

    static ArrayList<Byte> recoverBytes(byte[] arr) {
        final ArrayList<Byte> recovered = new ArrayList<>();
        for (int i = 0; i < arr.length; ) {
            byte token = arr[i++];
            if (token <= 0) {
                final int len = Math.abs(token) + 1;
                for (int j = 0; j < len; ++j) {
                    final var b = arr[i++];
                    recovered.add(b);
                }
            } else {
                final var nextId = recovered.size();
                int backOff = arr[i++] & 0xFF;
                backOff |= (arr[i++] & 0xFF) << 8;
                if (nextId - backOff < 0) {
                    throw new RuntimeException("nextId - backOff < 0");
                }
                int k = 0;
                for (int j = nextId - backOff; k < token & j < nextId; ++j, ++k) {
                    byte b  = recovered.get(j);
                    recovered.add(b);
                }
            }
        }
        return recovered;
    }

    static List<Byte> toBytes(long t) {
        final List<Byte> buff = new ArrayList<>();
        buff.add((byte)(t >> 24 & 0xFF));
        buff.add((byte)(t >> 16 & 0xFF));
        buff.add((byte)(t >> 8 & 0xFF));
        buff.add((byte)(t & 0xFF));
        return buff;
    }

    static Collection<Byte> toBytes3(int t) {
        final List<Byte> buff = new ArrayList<>();
        buff.add((byte)(t >> 16 & 0xFF));
        buff.add((byte)(t >> 8 & 0xFF));
        buff.add((byte)(t & 0xFF));
        return buff;
    }

    static List<Byte> toBytes3(int szCode, int t) {
        final List<Byte> buff = new ArrayList<>();
        buff.add((byte)((t >> 16 & 0x0F) | (szCode << 4 & 0xF0)));
        buff.add((byte)(t >> 8 & 0xFF));
        buff.add((byte)(t & 0xFF));
        return buff;
    }

    static int convertToInt(byte[] data, int i, int remains) {
        if (remains < 4) {
            throw new RuntimeException("Unexpected size");
        }
        return (data[i]<<24) & 0xff000000 |
                (data[i+1]<<16) & 0x00ff0000 |
                (data[i+2]<< 8) & 0x0000ff00 |
                (data[i+3]) & 0x000000ff;
    }
}
