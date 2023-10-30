import java.io.*;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

public class Dir2blks {

    record BTBlock(int btIdx, int bytes, int bytesTableId) {}
    record BTFileDescriptor(String name, byte[] orig, List<BTBlock> blocks) {}

    final static List<List<Byte>> bytesTables = new ArrayList<>(); //
    private static final int minSZ = 4;
    private static final int maxSZ = 32 + minSZ;         // 5 bits for 0..31 to code block size (0 == minSZ)
    private static final int maxBytesPerTable = 0x80000; // 19 bits to code offset (3 bytes to code one block) 0..0x7FFFF
    static BTBlock nextBlock(byte[] data, int dOff, int blocksOff, int bytesTableId) {
        int startIdx = -1;
        int len = 0;
        int i1 = dOff;
        final var bt = bytesTables.get(bytesTableId);
        for (int j0 = blocksOff; j0 < bt.size() && i1 < data.length; ++j0, ++i1) {
            final byte bj0 = bt.get(j0);
            if (data[i1] == bj0) {
                startIdx = j0;
                ++len;
                if (len == maxSZ)
                    break;
            }
            if (startIdx >= 0 && data[i1] != bj0) {
                break;
            }
        }
        if (startIdx == -1) {
            return null;
        } else {
            return new BTBlock(startIdx, len, bytesTableId);
        }
    }
    private static BTBlock createBtBlock(final byte[] data, final int i0, final int bytesTableId) {
        int bytes = 0;
        boolean switchBlock = false;
        final var bt = bytesTables.get(bytesTableId);
        final var startIdx = bt.size();
        for (int t = 0; (t < minSZ) && (i0 + t < data.length); ++t) {
            bytes += 1;
            if (bt.size() >= maxBytesPerTable) {
                switchBlock = true;
                break;
            }
            bt.add(data[i0 + t]);
        }
        if (switchBlock) {
            final var bbId = bytesTables.size();
            bytesTables.add(new ArrayList<>());
            return createBtBlock(data, i0, bbId);
        }
        return new BTBlock(startIdx, bytes, bytesTableId);
    }
    private static List<BTBlock> createBlocks(byte[] data, int bytesTableId) {
        final var bytesTable = bytesTables.get(bytesTableId);
        final List<BTBlock> btBlocks = new ArrayList<>();
        for (int i0 = 0; i0 < data.length; ) {
            // lookup in bytesTable
            var btBlock = nextBlock(data, i0, 0, bytesTableId);
            if (btBlock == null || btBlock.bytes < minSZ) {
                btBlock = createBtBlock(data, i0, bytesTableId);
                if (btBlock.bytesTableId != bytesTableId) {
                    System.out.println("// switch to next table id: " + btBlock.bytesTableId + "; magic switch code: 0xFFFFFF");
                    bytesTableId = btBlock.bytesTableId;
                }
            } else {
                for (int j0 = btBlock.btIdx + minSZ; j0 < bytesTable.size() && btBlock.bytes < maxSZ; ++j0) {
                    final var btBlockCandidate =  nextBlock(data, i0, j0, bytesTableId);
                    if (btBlockCandidate != null && btBlockCandidate.bytes > btBlock.bytes) {
                        btBlock = btBlockCandidate;
                    }
                }
            }
            btBlocks.add(btBlock);
            i0 += btBlock.bytes;
//            System.out.println("i0: " + i0 + "; btBlock.bytes: " + btBlock.bytes + "; btBlock.btIdx: " + btBlock.btIdx);
        }
        return btBlocks;
    }

    public static void main(final String[] args) {
        try {

            final List<BTFileDescriptor> btDescriptors = new ArrayList<>();

            final File folder = new File(args[0]);
            final File[] listOfFiles = folder.listFiles();
            assert listOfFiles != null;

            System.out.println("// Files:");
            int size = 4; // (int) number of files
            int maxFile = 0;
            final int blockSize = 16 * 1024;
            String firstFileName = null;
            byte[] firstFileData = null;
            bytesTables.add(new ArrayList<>());
            for (File file : listOfFiles) {
                if (file.isFile()) {
                    final String fn = file.getName();
                    if (firstFileName == null)
                        firstFileName = "\\NES\\" + fn;
                    final byte[] data = Files.readAllBytes(Paths.get(args[0] + "\\" + fn));

                    final var btBlocks = createBlocks(data, bytesTables.size() - 1);

                    if (firstFileData == null)
                        firstFileData = data;
                    if (maxFile < data.length)
                        maxFile = data.length;

                    final var btD = new BTFileDescriptor(fn, data, btBlocks);
                    btDescriptors.add(btD);
                    ;
                    System.out.println(
                            btD.name + "; sz: " + data.length + "b; blks: " + btD.blocks.size() +
                                    " (eff: " + btD.blocks.size() * 3 +
                                    "b); bytesTables.size(): " + bytesTables.size() +
                                    " (pwr: " + bytesTables.stream().mapToInt(List::size).sum() + "b)"
                    );
                }
            }

            int dataSum = 0;
            int blocksCnt = 0;
            for(final var btD: btDescriptors) {
                dataSum += btD.orig.length;
                blocksCnt += btD.blocks.size();
            }
            System.out.printf("dataSum: %d; bytesTables sz: %d; blocksCnt: %d\n", dataSum, bytesTables.size(), blocksCnt);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
