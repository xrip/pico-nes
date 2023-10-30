import java.io.*;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

public class Dir2LZW {
    static class LZWEncoderDescriptor {
        int eLenIdx = -1;
        List<Byte> encoded;
    }
    record LZWFileDescriptor(String name, int compressedLength, int decompressedLength, LZWEncoderDescriptor compressed) {}

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
    public static int get3bCodeLen(final byte[] data, final int dOff, final LZWEncoderDescriptor desc) {
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
    static LZWEncoderDescriptor encode(final byte[] data) {
        final LZWEncoderDescriptor desc = new LZWEncoderDescriptor();
        desc.encoded = new ArrayList<>();
        for (int i = 0; i < data.length; ) {
            int incSize = get3bCodeLen(data, i, desc);
            i += incSize;
        }
        return desc;
    }

    public static void main(final String[] args) {
        try {

            final File folder = new File(args[0]);
            final File[] listOfFiles = folder.listFiles();
            assert listOfFiles != null;

            System.out.println("// Files:");
            int size = 4; // (int) number of files
            int maxFile = 0;
            final int blockSize = 4 * 1024;
            final List<LZWFileDescriptor> descriptors = new ArrayList<>();
            String firstFileName = null;
            byte[] firstFileData = null;
            for (File file : listOfFiles) {
                if (file.isFile()) {
                    final String fn = file.getName();
                    if (firstFileName == null)
                        firstFileName = "\\NES\\" + fn;
                    final byte[] data = Files.readAllBytes(Paths.get(args[0] + "\\" + fn));

                    if (firstFileData == null)
                        firstFileData = data;
                    if (maxFile < data.length)
                        maxFile = data.length;
                    final LZWEncoderDescriptor encodedDesc = encode(data);
                    int compressedSize = encodedDesc.encoded.size();
                    descriptors.add(new LZWFileDescriptor(fn, compressedSize, data.length, encodedDesc));
                    System.out.println("// " + fn + "; cz: " + compressedSize + "; uz: " + data.length + "; cz: " + compressedSize);
                    size += fn.length() + compressedSize + 9;
                }
            }

            int maxFile2 = (maxFile >>> 12) << 12;
            if (maxFile2 != maxFile) maxFile2 += 4096;
            System.out.printf(
                    "\n// size: %dK (0x%X); maxFile: %dK (0x%X), sum: %dK\n",
                    size / 1024, size, maxFile2 / 1024, maxFile2, (size + maxFile2 + 570000/2 + 4096) / 1024
            );

            System.out.print("const unsigned char __in_flash() __aligned(4096) rom_filename[4096] = {");
            assert firstFileName != null;
            final byte[] fnb = new byte[4096]; int t = 0;
            for(var b : firstFileName.getBytes()) {
                fnb[t++] = b;
            }
            fnb[255] = 0;
            printBytes(fnb); System.out.println("};");
            System.out.printf("const unsigned char __in_flash() __aligned(4096) rom[0x%X] = {", maxFile2);
            printBytes(firstFileData); System.out.println("};");
            byte[] arr = new byte[size];
            int offset = toBytes(arr, 0, descriptors.size()); // 3 - count, 2-0 - offset to props
            for (final var d : descriptors) {
                offset = toBytes(arr, offset, d.name);
                offset = toBytes(arr, offset, d.compressedLength);
                offset = toBytes(arr, offset, d.decompressedLength);
            }
            Dir2LZMA.to3Bytes(arr, offset); // 2-0 - offset to props
            for (final var d : descriptors) {
                offset = toBytes(arr, offset, d.compressed.encoded);
            }
            try (var outputStream = Files.newOutputStream(Paths.get("nes.lzw"))) {
                outputStream.write(arr);
            }
            System.out.print("const unsigned char __in_flash() __aligned(4096) lz4source [] = {");
            printBytes(arr);
            System.out.println("};");

        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    static void printBytes(byte[] arr) {
        for (int i = 0; i < arr.length; ++i) {
            if (i % 16 == 0) {
                System.out.println("");
            }
            if (i == 0) {
                System.out.print("  ");
            } else {
                System.out.print(", ");
            }
            System.out.printf("0x%02X", arr[i]);
        }
        System.out.println("");
    }
    private static int toBytes(byte[] arr, int offset, final List<Byte> compressed) {
        for (int i = 0; i < compressed.size(); ++i) {
            arr[offset++] = compressed.get(i);
        }
        return offset;
    }
    static int toBytes(byte[] arr, int offset, long t) {
        arr[offset++] = (byte)(t >> 24 & 0xFF);
        arr[offset++] = (byte)(t >> 16 & 0xFF);
        arr[offset++] = (byte)(t >> 8 & 0xFF);
        arr[offset++] = (byte)(t & 0xFF);
        return offset;
    }
    static int toBytes(byte[] arr, int offset, String t) throws UnsupportedEncodingException {
        final var bb = t.getBytes("koi8-r");
        for (final var b : bb) {
            arr[offset++] = b;
        }
        arr[offset++] = (byte)0;
        return offset;
    }
}
