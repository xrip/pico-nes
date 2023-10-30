import SevenZip.Compression.LZMA.Decoder;
import SevenZip.Compression.LZMA.Encoder;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.UnsupportedEncodingException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

public class Dir2LZMA {

    record LZMABlock(int len, byte[] block) {}
    record LZMAFileDescriptor(String name, int compressedLength, int decompressedLength, List<LZMABlock> compressed) {}

    public static void main(final String[] args) {
        try {
            final File folder = new File(args[0]);
            final File[] listOfFiles = folder.listFiles();
            assert listOfFiles != null;

            final ByteArrayOutputStream propertiesStream = new ByteArrayOutputStream();
            final Encoder compressor = new Encoder();
            compressor.SetDictionarySize(4*1024);
            compressor.SetEndMarkerMode(true); // by using an end marker, the uncompressed size doesn't need to be stored with the compressed data
            compressor.WriteCoderProperties(propertiesStream);
            final byte[] props = propertiesStream.toByteArray();
            final Decoder decompressor = new Decoder();
            decompressor.SetDecoderProperties(props);

            System.out.println("// Files:");
            final List<LZMAFileDescriptor> files = new ArrayList<>();
            int size = 4; // (int) number of files
            int maxFile = 0;
            final int blockSize = 8 * 1024;
            for (File file : listOfFiles) {
                if (file.isFile()) {
                    final String fn = file.getName();
                    final byte[] data = Files.readAllBytes(Paths.get(args[0] + "\\" + fn));
                    if (maxFile < data.length)
                        maxFile = data.length;
                    int compressedLength = 0;
                    final List<LZMABlock> compressedBlocks = new ArrayList<>();
                    for (int i = 0; i < data.length; i += blockSize) {
                        final var remains = Math.min(data.length - i, blockSize);
                        final ByteArrayInputStream inputStream = new ByteArrayInputStream(data, i, remains);
                        final ByteArrayOutputStream compressedStream = new ByteArrayOutputStream();
                        compressor.Code(inputStream, compressedStream, -1, -1, null);
                        byte[] compressedArray = compressedStream.toByteArray();
                        compressedBlocks.add(new LZMABlock(compressedArray.length, compressedArray));
                        compressedLength += compressedArray.length;
                        // TEST:
                        {
                            final ByteArrayInputStream inputStreamD = new ByteArrayInputStream(compressedArray);
                            final ByteArrayOutputStream decompressedStream = new ByteArrayOutputStream();
                            final var res = decompressor.Code(inputStreamD, decompressedStream, remains);
                            final var dest = decompressedStream.toByteArray();
                            if (!res) {
                                throw new RuntimeException("Unexpected result");
                            }
                            for (int j = 0; j < remains; ++j) {
                                if (data[i + j] != dest[j]) {
                                    throw new RuntimeException("Unexpected result2");
                                }
                            }
                        }
                    }
                    files.add(new LZMAFileDescriptor(fn, compressedLength, data.length, compressedBlocks));
                    System.out.println("// " + fn + "; cz: " + compressedLength + "; uz: " + data.length + "; bz: " + compressedBlocks.size());
                    size += props.length + fn.length() + compressedLength + 9;
                }
            }
            System.out.printf(
                    "\n// size: %dK (0x%X); maxFile: %dK (0x%X), sum: %dK\n",
                    size / 1024, size, maxFile / 1024, maxFile, (size + maxFile + 570000/2 + 4096) / 1024
            );
            System.out.println("const char __in_flash() __aligned(4096) rom_filename[4096] = {0};");
            System.out.printf("const char __in_flash() __aligned(4096) rom[0x%X] = {0};\n\n", maxFile);
            byte[] arr = new byte[size];
            int offset = toBytes(arr, 0, files.size()); // 3 - count, 2-0 - offset to props
            for (final var d : files) {
                offset = toBytes(arr, offset, d.name);
                offset = toBytes(arr, offset, d.compressedLength);
                offset = toBytes(arr, offset, d.decompressedLength);
            }
            to3Bytes(arr, offset); // 2-0 - offset to props
            for (final var d : props) { // Props len: 20
                offset = toBytes(arr, offset, d);
            }
            for (final var d : files) {
                offset = toBytes(arr, offset, d.compressed);
            }
//            try (var outputStream = Files.newOutputStream(Paths.get("nes.lzma"))) {
//                outputStream.write(arr, 0, offset);
//            }
            System.out.print("const char __in_flash() lz4source[] = {");
            for (int i = 0; i < size; ++i) {
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
            System.out.println("};");
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static int toBytes(byte[] arr, int offset, final List<LZMABlock> compressed) {
        for (final var c : compressed) {
            for (int i = 0; i < c.len; ++i) {
                arr[offset++] = c.block[i];
            }
        }
        return offset;
    }
    static void to3Bytes(byte[] arr, int t) {
        arr[0] = (byte)(t >> 16 & 0xFF);
        arr[1] = (byte)(t >> 8 & 0xFF);
        arr[2] = (byte)(t & 0xFF);
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
