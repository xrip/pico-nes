import net.jpountz.lz4.*;

import java.io.*;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

public class Dir2lz4 {
/*
    static class OutStream extends OutputStream {
        final List<Byte> compressed = new ArrayList<>();
        @Override
        public void write(int b) {
            compressed.add((byte)(b & 0xFF));
        }
        public byte[] getBytes() {
            final byte[] res = new byte[compressed.size()];
            for (int i = 0; i < compressed.size(); ++i) {
                res[i] = compressed.get(i);
            }
            return res;
        }
    }
    static class InStream extends InputStream {
        final OutStream os;
        int offset = 0;
        InStream(OutStream os) {
            this.os = os;
        }
        @Override
        public int read() throws IOException {
            return os.compressed.get(offset++) & 0xFF;
        }
    }*/
    record LZ4Block(byte[] block, int compressed, int decompressed) {}
    record LZ4FileDescriptor(String name, int compressedLength, int decompressedLength, List<LZ4Block> compressed) {}

    public static void main(final String[] args) {
        try {
            final LZ4Factory factory = LZ4Factory.fastestInstance();
            final LZ4Compressor compressor = factory.fastCompressor();
            final LZ4Decompressor decompressor = factory.decompressor();

            final File folder = new File(args[0]);
            final File[] listOfFiles = folder.listFiles();
            assert listOfFiles != null;

            System.out.println("// Files:");
            int size = 4; // (int) number of files
            int maxFile = 0;
            final int blockSize = 4 * 1024;
            final List<LZ4FileDescriptor> descriptors = new ArrayList<>();
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
                    int compressedSize = 0;
                    final List<LZ4Block> blocks = new ArrayList<>();
                    for (int i = 0; i < data.length; i += blockSize) {
                        final int decompressedLength = Math.min(blockSize, data.length - i);
                        final int maxCompressedLength = compressor.maxCompressedLength(decompressedLength);
                        final byte[] compressed = new byte[maxCompressedLength];
                        final int compressedLength = compressor.compress(
                                data, i, decompressedLength, compressed, 0, maxCompressedLength
                        );
                        compressedSize += compressedLength;
                        blocks.add(new LZ4Block(compressed, compressedLength, decompressedLength));
                        // TEST
                        {
                            byte[] restored = new byte[decompressedLength];
                            final int read = decompressor.decompress(compressed, 0, restored, 0, decompressedLength);
                            if (read != compressedLength) {
                                throw new RuntimeException("Unexpected result. Actual read: " + read + "; " + fn + "; sz:" + compressedLength + "; #: " + i);
                            }
                            for (int j = 0; j < decompressedLength; ++j) {
                                if (restored[j] != data[i + j]) {
                                    throw new RuntimeException("Unexpected result. Deep cmp: " + j + "; " + fn + "; i: " + i);
                                }
                            }
                        }
                    }
                    compressedSize += blocks.size() * 8;
                    descriptors.add(new LZ4FileDescriptor(fn, compressedSize, data.length, blocks));
                    System.out.println("// " + fn + "; cz: " + compressedSize + "; uz: " + data.length + "; bz: " + blocks.size());
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
                offset = toBytes(arr, offset, d.compressed);
            }
            try (var outputStream = Files.newOutputStream(Paths.get("nes.lz4"))) {
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
    private static int toBytes(byte[] arr, int offset, final List<LZ4Block> compressed) {
        for (final var c : compressed) {
            offset = toBytes(arr, offset, c.compressed);
            offset = toBytes(arr, offset, c.decompressed);
            for (int i = 0; i < c.compressed; ++i) {
                arr[offset++] = c.block[i];
            }
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
